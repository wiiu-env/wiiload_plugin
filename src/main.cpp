#include "main.h"
#include "globals.h"
#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include <coreinit/debug.h>
#include <notifications/notifications.h>
#include <rpxloader/rpxloader.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>

WUPS_PLUGIN_NAME("Wiiload");
WUPS_PLUGIN_DESCRIPTION("Wiiload Server");
WUPS_PLUGIN_VERSION(VERSION_FULL);
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB();

WUPS_USE_STORAGE("wiiload"); // Unqiue id for the storage api
#define WIILOAD_ENABLED_STRING "enabled"

std::unique_ptr<TcpReceiver> tcpReceiverThread = nullptr;


static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle);
static void ConfigMenuClosedCallback();

INITIALIZE_PLUGIN() {
    RPXLoaderStatus error;
    if ((error = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("WiiLoad Plugin: Failed to init RPXLoader. Error %d", error);
    } else {
        gLibRPXLoaderInitDone = true;
    }

    NotificationModuleStatus res;
    if ((res = NotificationModule_InitLibrary()) != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init NotificationModule: %s", NotificationModule_GetStatusStr(res));
        gNotificationModuleLoaded = false;
    } else {
        NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_ERROR, NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, 10.0f);
        gNotificationModuleLoaded = true;
    }

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Wiiload Plugin"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init config api");
    }

    if (WUPSStorageAPI::GetOrStoreDefault(WIILOAD_ENABLED_STRING, gWiiloadServerEnabled, true) != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create item \"%s\"", WIILOAD_ENABLED_STRING);
    }

    if (WUPSStorageAPI::SaveStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to save storage");
    }
}

void gServerEnabledChanged(ConfigItemBoolean *item, bool newValue) {
    if (std::string_view(WIILOAD_ENABLED_STRING) != item->identifier) {
        DEBUG_FUNCTION_LINE_WARN("Unexpected identifier in bool callback: %s", item->identifier);
        return;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gWiiloadServerEnabled: %d", newValue);
    gWiiloadServerEnabled = newValue;

    tcpReceiverThread.reset();

    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Starting server!");
        tcpReceiverThread = std::make_unique<TcpReceiver>(4299);

        if (tcpReceiverThread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server has been stopped!");
    }
    // If the value has changed, we store it in the storage.
    auto res = WUPSStorageAPI::Store(item->identifier, gWiiloadServerEnabled);
    if (res != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store gWiiloadServerEnabled: %s", WUPSStorageAPI_GetStatusStr(res));
    }
}

static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

        root.add(WUPSConfigItemBoolean::Create(WIILOAD_ENABLED_STRING, "Enable Wiiload",
                                               true, gWiiloadServerEnabled,
                                               &gServerEnabledChanged));

    } catch (std::exception &e) {
        OSReport("Exception T_T : %s\n", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void ConfigMenuClosedCallback() {
    // Save all changes
    if (WUPSStorageAPI::SaveStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to close storage");
    }
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();
    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Start wiiload thread");
        tcpReceiverThread = std::make_unique<TcpReceiver>(4299);
        if (tcpReceiverThread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server is disabled");
    }
}

ON_APPLICATION_ENDS() {
    tcpReceiverThread.reset();
    deinitLogging();
}