#ifndef LIGHT_H
#define LIGHT_H

#include "spectrum.h"

namespace light
{

void blackout();
void init();
void update(const groggel::Spectrum spectrum);

}

#endif
