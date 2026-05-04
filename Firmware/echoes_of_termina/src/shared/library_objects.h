#if !defined(PROJECT_GLOBAL_OBJECTS)
#define PROJECT_GLOBAL_OBJECTS

#include "shared/dependencies.h"
#include "pinout.h"

//* ---------------------- GLOBAL OBJECT INSTANCES ----------------------

// Sets the name of the audio device
btAudio audio = btAudio("Echoes_of_Termina");

//* Preferences (NVS)
Preferences internal_storage;

#endif // PROJECT_GLOBAL_OBJECTS
