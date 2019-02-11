#ifndef LIGHT_H
#define LIGHT_H

#include "spectrum.h"

namespace light
{

void blackout();
void dmxinit();
void sendSpectrum(const groggel::Spectrum spectrum);

}

#endif
