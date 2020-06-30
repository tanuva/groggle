#ifndef COLOR
#define COLOR

#include <algorithm> // min, max
#include <cmath>
#include <cstdint>

namespace groggle
{

class Color
{
public:
    static inline uint8_t f2uint8(const float f) {
        const float clamped = round(std::min(std::max(f * 255, 0.0f), 255.0f));
        return static_cast<uint8_t>(clamped);
    }

    Color(const float h, const float s, const float l);

    float h() const { return m_h; }
    void setH(const float h) { m_h = h; toRgb(); }
    float s() const { return m_s; }
    void setS(const float s) { m_s = s; toRgb(); }
    float l() const { return m_l; }
    void setL(const float l) { m_l = l; toRgb(); }

    float r() const { return m_r; }
    float g() const { return m_g; }
    float b() const { return m_b; }

private:
    void toRgb();

    float m_h = 0;
    float m_s = 0;
    float m_l = 0;
    float m_r = 0;
    float m_g = 0;
    float m_b = 0;
};

}

#endif
