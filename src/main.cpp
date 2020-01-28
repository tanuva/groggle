#include "audiometadata.h"
#include "light.h"
#include "spectrum.h"
#include "timer.h"

#include <fftw3.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_log.h>

#include <tclap/CmdLine.h>

#include <algorithm> // min, max
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>

using namespace groggle;
using namespace TCLAP;

struct Options
{
    enum class InputType {
        FILE,
        DEVICE
    };
    InputType inputType;

    std::string inputName;
    bool listDevices;
};

static inline float magnitude(const float f[])
{
    return sqrt(pow(f[0], 2) + pow(f[1], 2));
}

/**
 * @param data The audio data to transform in s16le format
 * @param sampleCount Length of the audio data in samples
 * @param in Preallocated FFTW input array
 * @param out Preallocated FFTW output array
 */
static audio::Spectrum transform(const int16_t data[], const size_t sampleCount, const int channels, float *in, fftwf_complex *out)
{
    fftwf_plan p;
    p = fftwf_plan_dft_r2c_1d(sampleCount, in, out, FFTW_ESTIMATE);

    // plan_dft_r2c modifies the input array, *must* copy here!
    for (size_t i = 0; i < sampleCount; i++) {
        in[i] = data[i * channels] / (float)std::numeric_limits<int16_t>::max();
    }

    fftwf_execute(p);

    // "Realize", normalize and store the result
    // Scaling: http://fftw.org/fftw3_doc/The-1d-Discrete-Fourier-Transform-_0028DFT_0029.html#The-1d-Discrete-Fourier-Transform-_0028DFT_0029
    const float scaleFactor = 2.0f / sampleCount;

    audio::Spectrum spectrum;
    spectrum.add(audio::Band::LOW, magnitude(out[1]) * scaleFactor);
    spectrum.add(audio::Band::MID, magnitude(out[10]) * scaleFactor);
    spectrum.add(audio::Band::HIGH, magnitude(out[20]) * scaleFactor);

    // TODO Print something like a graphic equalizer? Render with SDL?

    fftwf_destroy_plan(p);
    return spectrum;
}

void lightLoop(AudioMetadataPtr meta)
{
    // Hack. Wait for the first audio data to arrive so that we can compute the
    // frequency data that is printed below.
    bool ready = false;
    do {
        std::lock_guard<std::mutex>(meta->mutex);
        ready = meta->dataSize > 0;
    } while (!ready);

    // The amount of frames analyzed at a time. Also determines the frequency
    // resolution of the Fourier transformation.
    static const int FRAME_SIZE = 1024;
    const int freqStep = floor(meta->fileSpec.freq / (float)FRAME_SIZE);
    SDL_Log("Frequency bucket size: %i Hz", freqStep);
    SDL_Log("Max frequency: %i Hz", FRAME_SIZE / 2 * freqStep);

    // FFTW input/output buffers are recycled
    float *in = (float*)fftwf_malloc(sizeof(float) * FRAME_SIZE);
    fftwf_complex *out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FRAME_SIZE / 2 + 1);

    // "Playback" timing
    Timer timer(meta->duration /*s*/, 30 /*Hz*/);
    timer.setCallback([meta, in, out](const long long elapsed) {
        meta->mutex.lock();
        const int16_t *data = reinterpret_cast<int16_t*>(meta->data);
        const uint32_t sampleCount = meta->dataSize / 2; // Casting int8 -> int16 halves dataSize as well!
        const uint32_t dataPos = std::min(meta->position / 2, sampleCount - FRAME_SIZE * meta->fileSpec.channels);
        const audio::Spectrum spectrum = transform(&data[dataPos], FRAME_SIZE, meta->fileSpec.channels, in, out);
        meta->mutex.unlock();
        light::update(spectrum);
    });
    timer.run();

    fftwf_free(out);
    fftwf_free(in);

    light::blackout();
    SDL_Log("Light thread done.");
}

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

bool loadFile(const std::string fileName, AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    if (SDL_LoadWAV(fileName.c_str(), &meta->fileSpec, &meta->data, &meta->dataSize) == 0) {
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
    return true;
}

bool openInputDevice(const std::string name, AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_AudioSpec have;
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.samples = 1024; // FRAME_SIZE?
    want.channels = 1;
    want.callback = &inputCallback;
    want.userdata = meta.get();

    meta->audioDeviceID = SDL_OpenAudioDevice(name.c_str(), true, &want, &have, 0);
    if (meta->audioDeviceID == 0) {
        return false;
    }

    meta->fileSpec = have;
    meta->duration = 0; // infinity
    meta->data = new uint8_t[have.samples * 2]; // samples != bytes

    SDL_PauseAudioDevice(meta->audioDeviceID, 0);
    return true;
}

void closeInputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_PauseAudioDevice(meta->audioDeviceID, 1);
    SDL_CloseAudioDevice(meta->audioDeviceID);
    delete[] meta->data;
}

