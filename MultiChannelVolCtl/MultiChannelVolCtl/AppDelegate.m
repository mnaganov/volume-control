//
//  AppDelegate.m
//  MultiChannelVolCtl
//
//  Created by Mikhail Naganov on 6/28/17.
//  Copyright © 2017 Mikhail Naganov. All rights reserved.
//

#import <CoreAudio/AudioHardware.h>
#import <CoreAudio/CoreAudio.h>

#import "AppDelegate.h"

#define LEFT_CHANNEL 1
#define RIGHT_CHANNEL (LEFT_CHANNEL + 1)
#define FIRST_MULTICHANNEL (RIGHT_CHANNEL + 1)

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    registerVolumeListener();
    registerOutputDeviceListener();
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
}

static NSString *NSStringFromOSStatus(OSStatus errCode) {
    if (errCode == noErr)
        return @"noErr";
    char message[5] = {0};
    *(UInt32*) message = CFSwapInt32HostToBig(errCode);
    return [NSString stringWithCString:message encoding:NSASCIIStringEncoding];
}

static OSStatus syncVolumeLevel(AudioDeviceID deviceID,
                                const AudioObjectPropertyAddress* srcChannelVolume,
                                AudioObjectPropertyElement startDstChannel,
                                AudioObjectPropertyElement endDstChannel) {
    Float32 srcVolume = 0;
    UInt32 dataSize = sizeof(Float32);
    OSStatus err = AudioObjectGetPropertyData(deviceID,
                                              srcChannelVolume,
                                              0,
                                              NULL,
                                              &dataSize,
                                              &srcVolume);
    if (err != noErr) {
        NSLog(@"Getting current volume: %@", NSStringFromOSStatus(err));
        return err;
    }
    
    for (AudioObjectPropertyElement elt = startDstChannel; elt < endDstChannel; ++elt) {
        AudioObjectPropertyAddress channelVolume = {
            srcChannelVolume->mSelector,
            srcChannelVolume->mScope,
            elt };
        err = AudioObjectSetPropertyData(deviceID,
                                         &channelVolume,
                                         0,
                                         NULL,
                                         dataSize,
                                         &srcVolume);
        if (err != noErr) {
            NSLog(@"Setting volume for channel %d: %@", elt, NSStringFromOSStatus(err));
            return err;
        }
    }
    
    return err;
}

static OSStatus onVolumeChange(AudioObjectID inObjectID,
                               UInt32 inNumberAddresses,
                               const AudioObjectPropertyAddress* inAddresses,
                               void* inClientData) {
    UInt32 channelEnd = (UInt32)inClientData;
    if (channelEnd <= RIGHT_CHANNEL) return noErr;

    for (UInt32 i = 0; i < inNumberAddresses; ++i) {
        if (inAddresses[i].mElement == LEFT_CHANNEL) {
            syncVolumeLevel(inObjectID, &inAddresses[i], FIRST_MULTICHANNEL, channelEnd);
            break;
        }
    }
    return noErr;
}

static OSStatus onDeviceChange(AudioObjectID inObjectID,
                               UInt32 inNumberAddresses,
                               const AudioObjectPropertyAddress* inAddresses,
                               void* inClientData) {
    registerVolumeListener();
    return noErr;
}

static AudioDeviceID GetDefaultOutputDevice() {
    AudioDeviceID deviceID = 0;
    UInt32 dataSize = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress defaultOutputDevice = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                              &defaultOutputDevice,
                                              0,
                                              NULL,
                                              &dataSize,
                                              &deviceID);
    if (err != noErr) {
        NSLog(@"GetDefaultOutputDevice: %@", NSStringFromOSStatus(err));
    }
    
    return deviceID;
}

static UInt32 GetOutputChannelCount(AudioDeviceID deviceID) {
    UInt32 channelCount = 0;
    UInt32 dataSize = 0;
    AudioObjectPropertyAddress streamConfig = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput };
    OSStatus err = AudioObjectGetPropertyDataSize(deviceID,
                                                       &streamConfig,
                                                       0,
                                                       NULL,
                                                       &dataSize);
    if (err != noErr) {
        NSLog(@"getting the size of stream configuration: %@", NSStringFromOSStatus(err));
        return 0;
    }
    
    AudioBufferList *bufferList = (AudioBufferList *) malloc(dataSize);
    err = AudioObjectGetPropertyData(deviceID,
                                     &streamConfig,
                                     0,
                                     NULL,
                                     &dataSize,
                                     bufferList);
    if (err == noErr) {
        for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
            channelCount += bufferList->mBuffers[i].mNumberChannels;
        }
    } else {
        NSLog(@"getting the stream configuration: %@", NSStringFromOSStatus(err));
    }
    free(bufferList);
    
    return channelCount;
}

static void registerVolumeListener() {
    AudioDeviceID outputDeviceID = GetDefaultOutputDevice();
    if (outputDeviceID == 0) return;
    
    UInt32 channelCount = GetOutputChannelCount(outputDeviceID);
    NSLog(@"output device has %u channels", channelCount);
    if (channelCount <= RIGHT_CHANNEL) return;
    
    AudioObjectPropertyAddress channelVolume = {
        kAudioDevicePropertyVolumeScalar,
        kAudioDevicePropertyScopeOutput,
        LEFT_CHANNEL };
    OSStatus err = AudioObjectAddPropertyListener(outputDeviceID,
                                                  &channelVolume,
                                                  onVolumeChange,
                                                  (void*)(channelCount + 1));
    if (err != noErr) {
        NSLog(@"registerVolumeListener: %@", NSStringFromOSStatus(err));
    }
}

static void registerOutputDeviceListener() {
    AudioObjectPropertyAddress theAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };
    OSStatus err = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                                       &theAddress,
                                                       onDeviceChange,
                                                       NULL);
    if (err != noErr) {
        NSLog(@"registerOutputDeviceListener: %@", NSStringFromOSStatus(err));
    }
}

@end
