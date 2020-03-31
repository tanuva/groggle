#ifndef PAINPUT
#define PAINPUT

#include "audiometadata.h"

#include <functional>
#include <list>

namespace groggle
{
namespace audio
{
namespace pulse
{

typedef std::function<void(std::list<std::string>)> SinkInfoCb;

int getSinks(audio::pulse::SinkInfoCb cb);
int run(AudioMetadataPtr metadata);
void quit(int retval);

}
}
}

#endif