bool openOutputDevice(const std::string name, AudioMetadataPtr meta)
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

    meta->audioDeviceID = SDL_OpenAudioDevice(name.c_str(), false, &want, &have, 0);
    if (meta->audioDeviceID == 0) {
        return false;
    }

    SDL_PauseAudioDevice(meta->audioDeviceID, 0);
    return true;
}

void closeOutputDevice(AudioMetadataPtr meta)
{
    std::lock_guard<std::mutex>(meta->mutex);
    SDL_PauseAudioDevice(meta->audioDeviceID, 1);
    SDL_CloseAudioDevice(meta->audioDeviceID);
}

void printAudioDevices()
{
    std::cout << "Input Devices:\n";
    for (int i = 0; i < SDL_GetNumAudioDevices(true); i++) {
        std::cout << "\t- " << SDL_GetAudioDeviceName(i, true) << std::endl;
    }

    std::cout << "Output Devices:\n";
    for (int i = 0; i < SDL_GetNumAudioDevices(false); i++) {
        std::cout << "\t- " << SDL_GetAudioDeviceName(i, false) << std::endl;
    }
}

bool parseArgs(const int argc, const char **argv, Options *options)
{
    try {
        // Define the command line object.
        CmdLine cmd("", ' ', "0.1");

        ValueArg<std::string> inputArg("i",
                                       "input",
                                       "Input device name",
                                       false,
                                       "",
                                       "string");
        cmd.add(inputArg);

        SwitchArg devicesArg("l",
                             "list-devices",
                             "Lists the available input/output devices.",
                             false);
        cmd.add(devicesArg);

        ValueArg<std::string> fileNameArg("f",
                                          "file",
                                          "Path to the audio file to play.",
                                          false,
                                          "",
                                          "string");
        cmd.add(fileNameArg);

        cmd.parse(argc, argv);
        options->inputType = fileNameArg.isSet() ? Options::InputType::FILE : Options::InputType::DEVICE;
        switch (options->inputType) {
            case Options::InputType::FILE:
                options->inputName = fileNameArg.getValue();
                break;
            case Options::InputType::DEVICE:
                options->inputName = inputArg.getValue();
                break;
            default:
                assert(false && "Unexpected input type");
        }
        options->listDevices = devicesArg.getValue();

        if (fileNameArg.isSet() && inputArg.isSet()) {
            throw ArgException("Cannot set input file and input device at the same time.", "fileName");
        }
    } catch (ArgException &e) {
        std::cerr << "Failed to parse command line: " << e.argId() << ": " << e.error() << std::endl;
        return false;
    }

    return true;
}

int liveMain(std::thread lightThread, AudioMetadataPtr meta, std::string audioDeviceName)
{
    if (!openInputDevice(audioDeviceName, meta)) {
        SDL_Log("Error opening audio device: %s", SDL_GetError());
        return -1;
    }

    lightThread.join(); // Wait for Godot non-blockingly
    closeInputDevice(meta);
    return 0;
}

int fileMain(std::thread lightThread, AudioMetadataPtr meta, std::string fileName)
{
    if (!loadFile(fileName, meta)) {
        SDL_Log("Error loading \"%s\": %s", fileName.c_str(), SDL_GetError());
        return -1;
    }

    if (!openOutputDevice(SDL_GetAudioDeviceName(0, false), meta)) {
        SDL_Log("Error opening audio device: %s", SDL_GetError());
        return -1;
    }

    lightThread.join();
    closeOutputDevice(meta);
    SDL_FreeWAV(meta->data);
    return 0;
}

void cleanup()
{
    SDL_Quit();
}

int main(const int argc, const char **argv)
{
    Options options;
    if (!parseArgs(argc, argv, &options)) {
        return -1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    atexit(&cleanup);

    if (options.listDevices) {
        printAudioDevices();
        return 0;
    }

    // Use the first audio input device if none was given.
    if (options.inputName.size() == 0 && SDL_GetNumAudioDevices(true) > 0) {
        options.inputName = SDL_GetAudioDeviceName(0, true);
    }

    AudioMetadataPtr meta = std::make_shared<AudioMetadata>();

    // Launch the lighting thread
    light::init();
    std::thread lightThread(lightLoop, meta);

    switch (options.inputType) {
    case Options::InputType::DEVICE:
        return liveMain(std::move(lightThread), meta, options.inputName);
    case Options::InputType::FILE:
        return fileMain(std::move(lightThread), meta, options.inputName);
    }

    assert(false && "This is not the case you are looking for!");
    return -1;
}
