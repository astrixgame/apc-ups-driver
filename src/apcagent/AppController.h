/*
 * AppController.h
 *
 * Apcupsd monitoring applet for Mac OS X
 */

/*
 * Copyright (C) 2009 Adam Kropelin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1335, USA.
 */

#import <Cocoa/Cocoa.h>
#import "alist.h"
#import "astring.h"
#import "InstanceManager.h"
#import "InstanceConfig.h"

class StatMgr;

// Prior to 10.6, NSTableViewDataSource was an informal protocol
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1060
@protocol NSTableViewDataSource <NSObject> @end
#endif 

// NSUserNotificationCenterDelegate was added in 10.8
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1080
@protocol NSUserNotificationCenterDelegate <NSObject> @end
#endif

// NSMenuDelegate was added in 10.6
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1060
@protocol NSMenuDelegate <NSObject> @end
#endif

@interface StatusTableDataSource: NSObject <NSTableViewDataSource>
{
   NSLock *_mutex;
   NSMutableArray *_keys;
   NSMutableArray *_values;
}

- (id) init;
- (void)populate:(alist<astring> &)keys values:(alist<astring> &)values;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView 
   objectValueForTableColumn:(NSTableColumn *)aTableColumn 
   row:(int)rowIndex;
@end

@interface EventsTableDataSource: NSObject <NSTableViewDataSource>
{
   NSLock *mutex;
   NSMutableArray *strings;
}

- (id) init;
- (void)populate:(alist<astring> &)stats;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView 
   objectValueForTableColumn:(NSTableColumn *)aTableColumn 
   row:(int)rowIndex;
@end

@interface AppController: NSObject <NSMenuDelegate, NSUserNotificationCenterDelegate>
{
   // Status item shown in the status bar
   NSStatusItem *statusItem;

   // The menu attached to the status item
   IBOutlet NSMenu *statusMenu;
   IBOutlet NSMenuItem *upsName;
   IBOutlet NSMenuItem *upsHost;
   IBOutlet NSMenuItem *startAtLogin;

   // The config window and its controls
   IBOutlet NSWindow *configWindow;
   IBOutlet NSTextField *configHost;
   IBOutlet NSTextField *configPort;
   IBOutlet NSTextField *configRefresh;
   IBOutlet NSTextField *growlLabel;
   IBOutlet NSButton *configPopups;

   // The status window and its controls
   IBOutlet NSWindow *statusWindow;
   IBOutlet NSTextField *statusText;
   IBOutlet NSTextField *statusRuntime;
   IBOutlet NSLevelIndicator *statusBatteryBar;
   IBOutlet NSTextField *statusBatteryText;
   IBOutlet NSTableView *statusGrid;
   IBOutlet NSLevelIndicator *statusLoadBar;
   IBOutlet NSTextField *statusLoadText;

   // The events window and its controls
   IBOutlet NSWindow *eventsWindow;
   IBOutlet NSTableView *eventsGrid;

   // Icon images
   NSImage      *commlostImage;
   NSImage      *chargingImage;
   NSImage      *onbattImage;
   NSImage      *onlineImage;

   // User-configurable settings and a lock to protect them
   NSLock *configMutex;
   InstanceConfig *config;
   InstanceManager *manager;

   // Copy of settings used to detect changes
   NSString *prevHost;
   int prevPort;
   int prevRefresh;

   // Full set of status strings from the UPS, used to provide 
   // data for status window grid control
   StatusTableDataSource *statusDataSource;

   // Full set of events strings from the UPS, used to provide 
   // data for events window grid control plus a flag indicating if
   // the polling loop should update it.
   EventsTableDataSource *eventsDataSource;
   bool updateEvents;

   // Previous status, used to detect changes
   NSString *lastStatus;

   // Timer for UPS polling
   NSTimer *timer;

   // C++ object which handles polling the UPS
   StatMgr *statmgr;

   // If system support Notification Center (10.8 and above)
   BOOL haveNotifCtr;

   // Runloop variables
   BOOL running;
   CFRunLoopRef runLoop;
   NSConditionLock *condLock;
}

-(IBAction)config:(id)sender;
-(IBAction)configComplete:(id)sender;

-(IBAction)status:(id)sender;
-(IBAction)events:(id)sender;
-(IBAction)about:(id)sender;

-(NSString *)id;
-(void)close;

- (void)timerHandler:(NSTimer*)theTimer;

- (void)activateWithConfig:(InstanceConfig*)cfg manager:(InstanceManager*)mgr;

- (void)menuNeedsUpdate:(NSMenu *)menu;

@end
