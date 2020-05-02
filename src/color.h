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

    void toRgb(float *r, float *g, float *b) const;

private:
    float m_h = 0;
    float m_s = 0;
    float m_l = 0;
};

}

#endif
