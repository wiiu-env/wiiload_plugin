#include "config.h"
#include "globals.h"
#include "utils/TcpReceiver.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

#include <string>

static void gServerEnabledChanged(ConfigItemBoolean *item, bool newValue) {
    if (std::string_view(WIILOAD_ENABLED_STRING) != item->identifier) {
        DEBUG_FUNCTION_LINE_WARN("Unexpected identifier in bool callback: %s", item->identifier);
        return;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gWiiloadServerEnabled: %d", newValue);
    gWiiloadServerEnabled = newValue;

    gTcpReceiverThread.reset();

    if (gWiiloadServerEnabled) {
        DEBUG_FUNCTION_LINE("Starting server!");
        gTcpReceiverThread = make_unique_nothrow<TcpReceiver>(4299);
        if (gTcpReceiverThread == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to create wiiload thread");
        }
    } else {
        DEBUG_FUNCTION_LINE("Wiiload server has been stopped!");
    }
    // If the value has changed, we store it in the storage.
    WUPSStorageError res;
    if ((res = WUPSStorageAPI::Store(item->identifier, gWiiloadServerEnabled)) != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store gWiiloadServerEnabled: %s (%d)", WUPSStorageAPI_GetStatusStr(res), res);
    }
}

static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

        root.add(WUPSConfigItemBoolean::Create(WIILOAD_ENABLED_STRING, "Enable Wiiload",
                                               DEFAULT_WIILOAD_ENABLED_VALUE, gWiiloadServerEnabled,
                                               &gServerEnabledChanged));

    } catch (std::exception &e) {
        OSReport("Exception: %s\n", e.what());
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

void InitConfigAndStorage() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = "Wiiload Plugin"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init config api");
    }

    if (WUPSStorageAPI::GetOrStoreDefault(WIILOAD_ENABLED_STRING, gWiiloadServerEnabled, DEFAULT_WIILOAD_ENABLED_VALUE) != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create item \"%s\"", WIILOAD_ENABLED_STRING);
    }

    if (WUPSStorageAPI::SaveStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to save storage");
    }
}