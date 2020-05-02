#include "color.h"

#include <cassert>

using namespace groggle;

Color::Color(const float h, const float s, const float l)
{
    assert(h >= 0 && h < 360);
    assert(s >= 0 && s <= 1);
    assert(l >= 0 && l <= 1);
    m_h = h;
    m_s = s;
    m_l = l;
}
