#include "olaoutput.h"
#include "spectrum.h"

#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <SDL_log.h>

#include <algorithm> // min, max
#include <cmath>
#include <deque>

namespace groggle
{

#if 0
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
#endif

static uint8_t f2dmx(const float f)
{
    const float clamped = round(std::min(std::max(f * 255, 0.0f), 255.0f));
    return static_cast<uint8_t>(clamped);
}

static const float ORANGE = 18.0f; // TODO Move into Color

OlaOutput::OlaOutput()
    : m_color(ORANGE, 1.0f, 0.5f)
{
    // turn on OLA logging
    ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);

    // Create a new client.
    m_olaClient = new ola::client::StreamingClient((ola::client::StreamingClient::Options()));

    // Setup the client, this connects to the server
    if (!m_olaClient->Setup()) {
        SDL_Log("Setup failed");
        return;
    }

    blackout();
}

Color OlaOutput::color()
{
    return m_color;
}

void OlaOutput::setColor(const Color &color)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_color = color;
}

void OlaOutput::setEnabled(const bool enabled)
{
    if (m_enabled && !enabled) {
        blackout();
    }

    // Lock after blackout, it also acquires a lock!
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enabled;
}

void OlaOutput::blackout()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dmx.Blackout();
    m_olaClient->SendDmx(m_universe, m_dmx);
}

void OlaOutput::update(const audio::Spectrum spectrum)
{
    /* Tripar:
     * 1-3: RGB
     * 6: Dimmer
     */

    std::lock_guard<std::mutex> lock(m_mutex);
    const float val = spectrum.at(1);

    static float lastVal = 0;
    if (val > lastVal) {
        lastVal = val;
    } else {
        lastVal *= 0.9f;
    }

    const float intensity = lastVal * 2.0f; // TODO Configurable scaling factor!

    m_dmx.SetChannel(m_adj + 5, f2dmx(intensity));
    m_dmx.SetChannel(m_adj + 0, f2dmx(m_color.r()));
    m_dmx.SetChannel(m_adj + 1, f2dmx(m_color.g()));
    m_dmx.SetChannel(m_adj + 2, f2dmx(m_color.b()));
    if (!m_olaClient->SendDmx(m_universe, m_dmx)) {
        SDL_Log("SendDmx() failed");
    }
}

}
