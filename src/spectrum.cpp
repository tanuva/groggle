#include "spectrum.h"

namespace groggle
{
namespace audio
{

audio::Spectrum::Spectrum()
{
}

void audio::Spectrum::add(Band::Enum band, const float value)
{
    // TODO Build incremental average?
    // Use FPS counter smoothing for that
    m_bands[band] = value;
}

}
}
