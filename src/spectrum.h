#ifndef SPECTRUM
#define SPECTRUM

#include <vector>

namespace groggle
{
namespace audio
{

enum class Band {
    LOW = 0,
    MID,
    HIGH,
    BAND_COUNT
};

class Spectrum
{
public:
    Spectrum();
    void add(Band band, const float value);
    float get(Band band) const {
        return m_bands[static_cast<int>(band)];
    };

private:
    float m_bands[static_cast<int>(Band::BAND_COUNT)];
};

}
}

#endif
