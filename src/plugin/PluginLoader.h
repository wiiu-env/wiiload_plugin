#ifndef _PLUGIN_LOADER_H_
#define _PLUGIN_LOADER_H_

#include <vector>
#include "PluginInformation.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <utils/utils.h>

#ifdef __cplusplus
}
#endif


class PluginLoader {

public:
    static PluginLoader * createInstance(uint32_t startAddress, uint32_t endAddress);

    static void destroyInstance(PluginLoader * loader);

    ~PluginLoader();

    /**
        \brief  Takes a list of plugins that should be linked (relocated) loaded into the memory.
                The function that should be replaced will be replaced in the order of the given plugin list.
                So two plugin will override the same function, the plugin first in this list will override the function first.
                Also the hooks of the plugins will be called in the order their plugin where passed to this method.

        \param A list of plugin that should be linked (relocated) an loaded into memory

        \return Returns true if all plugins were linked successfully. Returns false if at least one plugin failed while linking.
    **/
    bool loadAndLinkPlugins(std::vector<PluginInformation *> pluginInformation);



    /**
        \brief Load

        \param pluginInformation a PluginInformation object of the plugin that should be linked (relocated) and loaded.
    **/
    bool loadAndLinkPlugin(PluginInformation * pluginInformation);


    /*
    size_t getTotalSpace() {
        return ((uint32_t) this->endAddress - (uint32_t) this->startAddress);
    }

    size_t getAvailableSpace() {
        return ((uint32_t) this->endAddress - (uint32_t) this->currentStoreAddress);
    }

    size_t getUsedSpace() {
        return getTotalSpace() - getAvailableSpace();
    }

    void resetPluginLoader() {
        this->currentStoreAddress = ROUNDUP((uint32_t)startAddress, 0x10000);
    }*/
private:
    PluginLoader(plugin_loader_handle handle, uint32_t startAddress, uint32_t endAddress);

    static std::vector<PluginInformation *> getPluginInformationByStruct(plugin_information_handle * handleList, uint32_t  handleListSize);

    plugin_loader_handle handle = 0;

    uint32_t startAddress = 0;
    uint32_t endAddress = 0;
    uint32_t currentStoreAddress = 0;
};


#endif
