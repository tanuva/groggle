#include "audiodecoder.h"
#include "spectrum.h"
#include "timer.h"

#include <ao/ao.h>
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

static inline float magnitude(const float f[])
{
    return sqrt(pow(f[0], 2) + pow(f[1], 2));
}

Spectrum transform(const int16_t data[], const size_t dataSize)
{
    assert(dataSize <= 2048);

    const size_t outSize = dataSize / 2 + 1;
    fftwf_plan p;
    p = fftwf_plan_dft_r2c_1d(dataSize, in, out, FFTW_ESTIMATE);

    // plan_dft_r2c modifies the input array, *must* copy here!
    for (size_t i = 0; i < dataSize; i++) {
        in[i] = data[i] / (float)std::numeric_limits<int16_t>::max();
    }

    fftwf_execute(p);

    // "Realize" and normalize the result
    std::vector<float> normOut(outSize);
    const float scaleFactor = dataSize / 2.0f;

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
        const int freqStep = floor(INPUT_SAMPLERATE / (float)dataSize);

        /*std::cout << "Freq:\t\tMag:" << std::endl;
        for (int i = 0; i * freqStep < 1024; i += 1) {
            std::cout << std::setfill('0') << std::setw(5) << i * freqStep << " Hz\t" << magnitude(out[i]) << ' ' << normOut[i] << std::endl;
        }
        std::cout << std::endl;*/

        // For pasting into WA
        /*std::cout << "Mag:\n";
        for (int i = 0; i < dataSize / 2 + 1; i++) {
            std::cout << magnitude(out[i][0], out[i][1]) << ',';
        }
        std::cout << std::endl;*/
    }

    fftwf_destroy_plan(p);
    return spectrum;
}

void audiotest(const int16_t data[], const size_t dataSize, const float duration)
{
    const int freqStep = floor(INPUT_SAMPLERATE / (float)FRAME_SIZE);
    std::cout << "Freq min: " << freqStep << " Hz max: " << FRAME_SIZE / 2 * freqStep << " Hz\n";

    // FFTW input/output buffers are recycled
    const size_t FOURIER_BUFSIZE = 2048;
    in = (float*) fftwf_malloc(sizeof(float) * FOURIER_BUFSIZE);
    out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FOURIER_BUFSIZE / 2 + 1);

    const long long S_TO_NS = 1000 * 1000 * 1000;
    static const long long durationNs = duration * S_TO_NS;

    // "Playback" timing
    Timer timer(duration /*s*/, 30 /*Hz*/);
    timer.setCallback([data, dataSize](const long long elapsed) {
        const float elapsedPerc = fmax(0.0f, fmin(elapsed / (float)durationNs, 100.0f));
        const size_t dataPos = round(fmin(dataSize * elapsedPerc,
                                          dataSize - FRAME_SIZE)); // dataPos <= (dataSize - FRAME_SIZE)
        const Spectrum spectrum = transform(&data[dataPos], FRAME_SIZE);
        //std::cout << "Elapsed: " << elapsed / S_TO_NS  << " s\n";
        sendSpectrum(spectrum);
    });
    timer.run();

    fftwf_free(out);
    fftwf_free(in);

    blackout();
}

void play(int16_t data[], size_t dataSize, ao_sample_format *sampleFormat)
{
    const int driverId = ao_default_driver_id();
    if (driverId < 0) {
        std::cerr << "libao couldn't open the default audio driver\n";
        return;
    }

    ao_device *outputDevice = ao_open_live(driverId, sampleFormat, NULL);
    // Need to "duplicate" the data, libao reads it as 8 bit chunks!
    const int result = ao_play(outputDevice, (char*)data, dataSize * 2);
}
#include <unistd.h>
int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "argc != 2\n";
        return -1;
    }

    std::string fileName(argv[1]);

    // TODO Average both channels (or support stereo lighting!)
    int16_t *data = nullptr;
    size_t dataSize = 0;
    float duration = 0;
    if (decode_audio_file(fileName.c_str(), &data, &dataSize, &duration) != 0) {
        std::cerr << "Failed to decode the audio file\n";
        return -1;
    }
    std::cout << "Read " << duration << " s (" << dataSize << " bytes) of audio data\n";

    if (dataSize == 0) {
        std::cerr << "dataSize = 0, bailing out!\n";
        return -1;
    }

    dmxinit();
    atexit(&blackout);

    // Set up audio output
    ao_initialize();

    ao_sample_format sampleFormat;
    sampleFormat.bits = 16;
    sampleFormat.rate = INPUT_SAMPLERATE; // Unify with audio decoding
    sampleFormat.channels = 1;
    sampleFormat.byte_format = AO_FMT_NATIVE;
    sampleFormat.matrix = 0;

    // Launch the player thread
    std::thread playerThread(play, data, dataSize, &sampleFormat);

    // Main thread
    //usleep(50 * 1000);
    audiotest(data, dataSize, duration);

    playerThread.join();
    ao_shutdown();

    return 0;
}
