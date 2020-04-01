#ifndef SDLINPUT
#define SDLINPUT

#include "audiometadata.h"

namespace groggle
{
namespace audio
{
namespace sdl
{

bool openInputDevice(AudioMetadataPtr meta);
void closeInputDevice(AudioMetadataPtr meta);
bool openOutputDevice(AudioMetadataPtr meta);
void closeOutputDevice(AudioMetadataPtr meta);
bool loadFile(AudioMetadataPtr meta);

}
}
}

#endif
