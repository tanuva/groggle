#include "olaoutput.h"

#include "spectrum.h"

#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>

#include <SDL_log.h>

#include <deque>

namespace groggle
{

static const float ORANGE = 18.0f; // TODO Move into Color

OlaOutput::OlaOutput()
    : m_color(ORANGE, 1.0f, 0.5f)
    , m_magnitudeBuf(64)
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

    if (val > m_intensity) {
        m_intensity = val;
    } else {
        m_intensity *= 0.9f;
    }

    m_magnitudeBuf.append(val);
    //const float scale = 0.5 * 1.0 / std::max(m_magnitudeBuf.average(), 0.01f);
    const float scale = 1.0;

    m_dmx.SetChannel(m_adj + 5, Color::f2uint8(m_intensity * scale));
    m_dmx.SetChannel(m_adj + 0, Color::f2uint8(m_color.r()));
    m_dmx.SetChannel(m_adj + 1, Color::f2uint8(m_color.g()));
    m_dmx.SetChannel(m_adj + 2, Color::f2uint8(m_color.b()));
    if (!m_olaClient->SendDmx(m_universe, m_dmx)) {
        SDL_Log("SendDmx() failed");
    }
}

}
