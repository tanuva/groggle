#include "light.h"
#include "spectrum.h"
#include "timer.h"

#include <fftw3.h>

#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_log.h>

#include <algorithm> // min, max
#include <cmath>
#include <cstdlib>
#include <thread>
#include <iomanip>
#include <iostream>
#include <limits>

using namespace groggel;

const unsigned int universe = 1; // universe to use for sending data
ola::client::StreamingClient *olaClient = nullptr;
ola::DmxBuffer buffer;

const int ADJ = 69;

void blackout()
{
    buffer.Blackout();
    olaClient->SendDmx(universe, buffer);
}

void dmxinit()
{
    // turn on OLA logging
    ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);

    // Create a new client.
    olaClient = new ola::client::StreamingClient((ola::client::StreamingClient::Options()));

    // Setup the client, this connects to the server
    if (!olaClient->Setup()) {
        SDL_Log("Setup failed");
        return;
    }

    blackout();
}

void sendValue(const int channel, const float v)
{
    //ola::DmxBuffer buffer;
    //buffer.Blackout(); // Don't overwrite previous settings!
    const float clamped = round(std::min(std::max(v * 255, 0.0f), 255.0f));
    const uint8_t native = static_cast<uint8_t>(clamped);
    //std::cerr << "c: " << clamped << " n: " << (int)native << std::endl;
    buffer.SetChannel(channel, native);

    if (!olaClient->SendDmx(universe, buffer)) {
        SDL_Log("SendDmx() failed");
    }
}

void sendSpectrum(const Spectrum spectrum)
{
    /* Tripar:
     * 1-3: RGB
     * 6: Dimmer
     */

    /* Ideas:
     * - Beat to white, decay to color (or vice versa)
     */
    static const int NUM_COLORS = 12;
    static float hsl[3] {18.0f, 1.0f, 0.5f};

    static float lastVal;
    float val = spectrum.get(Band::LOW);
    if ( val > lastVal ) {
        lastVal = val;
        //hsl[0] += 360 / NUM_COLORS;
    } else {
        lastVal *= 0.9f;
        val = lastVal;
    }

    float rgb[3];
    hslToRgb(hsl, rgb);

    // TODO Prepare one DMX buffer per frame, don't send every time
    sendValue(ADJ + 5, val);
    sendValue(ADJ + 0, rgb[0]);
    sendValue(ADJ + 1, rgb[1]);
    sendValue(ADJ + 2, rgb[2]);
}

struct AudioMetadata
{
    SDL_AudioDeviceID audioDeviceID;
    SDL_AudioSpec fileSpec;
    uint8_t *data = nullptr;
    uint32_t dataSize = 0;
    float duration = 0;
    std::atomic<uint32_t> position;
    //uint32_t position;
};
AudioMetadata meta;

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
static Spectrum transform(const int16_t data[], const size_t sampleCount, float *in, fftwf_complex *out)
{
    fftwf_plan p;
    p = fftwf_plan_dft_r2c_1d(sampleCount, in, out, FFTW_ESTIMATE);

    // plan_dft_r2c modifies the input array, *must* copy here!
    for (size_t i = 0; i < sampleCount; i++) {
        in[i] = data[i * meta.fileSpec.channels] / (float)std::numeric_limits<int16_t>::max();
    }

    fftwf_execute(p);

    // "Realize", normalize and store the result
    // Scaling: http://fftw.org/fftw3_doc/The-1d-Discrete-Fourier-Transform-_0028DFT_0029.html#The-1d-Discrete-Fourier-Transform-_0028DFT_0029
    const float scaleFactor = 2.0f / sampleCount;

    Spectrum spectrum;
    spectrum.add(Band::LOW, magnitude(out[1]) * scaleFactor);
    spectrum.add(Band::MID, magnitude(out[10]) * scaleFactor);
    spectrum.add(Band::HIGH, magnitude(out[20]) * scaleFactor);

    // TODO Print something like a graphic equalizer? Render with SDL?

    fftwf_destroy_plan(p);
    return spectrum;
}

