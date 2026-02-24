#include "port/broadcast.h"
#import <Foundation/Foundation.h>
#import <Syphon/Syphon.h>
#import <OpenGL/gl3.h>

static SyphonServer* g_syphon_server = nil;
static NSString* g_sender_name = nil;

extern "C" {

static bool Syphon_Init(const char* sender_name) {
    @autoreleasepool {
        g_sender_name = sender_name ? [NSString stringWithUTF8String:sender_name] : @"3SX Broadcast";
        // Syphon server will be initialized on the first texture send or here if context is ready
        // We typically need an active CGLContext for Syphon
        return true;
    }
}

static void Syphon_Shutdown() {
    @autoreleasepool {
        if (g_syphon_server) {
            [g_syphon_server stop];
            g_syphon_server = nil;
        }
    }
}

static bool Syphon_SendTexture(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped) {
    @autoreleasepool {
        if (!g_syphon_server) {
            // Initialize server with current context
            CGLContextObj context = CGLGetCurrentContext();
            if (!context) return false;
            
            g_syphon_server = [[SyphonServer alloc] initWithName:g_sender_name
                                                      context:context
                                                      options:nil];
        }
        
        if (g_syphon_server) {
            NSSize size = NSMakeSize(width, height);
            [g_syphon_server publishFrameTexture:texture_id
                                   textureTarget:GL_TEXTURE_2D
                                     imageRegion:NSMakeRect(0, 0, width, height)
                               textureDimensions:size
                                         flipped:is_flipped];
            return true;
        }
        return false;
    }
}

static void Syphon_UpdateConfig(const BroadcastConfig* config) {
    if (config && !config->enabled) {
        Syphon_Shutdown();
    }
}

BroadcastPort g_broadcast_port_macos = {
    Syphon_Init,
    Syphon_Shutdown,
    Syphon_SendTexture,
    Syphon_UpdateConfig
};

} // extern "C"
