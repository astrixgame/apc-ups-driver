var net = require('net');

var client = new net.Socket();
client.connect(3551, '127.0.0.1', function() {
	console.log('Connected');
	client.write('\x00\x06events');
});

client.on('data', function(data) {
	console.log('Received: ' + data);
	
});

client.on('close', function() {
	console.log('Connection closed');
});