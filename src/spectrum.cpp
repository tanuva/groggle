#include "spectrum.h"

using namespace groggel;

Spectrum::Spectrum()
{
}

void Spectrum::add(Band::Enum band, const float value)
{
    // TODO Build incremental average?
    // Use FPS counter smoothing for that
    m_bands[band] = value;
}
