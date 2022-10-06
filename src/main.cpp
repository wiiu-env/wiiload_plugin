#include "main.h"
#include "globals.h"
#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include <coreinit/debug.h>
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

void gServerEnabledChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gWiiloadServerEnabled: %d", newValue);
    gWiiloadServerEnabled = newValue;
    if (thread) {
        delete thread;
        thread = nullptr;
    }
    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Starting server!");
        thread = new (std::nothrow) TcpReceiver(4299);
        if (thread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server has been stopped!");
    }
    // If the value has changed, we store it in the storage.
    auto res = WUPS_StoreInt(nullptr, item->configId, gWiiloadServerEnabled);
    if (res != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store gWiiloadServerEnabled: %s (%d)", WUPS_GetStorageStatusStr(res), res);
    }
}

WUPS_GET_CONFIG() {
    // We open the storage, so we can persist the configuration the user did.
    if (WUPS_OpenStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to open storage");
        return 0;
    }

    WUPSConfigHandle config;
    WUPSConfig_CreateHandled(&config, "Wiiload");

    WUPSConfigCategoryHandle setting;
    WUPSConfig_AddCategoryByNameHandled(config, "Settings", &setting);
    WUPSConfigItemBoolean_AddToCategoryHandled(config, setting, WIILOAD_ENABLED_STRING, "Enable Wiiload", gWiiloadServerEnabled, &gServerEnabledChanged);

    return config;
}

WUPS_CONFIG_CLOSED() {
    // Save all changes
    if (WUPS_CloseStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to close storage");
    }
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();
    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Start wiiload thread");
        thread = new (std::nothrow) TcpReceiver(4299);
        if (thread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server is disabled");
    }
}

ON_APPLICATION_ENDS() {
    DEBUG_FUNCTION_LINE("Stop wiiload thread!");
    if (thread != nullptr) {
        delete thread;
        thread = nullptr;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Done!");
    deinitLogging();
}