/* based on module.c
 *   by Alex Chadwick
 *
 * Copyright (C) 2014, Alex Chadwick
 * Modified 2018,2019 Maschell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _PLUGIN_INFORMATION_UTILS_H_
#define _PLUGIN_INFORMATION_UTILS_H_

#include <string>
#include <vector>
#include "PluginInformation.h"
#include "utils/logger.h"
#include "utils/ipcclient.h"

class PluginInformationUtils {
public:


    static PluginInformation * loadPluginInformation(std::string path);


    /**
        \brief Parses the meta data of all plugins in the given directory.

        \param path the path of the directory which should be scanned.

        \return a list of PluginInformation objects, one for each valid plugin.
    **/
    static std::vector<PluginInformation *> getPluginsByPath(std::string path);


    /**
        \brief Gets plugin information from the global struct.

        \return a list of MetaInformation objects for all plugins currently loaded and linked (relocated). Will only contain
                plugin which are still on the sd card.
    **/
    static std::vector<PluginInformation *> getPluginsLoadedInMemory();


    static void clearPluginInformation(std::vector<PluginInformation *> pluginInformation) ;


    static bool loadAndLinkPluginsOnRestart(std::vector<PluginInformation *> pluginInformation);

    static std::vector<PluginInformation *> getPluginInformationByStruct(plugin_information_handle * handleList, uint32_t  handleListSize);
};


#endif
