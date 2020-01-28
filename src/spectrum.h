#ifndef SPECTRUM
#define SPECTRUM

#include <vector>

namespace groggle
{
namespace audio
{

namespace Band
{
    enum Enum {
        LOW = 0,
        MID,
        HIGH,
        BAND_COUNT
    };
}

class Spectrum
{
public:
    Spectrum();
    void add(Band::Enum band, const float value);
    float get(Band::Enum band) const {
        return m_bands[band];
    };

private:
    float m_bands[Band::BAND_COUNT];
};

}
}

#endif
