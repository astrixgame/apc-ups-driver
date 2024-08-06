/*
 * AppController.m
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

#import "AppController.h"
#import "statmgr.h"

//******************************************************************************
// CLASS AppController
//******************************************************************************
@implementation AppController

- (void)awakeFromNib
{
   // Load images
   NSBundle *bundle = [NSBundle mainBundle];
   chargingImage = [[NSImage alloc] initWithContentsOfFile:
      [bundle pathForResource:@"charging" ofType:@"png"]];
   commlostImage = [[NSImage alloc] initWithContentsOfFile:
      [bundle pathForResource:@"commlost" ofType:@"png"]];
   onbattImage = [[NSImage alloc] initWithContentsOfFile:
      [bundle pathForResource:@"onbatt" ofType:@"png"]];
   onlineImage = [[NSImage alloc] initWithContentsOfFile:
      [bundle pathForResource:@"online" ofType:@"png"]];

   // Create our status item
   statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:
                  NSVariableStatusItemLength] retain];
   [statusItem setHighlightMode:YES];
   [statusItem setImage:commlostImage];
   [statusItem setMenu:statusMenu];

   // Setup status table control
   statusDataSource = [[StatusTableDataSource alloc] init];
   [statusGrid setDataSource:statusDataSource];

   // Setup events table control
   eventsDataSource = [[EventsTableDataSource alloc] init];
   [eventsGrid setDataSource:eventsDataSource];

   // Configure to post notifications
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
   haveNotifCtr = NSClassFromString(@"NSUserNotificationCenter") != nil;
   if (haveNotifCtr)
   {
      // Allow user to enable/disable notifications
      [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];
      [configPopups setEnabled:YES];
      [configPopups setHidden:NO];
   }
#else
   haveNotifCtr = NO;
#endif
}

- (void)activateWithConfig:(InstanceConfig*)cfg manager:(InstanceManager*)mgr
{
   manager = [mgr retain];

   // Save copies of preferences so worker can tell what changed
   configMutex = [[NSLock alloc] init];
   config = [cfg retain];
   prevHost = [[config host] retain];
   prevPort = [config port];
   prevRefresh = [config refresh];
   statmgr = NULL;
   lastStatus = [@"" retain];

   // Create timer object used by the thread
   timer = [[NSTimer timerWithTimeInterval:[config refresh] target:self 
            selector:@selector(timerHandler:) userInfo:nil repeats:YES] retain];

   // Start worker thread to handle polling UPS for status
   running = YES;
   condLock = [[NSConditionLock alloc] initWithCondition:1];
   [NSThread detachNewThreadSelector:@selector(thread:) 
                                     toTarget:self 
                                     withObject:nil];
}

- (void) thread:(id)arg
{
   NSAutoreleasePool *p = [[NSAutoreleasePool alloc] init];

   // Hold this lock while we're running to prevent release of the object
   [condLock lockWhenCondition:1];

   // Publicise our CFRunLoop reference so main thread can stop us
   runLoop = [[NSRunLoop currentRunLoop] getCFRunLoop];

   // Add periodic timer to the runloop
   [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];

   // Fire timer once immediately to get the first data sample ASAP
   [timer fire];

   // Invoke the runloop until we're asked to exit
   while (running)
   {
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode 
                                  beforeDate:[NSDate distantFuture]];
   }

   // All done...clean up
   [timer invalidate];
   [timer release];
   delete statmgr;
   [p release];

   // Signal that we've completed
   [condLock unlockWithCondition:0];
}

- (void)timerHandler:(NSTimer*)theTimer
{
   // Reinitialize the StatMgr if host or port setting changes
   [configMutex lock];
   if (!statmgr || 
       ![prevHost isEqualToString:[config host]] || 
       prevPort != [config port])
   {
      [statusItem setImage:commlostImage];
      delete statmgr;
      statmgr = new StatMgr([[config host] UTF8String], [config port]);
      [prevHost release];
      prevHost = [[config host] retain];
      prevPort = [config port];
   }
   BOOL doPopup = [config popups];
   [configMutex unlock];

   // Grab updated status info from apcupsd
   int battstat;
   astring statstr, upsname;
   statmgr->GetSummary(battstat, statstr, upsname);

   // Update icon based on UPS state
   if (battstat == -1)
      [statusItem setImage:commlostImage];
   else if (battstat == 0)
      [statusItem setImage:onbattImage];
   else if (battstat >= 100)
      [statusItem setImage:onlineImage];
   else
      [statusItem setImage:chargingImage];   

   // Update tooltip with status and UPS name
   NSString *tooltip;
   if (upsname == "UPS_IDEN" || upsname.empty())
      tooltip = [NSString stringWithUTF8String:statstr];
   else
      tooltip = [NSString stringWithFormat:
                 @"%s: %s", upsname.str(), statstr.str()];
   [statusItem setToolTip:tooltip];

   // Update menu with UPS name and host
   if (upsname.empty())
      upsname = "<unknown>";
   [upsName setTitle:[NSString stringWithFormat:@"UPS: %s",upsname.str()]];
   [upsHost setTitle:[NSString stringWithFormat:@"HOST: %@:%d",[config host],[config port]]];

   // Update status window, but only if it's visible (optimization)
   if ([statusWindow isVisible])
   {
      // Set window title to include UPS name
      [statusWindow setTitle:
         [NSString stringWithFormat:@"Status for UPS: %s",upsname.str()]];

      // Update raw status table
      alist<astring> keys, values;
      statmgr->GetAll(keys, values);
      [statusDataSource populate:keys values:values];
      [statusGrid reloadData];

      // Update status text
      [statusText setStringValue:[NSString stringWithUTF8String:statstr]];

      // Update runtime
      NSString *tmp = [NSString stringWithUTF8String:statmgr->Get("TIMELEFT")];
      tmp = [[tmp componentsSeparatedByString:@" "] objectAtIndex:0];
      [statusRuntime setStringValue:tmp];

      // Update battery
      tmp = [NSString stringWithUTF8String:statmgr->Get("BCHARGE")];
      tmp = [[tmp componentsSeparatedByString:@" "] objectAtIndex:0];
      [statusBatteryText setStringValue:[tmp stringByAppendingString:@"%"]];
      [statusBatteryBar setIntValue:[tmp intValue]];

      // Update load
      tmp = [NSString stringWithUTF8String:statmgr->Get("LOADPCT")];
      tmp = [[tmp componentsSeparatedByString:@" "] objectAtIndex:0];
      [statusLoadText setStringValue:[tmp stringByAppendingString:@"%"]];
      [statusLoadBar setIntValue:[tmp intValue]];
   }

   // Update events window, but only if it's visible (optimization)
   if ([eventsWindow isVisible])
   {
      // Set window title to include UPS name
      [eventsWindow setTitle:
         [NSString stringWithFormat:@"Events for UPS: %s",upsname.str()]];

      // Fetch current events from the UPS
      alist<astring> eventStrings;
      statmgr->GetEvents(eventStrings);
      [eventsDataSource populate:eventStrings];
      [eventsGrid reloadData];
   }

   // If status has changed, display a popup window
   NSString *newStatus = [NSString stringWithUTF8String:statstr];
   if (haveNotifCtr && doPopup && [lastStatus length] && 
       ![lastStatus isEqualToString:newStatus])
   {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
      // Post notification here...
      NSUserNotification *notification = [[NSUserNotification alloc] init];
      notification.title = @"Apcupsd Event";
      notification.subtitle = 
         [NSString stringWithFormat:@"%s %@",upsname.str(),newStatus];
      NSDate *now = [NSDate dateWithTimeIntervalSinceNow:0];
      notification.informativeText = 
         [NSString stringWithFormat:@"UPS '%s' status %@ at %@",
          upsname.str(),newStatus,now];
      notification.contentImage = [statusItem image];
      [[NSUserNotificationCenter defaultUserNotificationCenter] 
         deliverNotification:notification];
      [notification release];
#endif
   }

   // Save status for comparison next time
   [lastStatus release];
   lastStatus = [newStatus retain];

   // If refresh interval changed, invalidate old timer and start a new one
   [configMutex lock];
   if (prevRefresh != [config refresh])
   {
      prevRefresh = [config refresh];
      [theTimer invalidate];
      [theTimer release];
      timer = [[NSTimer timerWithTimeInterval:[config refresh] target:self 
               selector:@selector(timerHandler:) userInfo:nil repeats:YES] retain];
      [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
   }
   [configMutex unlock];
}

- (void) dealloc {
   NSLog(@"%s:%d", __FUNCTION__, __LINE__);
   [chargingImage release];
   [commlostImage release];
   [onlineImage release];
   [onbattImage release];

   [prevHost release];
   [lastStatus release];

   [statusDataSource release];
   [eventsDataSource release];
   [configMutex release];
   [condLock release];
   [manager release];
   [statusItem release];
   [super dealloc];
}

-(IBAction)config:(id)sender;
{
   // Copy current settings into config window
   [configHost setStringValue:[config host]];
   [configPort setIntValue:[config port]];
   [configRefresh setIntValue:[config refresh]];
   [configPopups setIntValue:[config popups]];

   // Force app to foreground and move key focus to config window
   [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
   [configWindow makeKeyAndOrderFront:self];
}

-(IBAction)configComplete:(id)sender;
{
   // Validate fields are not empty
   NSString *tmpstr = [configHost stringValue];
   tmpstr = [tmpstr stringByTrimmingCharactersInSet:
               [NSCharacterSet whitespaceCharacterSet]];
   if ([tmpstr length] == 0)
   {
      [configWindow makeFirstResponder:configHost];
   }
   else if ([[configPort stringValue] length] == 0)
   {
      [configWindow makeFirstResponder:configPort];
   }
   else if ([[configRefresh stringValue] length] == 0)
   {
      [configWindow makeFirstResponder:configRefresh];
   }
   else
   {
      // Grab new settings from window controls
      [configMutex lock];
      [config setHost:[configHost stringValue]];
      [config setPort:[configPort intValue]];
      [config setRefresh:[configRefresh intValue]];
      [config setPopups:[configPopups intValue]];

      [config save];
      [configMutex unlock];

      // Hide window
      [configWindow orderOut:self];

      // Kick runloop timer so new config will be acted on immediately
      [timer fire];
   }
}

-(IBAction)status:(id)sender
{
   // Force app to foreground and move key focus to status window
   [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
   [statusWindow makeKeyAndOrderFront:self];

   // Kick timer so window is updated immediately
   [timer fire];
}

-(IBAction)events:(id)sender
{
   // Force app to foreground and move key focus to events window
   [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
   [eventsWindow makeKeyAndOrderFront:self];

   // Kick timer so window is updated immediately
   [timer fire];
}

-(IBAction)about:(id)sender;
{
   // Normally we'd just wire the about button directly to NSApp 
   // orderFrontStandardAboutPanel. However, since we're a status item without
   // a main window we need to bring ourself to the foreground or else the
   // about box will be buried behind whatever window the user has active at
   // the time. So we'll force ourself active, then call out to NSApp.
   [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
   [[NSApplication sharedApplication] orderFrontStandardAboutPanel:self];
}

-(NSString *)id
{
   return [config id];
}

-(void)close
{
   NSLog(@"%s:%d", __FUNCTION__, __LINE__);

   // Stop the runloop and wait for it to complete
   // Using a lock to wait for thread termination is silly, but Apple's 
   // lousy threading API gives us no 'join'.
   running = NO;
   CFRunLoopStop(runLoop);
   [condLock lockWhenCondition:0];
   [condLock unlockWithCondition:0];

   // Remove the icon from the status bar
   [[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
   [startAtLogin setState:([manager isStartAtLogin] ? NSOnState : NSOffState)];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center 
   shouldPresentNotification:(NSUserNotification *)notification
{
    return YES;
}
#endif

@end

//******************************************************************************
// CLASS StatusTableDataSource
//******************************************************************************
@implementation StatusTableDataSource

- (id) init
{
   if ((self = [super init]))
   {
      _mutex = [[NSLock alloc] init];
      _keys = [[NSMutableArray alloc] init];
      _values = [[NSMutableArray alloc] init];
   }
   return self;
}

- (void) dealloc
{
   [_keys release];
   [_values release];
   [_mutex release];
   [super dealloc];
}

- (void)populate:(alist<astring> &)keys values:(alist<astring> &)values
{
   [_mutex lock];
   [_keys removeAllObjects];
   [_values removeAllObjects];

   alist<astring>::const_iterator iter;
   for (iter = keys.begin(); iter != keys.end(); ++iter)
      [_keys addObject:[NSString stringWithUTF8String:*iter]];
   for (iter = values.begin(); iter != values.end(); ++iter)
      [_values addObject:[NSString stringWithUTF8String:*iter]];

   [_mutex unlock];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
   [_mutex lock];
   int count = [_keys count];
   [_mutex unlock];
   return count;
}

- (id)tableView:(NSTableView *)aTableView 
   objectValueForTableColumn:(NSTableColumn *)aTableColumn 
   row:(int)rowIndex
{
   NSString *ret;

   // Lookup and retain value under lock
   [_mutex lock];
   if ([[aTableColumn identifier] intValue] == 0)
      ret = [[[_keys objectAtIndex:rowIndex] retain] autorelease];
   else
      ret = [[[_values objectAtIndex:rowIndex] retain] autorelease];
   [_mutex unlock];

   return ret;
}

@end

//******************************************************************************
// CLASS EventsTableDataSource
//******************************************************************************
@implementation EventsTableDataSource

- (id) init
{
   if ((self = [super init]))
   {
      mutex = [[NSLock alloc] init];
      strings = [[NSMutableArray alloc] init];
   }
   return self;
}

- (void) dealloc
{
   [strings release];
   [mutex release];
   [super dealloc];
}

- (void)populate:(alist<astring> &)stats
{
   [mutex lock];
   [strings removeAllObjects];

   alist<astring>::const_iterator iter;
   for (iter = stats.begin(); iter != stats.end(); ++iter)
   {
      NSString *data = [NSString stringWithUTF8String:*iter];
      [strings addObject:data];
   }
   [mutex unlock];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
   [mutex lock];
   int count = [strings count];
   [mutex unlock];
   return count;
}

- (id)tableView:(NSTableView *)aTableView 
   objectValueForTableColumn:(NSTableColumn *)aTableColumn 
   row:(int)rowIndex
{
   NSString *ret;

   // Lookup and retain value under lock
   [mutex lock];
   ret = [[[strings objectAtIndex:rowIndex] retain] autorelease];
   [mutex unlock];

   return ret;
}

@end
