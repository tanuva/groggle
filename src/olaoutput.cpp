#include "olaoutput.h"
#include "spectrum.h"

#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <SDL_log.h>

#include <algorithm> // min, max
#include <cmath>
#include <deque>

namespace groggle
{
namespace olaoutput
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
ola::DmxBuffer dmx;

const int ADJ = 69;

static const float ORANGE = 18.0f; // TODO Move into Color
static const Color curColor(ORANGE, 1.0f, 0.5f);

static std::atomic<bool> m_enabled = true; // This file should really become a class...

static void hslToRgb(const Color &color, float *rgb)
{
    // Source: https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB_alternative
    const float a = color.s() * std::min(color.l(), 1.0f - color.l());
    static const auto k = [color](const int n) {
        // (n + H / 30) mod 12
        // The modulus shall preserve the fractional component.
        const float tmp = n + color.h() / 30.0f;
        const float frac = tmp - std::floor(tmp);
        return (static_cast<int>(floor(tmp)) % 12) + frac;
    };
    static const auto f = [color, a](const int n) {
        // f(n) = L - a * max(-1, min(k - 3, 9 - k, 1))
        return color.l() - a * std::max(
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

bool isEnabled()
{
    return m_enabled;
}

void setEnabled(const bool enabled)
{
    if (m_enabled && !enabled) {
        blackout();
    }

    m_enabled = enabled;
}

void blackout()
{
    dmx.Blackout();
    olaClient->SendDmx(universe, dmx);
}

void init()
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

void update(const audio::Spectrum spectrum)
{
    /* Tripar:
     * 1-3: RGB
     * 6: Dimmer
     */

    float val = spectrum.get(audio::Band::LOW);

    static float lastVal = 0;
    if (val > lastVal) {
        lastVal = val;
    } else {
        lastVal *= 0.9f;
    }

    static Buffer<float> outputBuf(3);
    outputBuf.append(lastVal);
    const float intensity = outputBuf.average() * 2.0f; // TODO Configurable scaling factor!

    float rgb[3];
    hslToRgb(curColor, rgb);

    dmx.SetChannel(ADJ + 5, f2dmx(intensity));
    dmx.SetChannel(ADJ + 0, f2dmx(rgb[0]));
    dmx.SetChannel(ADJ + 1, f2dmx(rgb[1]));
    dmx.SetChannel(ADJ + 2, f2dmx(rgb[2]));
    if (!olaClient->SendDmx(universe, dmx)) {
        SDL_Log("SendDmx() failed");
    }
}

}
}
