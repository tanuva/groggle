#ifndef OLAOUTPUT_H
#define OLAOUTPUT_H

#include "color.h"
#include "ringbuffer.h"
#include "spectrum.h"

#include <ola/DmxBuffer.h>
#include <ola/client/StreamingClient.h>

#include <mutex>

namespace groggle
{

class OlaOutput
{
public:
    OlaOutput();
    void blackout();
    Color color();
    void setColor(const Color &color);
    bool isEnabled() { return m_enabled; }
    void setEnabled(const bool enabled);
    void update(const audio::Spectrum spectrum);

private:
    std::mutex m_mutex;
    ola::client::StreamingClient *m_olaClient = nullptr;
    ola::DmxBuffer m_dmx;

    const unsigned int m_universe = 1; // universe to use for sending data
    const int m_adj = 69;
    Color m_color;
    float m_intensity = 0;
    RingBuffer<float> m_magnitudeBuf;
    bool m_enabled = true;
};

}

#endif
