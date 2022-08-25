#include "globals.h"
#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include <coreinit/debug.h>
#include <rpxloader/rpxloader.h>
#include <wups.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB();

TcpReceiver *thread = nullptr;

INITIALIZE_PLUGIN() {
    RPXLoaderStatus error;
    if ((error = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("WiiLoad Plugin: Failed to init RPXLoader. Error %d", error);
    } else {
        gLibRPXLoaderInitDone = true;
    }
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();
    DEBUG_FUNCTION_LINE("Start wiiload thread");
    thread = new TcpReceiver(4299);
}

void stopThread() {
    if (thread != nullptr) {
        delete thread;
        thread = nullptr;
    }
}

ON_APPLICATION_REQUESTS_EXIT() {
    DEBUG_FUNCTION_LINE("Stop wiiload thread");
    stopThread();

    deinitLogging();
}