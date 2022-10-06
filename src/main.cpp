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
    thread = nullptr;
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();

    if (thread != nullptr) {
        DEBUG_FUNCTION_LINE_WARN("The wiiload thread is still allocated but not running.");
        thread->skipJoin = true;
        delete thread;
        thread = nullptr;
    }
    DEBUG_FUNCTION_LINE("Start wiiload thread");
    thread = new TcpReceiver(4299);
}

ON_APPLICATION_ENDS() {
    DEBUG_FUNCTION_LINE("Stop wiiload thread");
    if (thread != nullptr) {
        delete thread;
        thread = nullptr;
    }

    deinitLogging();
}