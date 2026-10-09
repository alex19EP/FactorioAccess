// miniaudio and stb_vorbis, compiled once as C. miniaudio decodes WAV, FLAC and MP3 itself; OGG
// Vorbis comes from stb_vorbis, whose declarations miniaudio needs first and whose
// implementation follows it.

#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
