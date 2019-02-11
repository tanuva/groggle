#ifndef LIGHT_H
#define LIGHT_H

#include "spectrum.h"

namespace light
{

void blackout();
void dmxinit();
void update(const groggel::Spectrum spectrum);

}

#endif
