// miniaudio and stb_vorbis, compiled once as C. miniaudio decodes WAV, FLAC and MP3 itself; OGG
// Vorbis comes from stb_vorbis, whose declarations miniaudio needs first and whose
// implementation follows it.

// MA_NO_ENCODING and MA_NO_RESOURCE_MANAGER come from CMake, so audio.cpp sees the same struct
// layouts.

// Low-latency shared mode (IAudioClient3) sets the period of the device's shared engine, which
// every other program on it then runs at; on some devices that distorts all of them, the screen
// reader too. The cue sounds don't need it.
#define MA_WASAPI_NO_LOW_LATENCY_SHARED_MODE

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
