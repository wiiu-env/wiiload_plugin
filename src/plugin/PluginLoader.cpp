#include <string>
#include "PluginLoader.h"
#include "utils/ipcclient.h"

PluginLoader * PluginLoader::createInstance(uint32_t startAddress, uint32_t endAddress) {
    plugin_loader_handle handle = IPC_Open_Plugin_Loader(startAddress, endAddress);

    if(handle != 0) {
        return new PluginLoader(handle, startAddress,endAddress);
    }
    return NULL;
}

void PluginLoader::destroyInstance(PluginLoader * loader) {
    if(loader != NULL) {
        delete loader;
    }
}

PluginLoader::PluginLoader(plugin_information_handle handle,uint32_t startAddress, uint32_t endAddress) {
    this->handle = handle;
    this->startAddress = startAddress;
    this->endAddress = endAddress;
}

PluginLoader::~PluginLoader() {
    IPC_Close_Plugin_Loader(this->handle);
}

bool PluginLoader::loadAndLinkPlugins(std::vector<PluginInformation *> pluginInformation) {
    uint32_t  handleListSize = pluginInformation.size();

    DEBUG_FUNCTION_LINE("Convert PluginInformation* to plugin_information_handle *\n");

    plugin_information_handle * handleList = (plugin_information_handle *) malloc(handleListSize * sizeof(plugin_information_handle));
    if(handleList == NULL) {
        return false;
    }

    DEBUG_FUNCTION_LINE("Allocation was okay %08X\n", handleList);


    uint32_t cur = 0;
    for (std::vector<PluginInformation *>::iterator it = pluginInformation.begin() ; it != pluginInformation.end(); ++it) {
        PluginInformation * curPlugin = *it;
        handleList[cur] = curPlugin->getHandle();
        DEBUG_FUNCTION_LINE("Adding to List %08X\n", handleList[cur]);
        cur++;
    }
    bool result = false;
    int32_t  res = IPC_Link_Plugin_Information(this->handle, handleList, handleListSize);

    if(res >= 0) {
        DEBUG_FUNCTION_LINE("result was %d\n", res);
        result = true;
    }

    free(handleList);
    return result;
}
