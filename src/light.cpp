#include "light.h"
#include "spectrum.h"

#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <SDL_log.h>

#include <algorithm> // min, max
#include <cmath>
#include <deque>

using namespace groggel;

namespace light
{

template <typename T>
class Buffer
{
public:
    Buffer(const size_t size)
        : m_desiredSize(size)
    {}

    void append(T t) {
        m_values.push_back(t);
        while (m_values.size() > m_desiredSize) {
            m_values.pop_front();
        }
    }

    T average() const {
        T avg = 0;
        for (const T v : m_values) {
            avg += v;
        }
        return avg / (m_values.size() > 0 ? m_values.size() : 1);
    }

private:
    size_t m_desiredSize;
    std::deque<T> m_values;
};

const unsigned int universe = 1; // universe to use for sending data
ola::client::StreamingClient *olaClient = nullptr;
ola::DmxBuffer buffer;

const int ADJ = 69;

static void hslToRgb(const float hsl[3], float *rgb)
{
    // Source: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSL_conversion
    const float a = hsl[1] * std::min(hsl[2], 1.0f - hsl[2]);
    static const auto k = [hsl](const int n) {
        // (n + H / 30) mod 12
        // The modulus shall preserve the fractional component.
        const float tmp = n + hsl[0] / 30.0f;
        const float frac = tmp - std::floor(tmp);
        return (static_cast<int>(floor(tmp)) % 12) + frac;
    };
    static const auto f = [hsl, a](const int n) {
        // f(n) = L - a * max(k - 3, 9 - k, 1), -1)
        return hsl[2] - a * std::max(
                                -1.0f,
                                std::min(
                                    1.0f,
                                    std::min(k(n) - 3.0f,
                                             9.0f - k(n))));
    };

    rgb[0] = f(0);
    rgb[1] = f(8);
    rgb[2] = f(4);
}

static uint8_t f2dmx(const float f)
{
    const float clamped = round(std::min(std::max(f * 255, 0.0f), 255.0f));
    return static_cast<uint8_t>(clamped);
}

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

void update(const Spectrum spectrum)
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

    buffer.SetChannel(ADJ + 5, f2dmx(val));
    buffer.SetChannel(ADJ + 0, f2dmx(rgb[0]));
    buffer.SetChannel(ADJ + 1, f2dmx(rgb[1]));
    buffer.SetChannel(ADJ + 2, f2dmx(rgb[2]));
    if (!olaClient->SendDmx(universe, buffer)) {
        SDL_Log("SendDmx() failed");
    }
}

}
