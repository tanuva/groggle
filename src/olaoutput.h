#ifndef OLAOUTPUT_H
#define OLAOUTPUT_H

#include "color.h"
#include "spectrum.h"

#include <atomic>

namespace groggle
{
namespace olaoutput
{

void blackout();
void init();
bool isEnabled();
void setEnabled(const bool enabled);
void update(const audio::Spectrum spectrum);

}
}

#endif
