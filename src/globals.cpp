#include "globals.h"

bool gLibRPXLoaderInitDone __attribute__((section(".data")))     = false;
bool gWiiloadServerEnabled __attribute__((section(".data")))     = true;
bool gNotificationModuleLoaded __attribute__((section(".data"))) = true;