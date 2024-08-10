var net = require('net');

class APCBroker {
    constructor(host = "127.0.0.1", port = 3551, timeout = 10000, CMD_SEQUENCE = "\x00\x06status") {
        this.host = host;
        this.port = port;
        this.timeout = timeout;

        this.CMD_SEQUENCE = CMD_SEQUENCE;
        this.END_SEQUENCE = "\n\x00\x00";

        this.socket = new net.Socket();
        this.socket.setTimeout(timeout);
        this.connected = false;
        this.awaiting = false;
        this.data = "";
        this.queue = [];

        this.events = {};
        
        this.result = {
            status: 0,
            linev: 0,
            loadpct: 0,
            bcharge: 0,
            timeleft: 0,
            mbattchg: 0,
            mintimel: 0,
            outputv: 0,
            dwake: 0,
            dshutd: 0,
            lotrans: 0,
            hitrans: 0,
            retpct: 0,
            itemp: 0,
            battv: 0,
            linefreq: 0,
            numxfers: 0,
            tonbatt: 0,
            cumonbatt: 0
        };
    }

    get(type) {
        type = type.toLowerCase();
        this.queue.push(['status', 'values'].includes(type) ? type : 'values');
        this._get();
    }

    on(event, callback) {
        if(this.events[event] != null)
            this.events[event].push(callback);
        else
            this.events[event] = [callback];
    }

    _emit(event, ...args) {
        if(this.events[event] != null)
            for(var event of this.events[event])
                event(...args);
    }

    _connect() {
        this.socket.connect(this.port, this.host, () => this._onConnect());
    }

    _get() {
        if(!this.awaiting)
            if(!this.connected)
                this._connect();
    }

    _onConnect() {
        this.connected = true;
        this.socket.on('data', data => this._onData(data));
        this.socket.on('error', () => this._close());
        this.socket.on('timeout', () => this._close());
        this.socket.write(Buffer.from(this.CMD_SEQUENCE))
    }

    _onData(data) {
        this.awaiting = true;
        this.data += data.toString();
        if(this.data.endsWith(this.END_SEQUENCE)) {
            this.data = this.data.replace("\n\x00\x00", "");
            this._processData();
        }
    }

    _processData() {
        var data = this.data.replace(/[\x00-\x09\x0B\x0C\x0E-\x1F\x7F-\x9F%'%\r()]/g, '').split("\n"),
            result = this.result;
        if(this.queue[0] == 'values')
            for(var line of data) {
                var l = line.split(":");
                var key = l[0].replace(/\s+$/, '').replace(' ', '_').toLowerCase(),
                    val = l[1].slice(1);
                if(result[key] != null)
                    result[key] = key != "status" ? parseFloat(val.split(' ')[0]) : (val.replace(' ', '').toLowerCase() === "online" ? 1 : 0);
            }
        else
            for(var line of data) {
                var l = line.split(":");
                result[l[0].replace(/\s+$/, '').replace(' ', '_').toLowerCase()] = l[1].slice(1);
            }
        this._emit(this.queue[0], result);
        this.awaiting = false;
        this.queue.shift();
        this._close();
        if(this.queue.length > 0)
            this._get();
    }

    _close() {
        this.socket.destroy();
        this.connected = false;
    }
}

module.exports = APCBroker
