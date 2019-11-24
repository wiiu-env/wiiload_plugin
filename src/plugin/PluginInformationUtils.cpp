#include <string>
#include "PluginInformationUtils.h"
#include "utils/ipcclient.h"

std::vector<PluginInformation *> PluginInformationUtils::getPluginInformationByStruct(plugin_information_handle * handleList, uint32_t  handleListSize) {
    std::vector<PluginInformation *> result;

    if(handleListSize > 0) {
        DEBUG_FUNCTION_LINE("Getting details for handles\n");
        plugin_information * informationList = NULL;
        uint32_t informationListSize = 0;
        uint32_t res =  IPC_Get_Plugin_Information_Details(handleList, handleListSize, &informationList, &informationListSize);
        if(res == 0) {
            for(uint32_t i = 0; i<informationListSize; i++) {
                DEBUG_FUNCTION_LINE("Adding %08X %s\n", informationList[i].handle, informationList[i].path);
                result.push_back(new PluginInformation(informationList[i].handle,informationList[i].path,informationList[i].name,informationList[i].author));
            }
        } else {
            DEBUG_FUNCTION_LINE("IPC_Get_Plugin_Information_Details failed\n");
        }
        if(informationList != NULL) {
            free(informationList);
        }
    } else {
        DEBUG_FUNCTION_LINE("List is empty.\n");
    }

    return result;
}


void PluginInformationUtils::clearPluginInformation(std::vector<PluginInformation *> pluginInformation) {
    for(size_t i = 0; i < pluginInformation.size(); i++) {
        PluginInformation * curPluginInformation = pluginInformation[i];
        if(curPluginInformation != NULL) {
            delete curPluginInformation;
        }
    }
}

std::vector<PluginInformation *> PluginInformationUtils::getPluginsByPath(std::string path) {
    std::vector<PluginInformation *> result;
    plugin_information_handle * handleList = NULL;
    uint32_t  handleListSize = 0;

    uint32_t res = IPC_Get_Plugin_Information(path.c_str(), &handleList, &handleListSize);
    if(res == 0) {
        DEBUG_FUNCTION_LINE("SUCCESS reading plugins from %s. handleListSize %d, handlelist %08X \n",path, handleListSize, handleList);
        result = getPluginInformationByStruct(handleList, handleListSize);
    }

    if(handleList != NULL) {
        free(handleList);
    }
    return result;
}

PluginInformation * PluginInformationUtils::loadPluginInformation(std::string path) {
    std::vector<PluginInformation *> result;
    plugin_information_handle handle = NULL;

    uint32_t res = IPC_Get_Plugin_Information_For_Filepath(path.c_str(), &handle);
    if(res == 0 && handle != NULL) {
        DEBUG_FUNCTION_LINE("SUCCESS reading plugins from %s. handle %08X \n",path.c_str(), &handle);
        result = getPluginInformationByStruct(&handle, 1);
    }

    if(result.size() > 0){
        return result.at(0);
    }
    return NULL;
}


bool PluginInformationUtils::loadAndLinkPluginsOnRestart(std::vector<PluginInformation *> pluginInformation) {
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
    int32_t  res = IPC_Link_Plugin_Information_On_Restart(handleList, handleListSize);

    if(res >= 0) {
        DEBUG_FUNCTION_LINE("result was %d\n", res);
        result = true;
    }

    free(handleList);
    return result;
}

std::vector<PluginInformation *> PluginInformationUtils::getPluginsLoadedInMemory() {
    std::vector<PluginInformation *> result;
    plugin_information_handle * handleList = NULL;
    uint32_t  handleListSize = 0;

    uint32_t res = IPC_Get_Plugin_Information_Loaded(&handleList, &handleListSize);
    if(res == 0) {
        result = getPluginInformationByStruct(handleList, handleListSize);
    }

    if(handleList != NULL) {
        free(handleList);
    }
    return result;
}
