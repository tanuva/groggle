#ifndef COLOR
#define COLOR

namespace groggle
{

class Color
{
public:
    Color(const float h, const float s, const float l);

    float h() const { return m_h; }
    float s() const { return m_s; }
    float l() const { return m_l; }

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
