#ifndef AUDIODECODER
#define AUDIODECODER

#include <inttypes.h>
#include <stddef.h> // size_t

int decode_audio_file(const char *path, int16_t **data, size_t *size, float *duration);

#endif
