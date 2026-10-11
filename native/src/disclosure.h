#pragma once

// Keeps a game running this DLL from reaching Wube as if it were vanilla. Wube agreed to the DLL on
// the condition that its crashes never bother them, so crash log uploading is forced off, the log
// says the DLL is loaded, and the version the game shows names it. The uploader process itself is
// ended by the winmm.dll stub (src/stub/stub.cpp), before any core is loaded.
namespace fa::disclosure {

// Called on the main thread from every application Gui logic: keeps the crash log upload setting
// off, greys its checkbox out in Settings > Other, and writes the log line once.
void tick();

// MinHook detour for ApplicationVersion::strDetailedNoBuildMode, and where MinHook keeps the original.
void* versionDetour();
void** versionOriginal();

} // namespace fa::disclosure
