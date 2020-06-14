#include "sdlinput.h"

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_log.h>

using namespace groggle;

void inputCallback(void *userData, uint8_t *stream, int bufferSize)
{
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userData);
    std::lock_guard<std::mutex>(meta->mutex);
    memcpy(meta->data, stream, bufferSize);
    meta->dataSize = bufferSize;
    meta->position = 0;
}

void outputCallback(void *userData, uint8_t *stream, int bufferSize)
{
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userData);
    std::lock_guard<std::mutex>(meta->mutex);
    const uint32_t count = std::min(static_cast<uint32_t>(bufferSize),
                                    meta->dataSize - meta->position);
    memcpy(stream, &meta->data[meta->position], count);
    //SDL_Log("Audio pos: %f", meta->position / (float)meta->dataSize);
    meta->position += count;
}

bool audio::sdl::openInputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_AudioSpec have;
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.samples = 1024; // Buffer size in samples
    want.channels = 1;
    want.callback = &inputCallback;
    want.userdata = meta.get();

    meta->audioDeviceID = SDL_OpenAudioDevice(meta->audioDevice.c_str(), true, &want, &have, 0);
    if (meta->audioDeviceID == 0) {
        return false;
    }

    SDL_Log("Want Freq: %i Format: 0x%0i Samples: %i Channels: %i", want.freq, want.format, want.samples, want.channels);
    SDL_Log("Have Freq: %i Format: 0x%0i Samples: %i Channels: %i", have.freq, have.format, have.samples, have.channels);

    meta->fileSpec = have;
    meta->duration = 0; // infinity
    meta->data = new uint8_t[have.size]; // samples != bytes

    SDL_PauseAudioDevice(meta->audioDeviceID, 0);
    return true;
}

void audio::sdl::closeInputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_PauseAudioDevice(meta->audioDeviceID, 1);
    SDL_CloseAudioDevice(meta->audioDeviceID);
    delete[] meta->data;
}

bool audio::sdl::openOutputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_AudioSpec have;
    SDL_AudioSpec want;
    SDL_zero(want); // O rly?
    want.freq = meta->fileSpec.freq;
    want.format = meta->fileSpec.format;
    want.channels = meta->fileSpec.channels;
    want.callback = &outputCallback;
    want.userdata = meta.get();

    meta->audioDeviceID = SDL_OpenAudioDevice(meta->audioDevice.c_str(), false, &want, &have, 0);
    if (meta->audioDeviceID == 0) {
        return false;
    }

    SDL_Log("Want Freq: %i Format: 0x%0i Samples: %i Channels: %i", want.freq, want.format, want.samples, want.channels);
    SDL_Log("Have Freq: %i Format: 0x%0i Samples: %i Channels: %i", have.freq, have.format, have.samples, have.channels);

    SDL_PauseAudioDevice(meta->audioDeviceID, 0);
    return true;
}

void audio::sdl::closeOutputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_PauseAudioDevice(meta->audioDeviceID, 1);
    SDL_CloseAudioDevice(meta->audioDeviceID);
    SDL_FreeWAV(meta->data);
}

bool audio::sdl::loadFile(AudioMetadataPtr meta)
{
    //std::lock_guard<std::mutex>(meta->mutex);
    meta->mutex.lock();
    if (SDL_LoadWAV(meta->inputFile.c_str(), &meta->fileSpec, &meta->data, &meta->dataSize) == 0) {
        return false;
    }

    if (SDL_AUDIO_BITSIZE(meta->fileSpec.format) != 16
        || !SDL_AUDIO_ISLITTLEENDIAN(meta->fileSpec.format)
        || !SDL_AUDIO_ISSIGNED(meta->fileSpec.format)) {
        SDL_Log("Input is not S16LE wav!");
        // Set SDL-internal error here?
        return false;
    }

    // Data is stored as uint8_t but might actually be int16_t, thus dataSize
    // needs to be divided by 2 to get the sample count.
    const int sampleSizeFactor = SDL_AUDIO_BITSIZE(meta->fileSpec.format) / 8;
    meta->duration = (float)meta->dataSize / sampleSizeFactor / (float)meta->fileSpec.channels / (float)meta->fileSpec.freq;

    /*SDL_Log("SDL says freq: %i format: %i channels: %i samples: %i",
            meta->fileSpec.freq,
            meta->fileSpec.format,
            meta->fileSpec.channels,
            meta->fileSpec.samples);*/
    SDL_Log("Length: %f s (%i bytes) sample size: %i LE: %i",
            meta->duration,
            meta->dataSize,
            SDL_AUDIO_MASK_BITSIZE & meta->fileSpec.format,
            SDL_AUDIO_ISLITTLEENDIAN(meta->fileSpec.format));
    meta->mutex.unlock();
    return true;
}
