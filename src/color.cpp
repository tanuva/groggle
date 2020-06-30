#include "color.h"

#include <cassert>
#include <cmath>

using namespace groggle;

Color::Color(const float h, const float s, const float l)
{
    assert(h >= 0 && h < 360);
    assert(s >= 0 && s <= 1);
    assert(l >= 0 && l <= 1);
    m_h = h;
    m_s = s;
    m_l = l;

    toRgb();
}

Color::Color(const Color &other)
{
    m_h = other.h();
    m_s = other.s();
    m_l = other.l();
    toRgb();
}

void Color::toRgb()
{
    // Source: https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB_alternative
    const float a = m_s * std::min(m_l, 1.0f - m_l);
    static const auto k = [this](const int n) {
        // (n + H / 30) mod 12
        // The modulus shall preserve the fractional component.
        const float tmp = n + m_h / 30.0f;
        const float frac = tmp - std::floor(tmp);
        return (static_cast<int>(floor(tmp)) % 12) + frac;
    };
    static const auto f = [this, a](const int n) {
        // f(n) = L - a * max(-1, min(k - 3, 9 - k, 1))
        return l() - a * std::max(
                                -1.0f,
                                std::min(
                                    1.0f,
                                    std::min(k(n) - 3.0f,
                                             9.0f - k(n))));
    };

    m_r = f(0);
    m_g = f(8);
    m_b = f(4);
}
