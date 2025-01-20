#include "main.h"
#include "config.h"
#include "globals.h"
#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <notifications/notifications.h>
#include <rpxloader/rpxloader.h>

#include <coreinit/debug.h>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups_backend/api.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION(VERSION_FULL);
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL3");

WUPS_USE_WUT_DEVOPTAB();

WUPS_USE_STORAGE("wiiload"); // Unique id for the storage api

INITIALIZE_PLUGIN() {
    RPXLoaderStatus error;
    if ((error = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("WiiLoad Plugin: Failed to init RPXLoader. Error %d", error);
    } else {
        gLibRPXLoaderInitDone = true;
    }
    gTcpReceiverThread.reset();

    NotificationModuleStatus res;
    if ((res = NotificationModule_InitLibrary()) != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init NotificationModule: %s", NotificationModule_GetStatusStr(res));
    } else {
        NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_ERROR, NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, 10.0f);
    }
    PluginBackendApiErrorType res2;
    if ((res2 = WUPSBackend_InitLibrary()) != PLUGIN_BACKEND_API_ERROR_NONE) {
        DEBUG_FUNCTION_LINE_WARN("Failed to init WUPSBackend Api: %s", WUPSBackend_GetStatusStr(res2));
    }

    InitConfigAndStorage();
}

DEINITIALIZE_PLUGIN() {
    RPXLoader_DeInitLibrary();
    NotificationModule_DeInitLibrary();
    WUPSBackend_DeInitLibrary();
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();
    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Start wiiload thread");
        gTcpReceiverThread = make_unique_nothrow<TcpReceiver>(4299);
        if (gTcpReceiverThread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server is disabled");
    }
}

ON_APPLICATION_ENDS() {
    gTcpReceiverThread.reset();
    deinitLogging();
}