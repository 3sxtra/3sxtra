/**
 * @file broadcast_pipewire.cpp
 * @brief PipeWire video broadcast backend (Linux).
 *
 * Provides the Broadcast API implementation for Linux using PipeWire
 * and shared memory buffers via glReadPixels.
 */
#include "port/broadcast.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pipewire/pipewire.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/result.h>

#include <glad/gl.h>

#define MAX_STREAM_NAME 128

// Internal PipeWire context structures
static pw_thread_loop* s_thread_loop = NULL;
static pw_context* s_context = NULL;
static pw_core* s_core = NULL;
static pw_stream* s_stream = NULL;

static bool s_initialized = false;
static char s_sender_name[MAX_STREAM_NAME] = "3SX Broadcast";

static uint32_t s_current_width = 0;
static uint32_t s_current_height = 0;

// Temporary FBO for glReadPixels
static GLuint s_fbo = 0;

static void on_process(void* data) {
    // PipeWire stream callback indicating we should provide a buffer.
    // In our push-based design (Broadcast_Send is called per-frame by the game loop),
    // we don't strictly generate frames inside this callback.
    // Instead, we just let it run. The actual enqueue happens in PipeWire_SendTexture.
    // However, if PipeWire demands a frame and we have none, we could send an empty one.
}

static const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void configure_stream(uint32_t width, uint32_t height) {
    if (!s_stream)
        return;

    pw_thread_loop_lock(s_thread_loop);

    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(NULL, 0);
    uint8_t buffer[1024];
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    const struct spa_pod* params[1];

    // Configure for standard 8-bit RGBA (OpenGL's GL_RGBA)
    struct spa_video_info_raw raw_info = SPA_VIDEO_INFO_RAW_INIT(.format = SPA_VIDEO_FORMAT_RGBA,
                                                                 .size = SPA_RECTANGLE(width, height),
                                                                 .framerate = SPA_FRACTION(60000, 1001));
    params[0] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &raw_info);

    pw_stream_update_params(s_stream, params, 1);

    s_current_width = width;
    s_current_height = height;

    pw_thread_loop_unlock(s_thread_loop);
}

extern "C" {

static bool PipeWire_Init(const char* sender_name) {
    if (s_initialized)
        return true;

    if (sender_name) {
        snprintf(s_sender_name, sizeof(s_sender_name), "%s", sender_name);
    }

    pw_init(NULL, NULL);

    s_thread_loop = pw_thread_loop_new("PipeWire Thread", NULL);
    if (!s_thread_loop) {
        fprintf(stderr, "[PipeWire] Failed to create thread loop\n");
        return false;
    }

    s_context = pw_context_new(pw_thread_loop_get_loop(s_thread_loop), NULL, 0);
    if (!s_context) {
        fprintf(stderr, "[PipeWire] Failed to create context\n");
        pw_thread_loop_destroy(s_thread_loop);
        return false;
    }

    if (pw_thread_loop_start(s_thread_loop) < 0) {
        fprintf(stderr, "[PipeWire] Failed to start thread loop\n");
        pw_context_destroy(s_context);
        pw_thread_loop_destroy(s_thread_loop);
        return false;
    }

    pw_thread_loop_lock(s_thread_loop);
    s_core = pw_context_connect(s_context, NULL, 0);
    if (!s_core) {
        fprintf(stderr, "[PipeWire] Failed to connect core\n");
        pw_thread_loop_unlock(s_thread_loop);
        return false;
    }

    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE,
                                             "Video",
                                             PW_KEY_MEDIA_CATEGORY,
                                             "Capture",
                                             PW_KEY_MEDIA_ROLE,
                                             "Screen",
                                             PW_KEY_NODE_NAME,
                                             s_sender_name,
                                             PW_KEY_NODE_DESCRIPTION,
                                             s_sender_name,
                                             NULL);

    s_stream = pw_stream_new(s_core, s_sender_name, props);
    pw_stream_add_listener(s_stream, NULL, &stream_events, NULL);

    // Initial connect with generic format
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod* params[1];

    struct spa_video_info_raw init_info = SPA_VIDEO_INFO_RAW_INIT(.format = SPA_VIDEO_FORMAT_RGBA,
                                                                  .size = SPA_RECTANGLE(640, 480),
                                                                  .framerate = SPA_FRACTION(60000, 1001));
    params[0] = spa_format_video_raw_build(&b, SPA_PARAM_EnumFormat, &init_info);

    int res = pw_stream_connect(
        s_stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_ALLOC_BUFFERS),
        params,
        1);

    pw_thread_loop_unlock(s_thread_loop);

    if (res < 0) {
        fprintf(stderr, "[PipeWire] Failed to connect stream: %s\n", spa_strerror(res));
        return false;
    }

    s_initialized = true;
    return true;
}

