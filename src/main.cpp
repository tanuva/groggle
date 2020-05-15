#include "audiometadata.h"
#include "olaoutput.h"
#include "painput.h"
#include "sdlinput.h"
#include "spectrum.h"
#include "timer.h"
#include "mqttcontrol.h"

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

void lightLoop(AudioMetadataPtr meta, std::shared_ptr<OlaOutput> olaOutput)
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
    timer.setCallback([meta, in, out, olaOutput](const long long /*elapsed*/) {
        if(!olaOutput->isEnabled()) {
            return;
        }

        meta->mutex.lock();
        const int16_t *data = reinterpret_cast<int16_t*>(meta->data);
        const uint32_t sampleCount = meta->dataSize / 2; // Casting int8 -> int16 halves dataSize as well!
        const uint32_t dataPos = std::min(meta->position / 2, sampleCount - FRAME_SIZE * meta->fileSpec.channels);
        const audio::Spectrum spectrum = transform(&data[dataPos], FRAME_SIZE, meta->fileSpec.channels, in, out);
        meta->mutex.unlock();
        olaOutput->update(spectrum);
    });
    timer.run();

    fftwf_free(out);
    fftwf_free(in);

    olaOutput->blackout();
    SDL_Log("Light thread done.");
}

void mqttLoop(std::shared_ptr<OlaOutput> olaOutput)
{
    MQTT mqtt;
    mqtt.init();

    mqtt.setEnabledCallback([&mqtt, olaOutput](const bool enabled) {
        SDL_Log(">> Enabled: %i", enabled);
        olaOutput->setEnabled(enabled);
        mqtt.publishEnabled(olaOutput->isEnabled());
    });

    mqtt.setColorCallback([&mqtt, olaOutput](const Color &color) {
        SDL_Log(">> Hue: %f Sat: %f", color.h(), color.s());
        olaOutput->setColor(color);
        mqtt.publishColor(olaOutput->color());
    });

    // Publish initial properties
    mqtt.publishInfo();
    mqtt.publishEnabled(olaOutput->isEnabled());
    mqtt.publishColor(olaOutput->color());
    mqtt.run();
}

void printAudioDevices()
{
    std::cout << "SDL inputs:\n";
    for (int i = 0; i < SDL_GetNumAudioDevices(true); i++) {
        std::cout << "\t- " << SDL_GetAudioDeviceName(i, true) << std::endl;
    }

    std::cout << "SDL outputs (for file playback):\n";
    for (int i = 0; i < SDL_GetNumAudioDevices(false); i++) {
        std::cout << "\t- " << SDL_GetAudioDeviceName(i, false) << std::endl;
    }

    std::cout << "PulseAudio output monitors:\n";
    audio::pulse::getSinks([=](const std::list<std::string> sinks) {
        for (auto it = sinks.begin(); it != sinks.end(); it++) {
            std::cout << "\t- " << *it << ".monitor" << std::endl;
        }

        audio::pulse::quit(0);
    });
}

bool parseArgs(const int argc, const char **argv, Options *options)
{
    try {
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

int liveMain(std::thread lightThread, AudioMetadataPtr meta)
{
    if (!audio::sdl::openInputDevice(meta)) {
        SDL_Log("Not an SDL audio device, trying PulseAudio (%s)", SDL_GetError());
    }

    if (!audio::pulse::run(meta)) { // Returns an error or blocks via the PA main loop
        SDL_Log("Not a PulseAudio device, giving up.");
        return -1;
    }

    lightThread.join();
    //audio::sdl::closeInputDevice(meta); // Technically we need to close *only* if using SDL. Will never happen though.
    return 0;
}

int fileMain(std::thread lightThread, AudioMetadataPtr meta)
{
    if (!audio::sdl::loadFile(meta)) {
        SDL_Log("Error loading \"%s\": %s", meta->inputName.c_str(), SDL_GetError());
        return -1;
    }

    if (!audio::sdl::openOutputDevice(meta)) {
        SDL_Log("Error opening audio device: %s", SDL_GetError());
        return -1;
    }

    lightThread.join();
    audio::sdl::closeOutputDevice(meta);
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


    AudioMetadataPtr meta = std::make_shared<AudioMetadata>();
    // TODO Use PA's default sink monitor as input device
    meta->inputName = options.inputName;

    auto olaOutput = std::make_shared<OlaOutput>();
    std::thread lightThread(lightLoop, meta, olaOutput);
    std::thread mqttThread(mqttLoop, olaOutput);

    switch (options.inputType) {
    case Options::InputType::DEVICE:
        return liveMain(std::move(lightThread), meta);
    case Options::InputType::FILE:
        return fileMain(std::move(lightThread), meta);
    }

    assert(false && "This is not the case you are looking for!");
    return -1;
}
