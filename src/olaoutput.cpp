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
static Color curColor(ORANGE, 1.0f, 0.5f);

static std::atomic<bool> m_enabled = true; // This file should really become a class...

static uint8_t f2dmx(const float f)
{
    const float clamped = round(std::min(std::max(f * 255, 0.0f), 255.0f));
    return static_cast<uint8_t>(clamped);
}

Color color()
{
    return curColor;
}

void setColor(const Color &color)
{
    curColor = color;
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

    dmx.SetChannel(ADJ + 5, f2dmx(intensity));
    dmx.SetChannel(ADJ + 0, f2dmx(curColor.r()));
    dmx.SetChannel(ADJ + 1, f2dmx(curColor.g()));
    dmx.SetChannel(ADJ + 2, f2dmx(curColor.b()));
    if (!olaClient->SendDmx(universe, dmx)) {
        SDL_Log("SendDmx() failed");
    }
}

}
}