void lightLoop(const int16_t data[], const uint32_t dataSize, const float duration)
{
    // The amount of frames analyzed at a time. Also determines the frequency
    // resolution of the Fourier transformation.
    static const int FRAME_SIZE = 1024;
    const int freqStep = floor(meta.fileSpec.freq / (float)FRAME_SIZE);
    SDL_Log("Freq: %i Hz - %i Hz", freqStep, FRAME_SIZE / 2 * freqStep);

    // FFTW input/output buffers are recycled
    float *in = (float*)fftwf_malloc(sizeof(float) * FRAME_SIZE);
    fftwf_complex *out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FRAME_SIZE / 2 + 1);

    // "Playback" timing
    Timer timer(duration /*s*/, 30 /*Hz*/);
    timer.setCallback([data, dataSize, in, out](const long long elapsed) {
        const size_t dataPos = std::min(meta.position.load() / 2, dataSize - FRAME_SIZE * meta.fileSpec.channels);
        const Spectrum spectrum = transform(&data[dataPos], FRAME_SIZE, in, out);
        sendSpectrum(spectrum);
    });
    timer.run();

    fftwf_free(out);
    fftwf_free(in);

    blackout();
    SDL_Log("Light thread done.");
}

void outputCallback(void *userData, uint8_t *stream, int bufferSize)
{
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userData);
    const uint32_t count = std::min(static_cast<uint32_t>(bufferSize),
                                    meta->dataSize - meta->position);
    memcpy(stream, &meta->data[meta->position], count);
    //SDL_Log("Audio pos: %f", meta->position / (float)meta->dataSize);
    meta->position += count;
}

bool loadFile(const std::string fileName)
{
    if (SDL_LoadWAV(fileName.c_str(), &meta.fileSpec, &meta.data, &meta.dataSize) == 0) {
        return false;
    }

    if (SDL_AUDIO_BITSIZE(meta.fileSpec.format) != 16
        || !SDL_AUDIO_ISLITTLEENDIAN(meta.fileSpec.format)
        || !SDL_AUDIO_ISSIGNED(meta.fileSpec.format)) {
        SDL_Log("Input is not S16LE wav!");
        // Set SDL-internal error here?
        return false;
    }

    // Data is stored as uint8_t but might actually be uint16_t, thus dataSize
    // needs to be divided by 2 to get the sample count.
    const int sampleSizeFactor = SDL_AUDIO_BITSIZE(meta.fileSpec.format) / 8;
    meta.duration = (float)meta.dataSize / sampleSizeFactor / (float)meta.fileSpec.channels / (float)meta.fileSpec.freq;

    /*SDL_Log("SDL says freq: %i format: %i channels: %i samples: %i",
            meta.fileSpec.freq,
            meta.fileSpec.format,
            meta.fileSpec.channels,
            meta.fileSpec.samples);*/
    SDL_Log("Length: %f s (%i bytes) sample size: %i LE: %i",
            meta.duration,
            meta.dataSize,
            SDL_AUDIO_MASK_BITSIZE & meta.fileSpec.format,
            SDL_AUDIO_ISLITTLEENDIAN(meta.fileSpec.format));
    return true;
}

bool openOutputDevice(const std::string name)
{
    SDL_AudioSpec have;
    SDL_AudioSpec want;
    SDL_zero(want); // O rly?
    want.freq = meta.fileSpec.freq;
    want.format = meta.fileSpec.format;
    want.channels = meta.fileSpec.channels;
    want.callback = &outputCallback;
    want.userdata = &meta;

    meta.audioDeviceID = SDL_OpenAudioDevice(name.c_str(), false, &want, &have, 0);
    if (meta.audioDeviceID == 0) {
        return false;
    }

    return true;
}

void cleanup()
{
    SDL_Quit();
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
    }
    atexit(&cleanup);

    std::string fileName(argv[1]);
    if (!loadFile(fileName)) {
        SDL_Log("Error loading \"%s\": %s", fileName.c_str(), SDL_GetError());
        return -1;
    }

    SDL_Log("Output Devices:");
    for (int i = 0; i < SDL_GetNumAudioDevices(false); i++) {
        SDL_Log("%i. %s", i, SDL_GetAudioDeviceName(i, false));
    }


    if (openOutputDevice(SDL_GetAudioDeviceName(0, false))) {
        SDL_Log("Error opening audio device: %s", SDL_GetError());
        return -1;
    }

    dmxinit();

    // Launch the lighting thread
    // TODO Store byte and sample dataSizes and positions separately! (Compute latter from former...)
    std::thread lightThread(lightLoop,
                            reinterpret_cast<int16_t *>(meta.data),
                            static_cast<size_t>(meta.dataSize / 2), // Casting int8 -> int16 halves dataSize as well!
                            meta.duration);

    // Main thread
    SDL_PauseAudioDevice(meta.audioDeviceID, 0);
    SDL_Delay(meta.duration * 1000);
    SDL_CloseAudioDevice(meta.audioDeviceID);
    lightThread.join();
    return 0;
}
