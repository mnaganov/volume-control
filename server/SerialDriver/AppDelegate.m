//
//  AppDelegate.m
//  SerialDriver
//
//  Created by Mikhail Naganov on 8/10/16.
//  Copyright © 2016 Mikhail Naganov. All rights reserved.
//

#import "AppDelegate.h"

#include "SerialThread.h"

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@property (strong, nonatomic) NSMenuItem *deviceStatusMenuItem;
@property (strong, nonatomic) NSMenuItem *volumeUpMenuItem;
@property (strong, nonatomic) NSMenuItem *volumeDownMenuItem;
@property (strong, nonatomic) NSMenu *statusMenu;
@property (strong, nonatomic) NSStatusItem *statusItem;
@property serial_thread_handle_t serial;
@property int pingResult;

@end

@implementation AppDelegate

- (id)init {
    self = [super init];
    if (self) {
        _serial = NULL;
        _pingResult = PING_RESULT_NOT_CONNECTED;
        init_serial_thread(&_serial);
    }
    return self;
}

void pingFinished(void *context, int result) {
    AppDelegate *ad = (__bridge AppDelegate*)context;
    ad.pingResult = result;
    [ad performSelectorOnMainThread:@selector(pingFinished:) withObject:nil waitUntilDone:FALSE];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    self.statusMenu = [[NSMenu alloc] initWithTitle:@""];
    _statusMenu.delegate = self;
    self.deviceStatusMenuItem =
    [[NSMenuItem alloc] initWithTitle:@"[Device Status]" action:nil keyEquivalent:@""];
    [_statusMenu addItem:_deviceStatusMenuItem];
    [_statusMenu addItem:[NSMenuItem separatorItem]];

    self.volumeUpMenuItem =
    [[NSMenuItem alloc] initWithTitle:@"Volume Up" action:@selector(volumeUp:) keyEquivalent:@""];
    [_statusMenu addItem:_volumeUpMenuItem];
    [_volumeUpMenuItem setEnabled:FALSE];

    self.volumeDownMenuItem =
    [[NSMenuItem alloc] initWithTitle:@"Volume Down" action:@selector(volumeDown:) keyEquivalent:@""];
    [_statusMenu addItem:_volumeDownMenuItem];
    [_volumeDownMenuItem setEnabled:FALSE];

    [_statusMenu addItem:[NSMenuItem separatorItem]];
    [_statusMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@""];

    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    _statusItem.image = [NSImage imageNamed:@"volume-control-16.png"];
    [_statusItem.image setTemplate:YES];
    [_statusItem setHighlightMode:YES];
    [_statusItem setMenu:_statusMenu];
}

- (void)pingFinished:(NSObject*)unused {
    bool enableControls = false;
    switch (_pingResult) {
        case PING_RESULT_NOT_CONNECTED:
            [_deviceStatusMenuItem setTitle:@"Device not connected"];
            break;
        case PING_RESULT_OK:
            [_deviceStatusMenuItem setTitle:@"Device is active"];
            enableControls = true;
            break;
        case PING_RESULT_COMMUNICATION_ERROR:
            [_deviceStatusMenuItem setTitle:@"Device not responding"];
            break;
    }
    [_volumeUpMenuItem setEnabled:enableControls];
    [_volumeDownMenuItem setEnabled:enableControls];
}

- (void)volumeUp:(NSObject*)unused {
    if (_serial != NULL) (*_serial)->volume_up(_serial);
}

- (void)volumeDown:(NSObject*)unused {
    if (_serial != NULL) (*_serial)->volume_down(_serial);
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    if (_serial != NULL) (*_serial)->shutdown(_serial);
    _serial = NULL;
}

- (void)menuWillOpen:(NSMenu *)menu {
    if (_serial != NULL) (*_serial)->start_ping(_serial, &pingFinished, (__bridge void *)(self));
}

- (void)menuDidClose:(NSMenu *)menu {
    if (_serial != NULL) (*_serial)->stop_ping(_serial);
}

@end
