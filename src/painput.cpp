#include "painput.h"

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

#include <SDL_audio.h>
#include <SDL_log.h>

#include <cmath>

using namespace groggle;

// We're dealing with C API here, just accept global variables...
static enum class Purpose {
    RECORD,
    QUERY_SINKS
} purpose;
static pa_mainloop *loop;

static void pa_stream_notify_cb(pa_stream *stream, void* /*userdata*/)
{
    const pa_stream_state state = pa_stream_get_state(stream);
    switch (state) {
    case PA_STREAM_FAILED:
        SDL_Log("Stream state: %i (failed)", state);
        break;
    case PA_STREAM_READY:
        break;
    }
}

static void pa_stream_read_cb(pa_stream *stream, const size_t /*nbytes*/, void *userdata)
{
    // Careful when to pa_stream_peek() and pa_stream_drop()!
    // c.f. https://www.freedesktop.org/software/pulseaudio/doxygen/stream_8h.html#ac2838c449cde56e169224d7fe3d00824
    uint8_t *samples = nullptr;
    size_t actualbytes = 0;
    if (pa_stream_peek(stream, (const void**)&samples, &actualbytes) != 0) {
        SDL_Log("Failed to peek at stream data");
        return;
    }

    if (samples == nullptr && actualbytes == 0) {
        // No data in the buffer, ignore.
        return;
    } else if (samples == nullptr && actualbytes > 0) {
        // Hole in the buffer. We must drop it.
        if (pa_stream_drop(stream) != 0) {
            SDL_Log("Failed to drop a hole! (Sounds weird, doesn't it?)");
            return;
        }
    }

    // Process data
    //SDL_Log(">> %i bytes", actualbytes);
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userdata);
    {
        // TODO What if we're getting too few bytes to fill the buffer?
        std::lock_guard<std::mutex>(meta->mutex);
        memcpy(meta->data, samples, std::min(actualbytes, (size_t)meta->fileSpec.samples * 2));
        meta->dataSize = actualbytes;
        meta->position = 0;
    }

    if (pa_stream_drop(stream) != 0) {
        SDL_Log("Failed to drop data after peeking.");
    }
}

static void pa_server_info_cb(pa_context *ctx, const pa_server_info *info, void *userdata)
{
    //SDL_Log("Default sink: %s", info->default_sink_name);

    const uint16_t BUFFER_SAMPLES = 1024; // Buffer size in samples
    const uint8_t CHANNELS = 1;
    const uint32_t RATE = 44100;

    // Set up stream parameters
    pa_sample_spec paSpec;
    paSpec.channels = CHANNELS;
    paSpec.format = PA_SAMPLE_S16LE;
    paSpec.rate = RATE;
    pa_stream *stream = pa_stream_new(ctx, "output monitor", &paSpec, nullptr);

    // Note stream parameters for later use
    // TODO Have SDL spec pre-filled by caller
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userdata);
    SDL_AudioSpec sdlSpec;
    SDL_zero(sdlSpec);
    sdlSpec.channels = CHANNELS;
    sdlSpec.format = AUDIO_S16LSB;
    sdlSpec.samples = BUFFER_SAMPLES;
    sdlSpec.freq = RATE;
    meta->fileSpec = sdlSpec;
    meta->duration = 0; // infinity
    meta->data = new uint8_t[BUFFER_SAMPLES * 2];

    pa_stream_set_state_callback(stream, &pa_stream_notify_cb, userdata);
    pa_stream_set_read_callback(stream, &pa_stream_read_cb, userdata);

    if (pa_stream_connect_record(stream, meta->inputName.c_str(), nullptr, PA_STREAM_NOFLAGS) != 0) {
        SDL_Log("PulseAudio failed to connect to \"%s\" for recording", meta->inputName.c_str());
        return;
    }

    //SDL_Log("Connected to %s", meta->inputName.c_str());
}

struct SinkInfoContainer
{
    audio::pulse::SinkInfoCb cb;
    std::list<std::string> sinks;
};

static void pa_sink_info_cb(pa_context *ctx, const pa_sink_info *info, int eol, void *userdata)
{
    SinkInfoContainer *sic = reinterpret_cast<SinkInfoContainer *>(userdata);
    if (info) {
        sic->sinks.push_back(std::string(info->name));
    }

    if (eol) {
        sic->cb(sic->sinks);
        delete sic;
    }
}

static void pa_context_notify_cb(pa_context *ctx, void *userdata)
{
    const pa_context_state state = pa_context_get_state(ctx);
    switch (state) {
    case PA_CONTEXT_FAILED:
        SDL_Log("Context state: %i (failed)", state);
        break;
    case PA_CONTEXT_READY:
        switch(purpose) {
        case Purpose::QUERY_SINKS:
            pa_context_get_sink_info_list(ctx, &pa_sink_info_cb, userdata);
            break;
        case Purpose::RECORD:
            pa_context_get_server_info(ctx, &pa_server_info_cb, userdata);
            break;
        }
        break;
    }
}

static int internalMain(void *userdata)
{
    loop = pa_mainloop_new();
    pa_context *ctx = pa_context_new(pa_mainloop_get_api(loop), "groggle");
    pa_context_set_state_callback(ctx, &pa_context_notify_cb, userdata);
    if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        SDL_Log("Connection to PulseAudio failed");
        return -1;
    }

    pa_mainloop_run(loop, nullptr);

    // The main loop is probably never going to return. Undead code ahead.
    // pa_stream_disconnect(..)
    pa_context_disconnect(ctx);
    pa_mainloop_free(loop);
    return 0;
}

int audio::pulse::getSinks(audio::pulse::SinkInfoCb cb)
{
    purpose = Purpose::QUERY_SINKS;
    SinkInfoContainer *sic = new SinkInfoContainer(); // Deleted after calling cb.
    sic->cb = cb;
    return internalMain(sic);
}

int audio::pulse::run(AudioMetadataPtr metadata)
{
    purpose = Purpose::RECORD;
    return internalMain(metadata.get());
}

void audio::pulse::quit(int retval)
{
    pa_mainloop_quit(loop, retval);
}
