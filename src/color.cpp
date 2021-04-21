#include "color.h"

#include <cassert>
#include <cmath>

using namespace groggle;

Color::Color(const float h, const float s, const float v)
{
    assert(h >= 0 && h < 360);
    assert(s >= 0 && s <= 1);
    assert(v >= 0 && v <= 1);
    m_h = h;
    m_s = s;
    m_v = v;

    toRgb();
}

Color::Color(const Color &other)
{
    m_h = other.h();
    m_s = other.s();
    m_v = other.v();
    toRgb();
}

void Color::toRgb()
{
    // Source: https://de.wikipedia.org/wiki/HSV-Farbraum#Umrechnung_HSV_in_RGB
    if (m_s == 0) {
        m_r = m_v;
        m_g = m_v;
        m_b = m_v;
        return;
    }

    const int hi = std::floor(m_h / 60.0);
    const float f = m_h / 60.0 - hi;
    const float p = m_v * (1.0 - m_s);
    const float q = m_v * (1.0 - m_s * f);
    const float t = m_v * (1.0 - m_s * (1.0 - f));

    switch(hi) {
    case 0:
    case 6:
        m_r = m_v;
        m_g = t;
        m_b = p;
        break;
    case 1:
        m_r = q;
        m_g = m_v;
        m_b = p;
        break;
    case 2:
        m_r = p;
        m_g = m_v;
        m_b = t;
        break;
    case 3:
        m_r = p;
        m_g = q;
        m_b = m_v;
        break;
    case 4:
        m_r = t;
        m_g = p;
        m_b = m_v;
        break;
    case 5:
        m_r = m_v;
        m_g = p;
        m_b = q;
        break;
    }
}
