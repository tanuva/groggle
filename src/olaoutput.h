#ifndef OLAOUTPUT_H
#define OLAOUTPUT_H

#include "spectrum.h"

namespace groggle
{
namespace olaoutput
{

void blackout();
void init();
void update(const audio::Spectrum spectrum);

}
}

#endif