static void PipeWire_Shutdown() {
    if (!s_initialized)
        return;

    if (s_thread_loop)
        pw_thread_loop_lock(s_thread_loop);

    if (s_stream) {
        pw_stream_disconnect(s_stream);
        pw_stream_destroy(s_stream);
        s_stream = NULL;
    }

    if (s_core) {
        pw_core_disconnect(s_core);
        s_core = NULL;
    }

    if (s_thread_loop)
        pw_thread_loop_unlock(s_thread_loop);

    if (s_thread_loop) {
        pw_thread_loop_stop(s_thread_loop);
    }

    if (s_context) {
        pw_context_destroy(s_context);
        s_context = NULL;
    }

    if (s_thread_loop) {
        pw_thread_loop_destroy(s_thread_loop);
        s_thread_loop = NULL;
    }

    pw_deinit();

    if (s_fbo) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }

    s_initialized = false;
}

static bool PipeWire_SendTexture(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped) {
    if (!s_initialized || !s_stream)
        return false;

    // Update params if stream size changed
    if (width != s_current_width || height != s_current_height) {
        configure_stream(width, height);
    }

    pw_thread_loop_lock(s_thread_loop);
    struct pw_buffer* b = pw_stream_dequeue_buffer(s_stream);
    if (!b) {
        pw_thread_loop_unlock(s_thread_loop);
        return false;
    }

    struct spa_buffer* buf = b->buffer;
    if (buf->datas[0].data == NULL) {
        pw_stream_queue_buffer(s_stream, b);
        pw_thread_loop_unlock(s_thread_loop);
        return false;
    }

    // Read OpenGL texture into PipeWire's shared memory buffer
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (s_fbo == 0) {
        glGenFramebuffers(1, &s_fbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

    void* dest = buf->datas[0].data;
    uint32_t stride = SPA_ROUND_UP_N(width * 4, 4); // RGBA is 4 bytes per pixel

    // GL usually reads bottom-up. PipeWire expects top-down by default for SPA_VIDEO_FORMAT_RGBA.
    // However, if the `is_flipped` flag is true, that implies the texture is already bottom-up and needs to be left as
    // is, or vice versa depending on rendering backend. In standard 3SX, the OpenGL FBOs are bottom-up, and
    // is_flipped=false forces the sender to flip it. We handle row-by-row reading if we need to flip it, or bulk read
    // if not. Let's do a simple bulk read and handle flipping manually if needed.

    // We bind a pack alignment of 1 to be safe
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    if (is_flipped) {
        // Just read it straight; receiver expects it like this
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dest);
    } else {
        // Need to flip Y for top-down
        for (uint32_t y = 0; y < height; y++) {
            uint8_t* row_dest = (uint8_t*)dest + ((height - 1 - y) * stride);
            glReadPixels(0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, row_dest);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->size = height * stride;
    buf->datas[0].chunk->stride = stride;

    pw_stream_queue_buffer(s_stream, b);
    pw_thread_loop_unlock(s_thread_loop);

    return true;
}

static void PipeWire_UpdateConfig(const BroadcastConfig* config) {
    if (config && !config->enabled) {
        PipeWire_Shutdown();
    }
}

BroadcastPort g_broadcast_port_linux = {
    PipeWire_Init, PipeWire_Shutdown, PipeWire_SendTexture, PipeWire_UpdateConfig
};

} // extern "C"
