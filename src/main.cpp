#include <wups.h>
#include "utils/TcpReceiver.h"
#include <whb/log_udp.h>
#include <coreinit/cache.h>
#include <sysapp/launch.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_CRT()

TcpReceiver *thread = NULL;

/* Entry point */
ON_APPLICATION_START(args) {
    WHBLogUdpInit();
    DEBUG_FUNCTION_LINE("Started wiiload thread");
    thread = new TcpReceiver(4299);
}

void stopThread() {
    if (thread != NULL) {
        delete thread;
        thread = NULL;
    }
}

ON_APPLICATION_END() {
    DEBUG_FUNCTION_LINE("Kill thread");
    stopThread();
}

bool gDoRelaunch __attribute__((section(".data"))) = 0;

ON_VYSNC() {
    // On each frame check if we want to exit.
    if(gDoRelaunch){
        SYSRelaunchTitle(0, NULL);
        gDoRelaunch = 0;
        DCFlushRange(&gDoRelaunch, sizeof(gDoRelaunch));
    }
}
