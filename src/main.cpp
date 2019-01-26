//#include "audiodecoder.h"
#include "spectrum.h"
#include "timer.h"

#include <fftw3.h>
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <algorithm> // min, max
#include <cstdlib>
#include <cmath>
#include <deque>
#include <limits>
#include <thread>
#include <iomanip>
#include <iostream>
#include <vector>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_log.h>

using namespace groggel;

const unsigned int universe = 1; // universe to use for sending data
ola::client::StreamingClient *olaClient = nullptr;
ola::DmxBuffer buffer;

const int ADJ = 69;

float *in;
fftwf_complex *out;

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
        std::cerr << "Setup failed" << std::endl;
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
        std::cout << "Send DMX failed" << std::endl;
    }
}

void sendSpectrum(const Spectrum spectrum)
{
    /* Tripar:
     * 1-3: RGB
     * 6: Dimmer
     */

    static float lastVal;
    float val = spectrum.get(Band::LOW);
    if ( val > lastVal ) {
        lastVal = val;
        // TODO Trigger color change here. Beat detection!
    } else {
        lastVal *= 0.9f;
        val = lastVal;
    }

    // TODO Prepare one DMX buffer per frame, don't send every time
    sendValue(ADJ + 5, val);
    sendValue(ADJ + 0, 1.0);
    sendValue(ADJ + 1, 0.25);
}

// Try to lower the sample rate if the pi is too slow
// These are defined by the output code. Take defines to the header.
static const int INPUT_SAMPLERATE = 44100;
static const int FRAME_SIZE = 1024;

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

Spectrum transform(const int16_t data[], const size_t sampleCount)
{
    assert(sampleCount <= 2048);

    const size_t outSize = sampleCount / 2 + 1;
    fftwf_plan p;
    p = fftwf_plan_dft_r2c_1d(sampleCount, in, out, FFTW_ESTIMATE);

    // plan_dft_r2c modifies the input array, *must* copy here!
    for (size_t i = 0; i < sampleCount; i++) {
        in[i] = data[i * meta.fileSpec.channels] / (float)std::numeric_limits<int16_t>::max();
    }

    fftwf_execute(p);

    // "Realize" and normalize the result
    std::vector<float> normOut(outSize);
    const float scaleFactor = sampleCount / 2.0f;

    for (size_t i = 0; i < outSize; i++) {
        normOut[i] = /*log(*/magnitude(out[i]) / scaleFactor;
    }

    // TODO Compute frequency offsets!
    Spectrum spectrum;
    spectrum.add(Band::LOW, normOut[1]);
    spectrum.add(Band::MID, normOut[10]);
    spectrum.add(Band::HIGH, normOut[20]);

    // Print some results
    {
        //const int freqStep = floor(INPUT_SAMPLERATE / (float)sampleCount);
    }

    fftwf_destroy_plan(p);
    return spectrum;
}

void lightLoop(const int16_t data[], const uint32_t dataSize, const float duration)
{
    const int freqStep = floor(INPUT_SAMPLERATE / (float)FRAME_SIZE);
    SDL_Log("Freq: %i Hz - %i Hz", freqStep, FRAME_SIZE / 2 * freqStep);

    // FFTW input/output buffers are recycled
    const size_t FOURIER_BUFSIZE = 2048;
    in = (float*) fftwf_malloc(sizeof(float) * FOURIER_BUFSIZE);
    out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FOURIER_BUFSIZE / 2 + 1);

    // "Playback" timing
    Timer timer(duration /*s*/, 30 /*Hz*/);
    timer.setCallback([data, dataSize](const long long elapsed) {
        const size_t dataPos = std::min(meta.position.load() / 2, dataSize - FRAME_SIZE * meta.fileSpec.channels);
        const Spectrum spectrum = transform(&data[dataPos], FRAME_SIZE);
        sendSpectrum(spectrum);
    });
    timer.run();

    fftwf_free(out);
    fftwf_free(in);

    SDL_Log("Light thread done.");
    blackout();
}

void audioCallback(void *userData, uint8_t *stream, int bufferSize)
{
    AudioMetadata *meta = reinterpret_cast<AudioMetadata *>(userData);
    const uint32_t count = std::min(static_cast<uint32_t>(bufferSize),
                                    meta->dataSize - meta->position);
    memcpy(stream, &meta->data[meta->position], count);
    //SDL_Log("Audio pos: %f", meta->position / (float)meta->dataSize);
    meta->position += count;
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
    if (SDL_LoadWAV(fileName.c_str(), &meta.fileSpec, &meta.data, &meta.dataSize) == 0) {
        SDL_Log("Error loading \"%s\": %s", fileName.c_str(), SDL_GetError());
        return -1;
    }

    // Data is stored as uint8_t but might actually be uint16_t, thus dataSize
    // needs to be divided by 2 to get the sample count.
    const int sampleSizeFactor = SDL_AUDIO_BITSIZE(meta.fileSpec.format) / 8;
    meta.duration = (float)meta.dataSize / sampleSizeFactor / (float)meta.fileSpec.channels / (float)meta.fileSpec.freq;

    SDL_Log("SDL says freq: %i format: %i channels: %i samples: %i",
            meta.fileSpec.freq,
            meta.fileSpec.format,
            meta.fileSpec.channels,
            meta.fileSpec.samples);
    SDL_Log("Length: %f s (%i bytes) sample size: %i LE: %i",
            meta.duration,
            meta.dataSize,
            SDL_AUDIO_MASK_BITSIZE & meta.fileSpec.format,
            SDL_AUDIO_ISLITTLEENDIAN(meta.fileSpec.format));

    if (SDL_AUDIO_BITSIZE(meta.fileSpec.format) != 16
        || !SDL_AUDIO_ISLITTLEENDIAN(meta.fileSpec.format)
        || !SDL_AUDIO_ISSIGNED(meta.fileSpec.format)) {
        SDL_Log("Input is not S16LE wav!");
        return -1;
    }

    SDL_Log("Audio Devices:");
    for (int i = 0; i < SDL_GetNumAudioDevices(false); i++) {
        SDL_Log("%i. %s", i, SDL_GetAudioDeviceName(i, false));
    }
    std::string audioDeviceName = SDL_GetAudioDeviceName(0, false);

    SDL_AudioSpec have;
    SDL_AudioSpec want;
    SDL_zero(want); // O rly?
    want.freq = meta.fileSpec.freq;
    want.format = meta.fileSpec.format;
    want.channels = meta.fileSpec.channels;
    want.callback = &audioCallback;
    want.userdata = &meta;

    meta.audioDeviceID = SDL_OpenAudioDevice(audioDeviceName.c_str(), false, &want, &have, 0);
    if (meta.audioDeviceID == 0) {
        SDL_Log("Error opening audio device");
        return -1;
    }

    dmxinit();
    //atexit(&blackout);

    // Launch the lighting thread
    // TODO Store byte and sample dataSizes and positions separately! (Compute latter from former...)
    std::thread lightThread(lightLoop,
                            reinterpret_cast<int16_t *>(meta.data),
                            static_cast<size_t>(meta.dataSize / 2), // Casting int8 -> int16 halves dataSize as well!
                            meta.duration);

    // Main thread
    //audiotest(data, dataSize, duration);

    SDL_PauseAudioDevice(meta.audioDeviceID, 0);
    SDL_Delay(meta.duration * 1000);
    SDL_CloseAudioDevice(meta.audioDeviceID);
    lightThread.join();
    return 0;
}
