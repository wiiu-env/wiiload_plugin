#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include <wups.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB();

TcpReceiver *thread = nullptr;

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