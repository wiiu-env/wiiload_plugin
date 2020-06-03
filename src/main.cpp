#include <wups.h>
#include "utils/TcpReceiver.h"
#include <whb/libmanager.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_CRT()

TcpReceiver *thread = NULL;

/* Entry point */
ON_APPLICATION_START(args) {
    WHBInitializeSocketLibrary();

    log_init();
    DEBUG_FUNCTION_LINE("Started wiiload thread\n");
    thread = new TcpReceiver(4299);
}

void stopThread() {
    if (thread != NULL) {
        delete thread;
        thread = NULL;
    }
}


ON_APPLICATION_END() {
    DEBUG_FUNCTION_LINE("Kill thread\n");
    stopThread();

    DEBUG_FUNCTION_LINE("Unmount SD\n");
}

