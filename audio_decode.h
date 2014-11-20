
#ifndef _audio_decode
#define _audio_decode
#include "ogg.h"
#include "vorbis.h"

extern void audio_decode(vorbis_setup *vs, vorbis_info *vi, oggpack_buffer *opb, vorbis_decode *vd);

#endif
