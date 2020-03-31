#ifndef AUDIOMETADATA
#define AUDIOMETADATA

#include <SDL_audio.h>

#include <memory>
#include <mutex>

struct AudioMetadata
{
    std::mutex mutex; // Used to lock the whole struct.
    std::string inputName; // Can be a file URI or an audio device that supplies data to us.
    SDL_AudioDeviceID audioDeviceID;
    SDL_AudioSpec fileSpec;
    uint8_t *data = nullptr;
    uint32_t dataSize;
    float duration;
    uint32_t position;
};
typedef std::shared_ptr<AudioMetadata> AudioMetadataPtr;

#endif
