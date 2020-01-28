#ifndef LIGHT_H
#define LIGHT_H

#include "spectrum.h"

namespace groggle
{
namespace light
{

void blackout();
void init();
void update(const audio::Spectrum spectrum);

}
}

#endif
