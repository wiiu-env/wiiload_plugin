#include "TcpReceiver.h"
#include "fs/FSUtils.h"
#include "globals.h"
#include "utils/net.h"
#include "utils/utils.h"
#include <algorithm>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/title.h>
#include <cstring>
#include <rpxloader/rpxloader.h>
#include <sysapp/launch.h>
#include <vector>
#include <wups_backend/PluginUtils.h>
#include <zlib.h>

#define RPX_TEMP_PATH       "fs:/vol/external01/wiiu/apps/"
#define RPX_TEMP_FILE       "fs:/vol/external01/wiiu/apps/temp.rpx"
#define WUHB_TEMP_FILE      "fs:/vol/external01/wiiu/apps/temp.wuhb"
#define WUHB_TEMP_FILE_2    "fs:/vol/external01/wiiu/apps/temp2.wuhb"
#define RPX_TEMP_FILE_EX    "wiiu/apps/temp.rpx"
#define WUHB_TEMP_FILE_EX   "wiiu/apps/temp.wuhb"
#define WUHB_TEMP_FILE_2_EX "wiiu/apps/temp2.wuhb"

TcpReceiver::TcpReceiver(int32_t port)
    : CThread(CThread::eAttributeAffCore1, 16, 0x20000, nullptr, nullptr, "Wiiload Thread"), exitRequested(false), serverPort(port), serverSocket(-1) {
    resumeThread();
}

TcpReceiver::~TcpReceiver() {
    exitRequested = true;
    OSMemoryBarrier();
    if (serverSocket >= 0) {
        cleanupSocket();
    }
}

bool TcpReceiver::createSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (serverSocket < 0) {
        return false;
    }

    uint32_t enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in bindAddress {};
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family      = AF_INET;
    bindAddress.sin_port        = serverPort;
    bindAddress.sin_addr.s_addr = INADDR_ANY;


    int32_t ret;
    if ((ret = bind(serverSocket, (struct sockaddr *) &bindAddress, 16)) < 0) {
        cleanupSocket();
        return false;
    }

    if ((ret = listen(serverSocket, 1)) < 0) {
        cleanupSocket();
        return false;
    }
    return true;
}

void TcpReceiver::cleanupSocket() {
    if (serverSocket >= 0) {
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }
}

void TcpReceiver::executeThread() {
    socklen_t len;
    while (!exitRequested) {
        if (serverSocket < 0) {
            if (!createSocket() && !exitRequested) {
                if (errno != EBUSY) {
                    DEBUG_FUNCTION_LINE_WARN("Create socket failed %d", errno);
                }
                OSSleepTicks(OSMillisecondsToTicks(10));
            }
            continue;
        }
        struct sockaddr_in clientAddr {};
        memset(&clientAddr, 0, sizeof(clientAddr));

        len                  = 16;
        int32_t clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &len);
        if (clientSocket >= 0) {
            uint32_t ipAddress = clientAddr.sin_addr.s_addr;
            DEBUG_FUNCTION_LINE("Waiting for wiiload connection");
            int32_t result = loadToMemory(clientSocket, ipAddress);
            close(clientSocket);

            if (result >= 0) {
                break;
            }
        } else if (!exitRequested) {
            if (errno != EBUSY) {
                DEBUG_FUNCTION_LINE_WARN("Accept failed %d", errno);
                cleanupSocket();
            }
            OSSleepTicks(OSMillisecondsToTicks(10));
        }
        OSSleepTicks(OSMillisecondsToTicks(1));
    }
    cleanupSocket();
    DEBUG_FUNCTION_LINE("Stopping wiiload server.");
}

int32_t TcpReceiver::loadToMemory(int32_t clientSocket, uint32_t ipAddress) {
    DEBUG_FUNCTION_LINE("Loading file from ip %08X", ipAddress);

    uint32_t fileSize    = 0;
    uint32_t fileSizeUnc = 0;
    unsigned char haxx[8];
    memset(haxx, 0, sizeof(haxx));
    //skip haxx
    recvwait(clientSocket, haxx, sizeof(haxx));
    recvwait(clientSocket, (unsigned char *) &fileSize, sizeof(fileSize));

    if (haxx[4] > 0 || haxx[5] > 4) {
        recvwait(clientSocket, (unsigned char *) &fileSizeUnc, sizeof(fileSizeUnc)); // Compressed protocol, read another 4 bytes
    }

    uint32_t bytesRead = 0;

    auto *loadAddress = (unsigned char *) memalign(0x40, fileSize);
    if (!loadAddress) {
        OSSleepTicks(OSSecondsToTicks(1));
        return NOT_ENOUGH_MEMORY;
    }

    // Copy rpl in memory
    while (bytesRead < fileSize) {
        uint32_t blockSize = 0x1000;
        if (blockSize > (fileSize - bytesRead))
            blockSize = fileSize - bytesRead;

        int32_t ret = recv(clientSocket, loadAddress + bytesRead, blockSize, 0);
        if (ret <= 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to receive file");
            break;
        }

        bytesRead += ret;
    }

    if (bytesRead != fileSize) {
        free(loadAddress);
        DEBUG_FUNCTION_LINE_ERR("File loading not finished, %i of %i bytes received", bytesRead, fileSize);
        return FILE_READ_ERROR;
    }

    bool res              = false;
    bool loadedRPX        = false;
    const char *file_path = nullptr;

    // Do we need to unzip this thing?
    if (haxx[4] > 0 || haxx[5] > 4) {
        unsigned char *inflatedData;

        // We need to unzip...
        if (loadAddress[0] == 'P' && loadAddress[1] == 'K' && loadAddress[2] == 0x03 && loadAddress[3] == 0x04) {
            //! TODO:
            //! mhmm this is incorrect, it has to parse the zip

            // Section is compressed, inflate
            inflatedData = (unsigned char *) malloc(fileSizeUnc);
            if (!inflatedData) {
                DEBUG_FUNCTION_LINE_ERR("Failed to malloc data");
                free(loadAddress);

                return NOT_ENOUGH_MEMORY;
            }

            int32_t ret = 0;
            z_stream s;
            memset(&s, 0, sizeof(s));

            s.zalloc = Z_NULL;
            s.zfree  = Z_NULL;
            s.opaque = Z_NULL;

            ret = inflateInit(&s);
            if (ret != Z_OK) {
                DEBUG_FUNCTION_LINE_ERR("inflateInit failed %i", ret);
                free(loadAddress);
                free(inflatedData);

                return FILE_READ_ERROR;
            }

            s.avail_in = fileSize;
            s.next_in  = (Bytef *) (&loadAddress[0]);

            s.avail_out = fileSizeUnc;
            s.next_out  = (Bytef *) &inflatedData[0];

            ret = inflate(&s, Z_FINISH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                DEBUG_FUNCTION_LINE_ERR("inflate failed %i", ret);
                free(loadAddress);
                free(inflatedData);

                return FILE_READ_ERROR;
            }

            inflateEnd(&s);
            fileSize = fileSizeUnc;
        } else {
            // Section is compressed, inflate
            inflatedData = (unsigned char *) malloc(fileSizeUnc);
            if (!inflatedData) {
                DEBUG_FUNCTION_LINE_ERR("malloc failed");
                free(loadAddress);

                return NOT_ENOUGH_MEMORY;
            }

            uLongf f       = fileSizeUnc;
            int32_t result = uncompress((Bytef *) &inflatedData[0], &f, (Bytef *) loadAddress, fileSize);
            if (result != Z_OK) {
                DEBUG_FUNCTION_LINE_ERR("uncompress failed %i", result);
                return FILE_READ_ERROR;
            }

            fileSizeUnc = f;
            fileSize    = fileSizeUnc;
        }

        if (memcmp(inflatedData, "WUHB", 4) == 0) {
            DEBUG_FUNCTION_LINE("Try to load a .wuhb");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res       = FSUtils::saveBufferToFile(WUHB_TEMP_FILE, inflatedData, fileSize);
            file_path = WUHB_TEMP_FILE_EX;
            if (!res) {
                // temp.wuhb might be mounted, let's try temp2.wuhb
                res       = FSUtils::saveBufferToFile(WUHB_TEMP_FILE_2, inflatedData, fileSize);
                file_path = WUHB_TEMP_FILE_2_EX;
            }

            loadedRPX = true;
        } else if (inflatedData[0x7] == 0xCA && inflatedData[0x8] == 0xFE && inflatedData[0x9] != 0x50 && inflatedData[0xA] != 0x4C) {
            DEBUG_FUNCTION_LINE("Try to load a .rpx");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res       = FSUtils::saveBufferToFile(RPX_TEMP_FILE, inflatedData, fileSize);
            file_path = RPX_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (inflatedData[0x7] == 0xCA && inflatedData[0x8] == 0xFE && inflatedData[0x9] == 0x50 && inflatedData[0xA] == 0x4C) {
            auto newContainer = PluginUtils::getPluginForBuffer((char *) inflatedData, fileSize);
            if (newContainer) {
                auto plugins = PluginUtils::getLoadedPlugins(32);

                auto &metaInformation = newContainer.value()->metaInformation;

                // remove plugins with the same name and author as our new plugin

                plugins.erase(std::remove_if(plugins.begin(), plugins.end(),
                                             [metaInformation](auto &plugin) {
                                                 return plugin->metaInformation->getName() == metaInformation->getName() &&
                                                        plugin->metaInformation->getAuthor() == metaInformation->getAuthor();
                                             }),
                              plugins.end());

                // at the new plugin
                plugins.push_back(std::move(newContainer.value()));

#ifdef VERBOSE_DEBUG
                for (auto &plugin : plugins) {
                    DEBUG_FUNCTION_LINE_VERBOSE("name: %s", plugin->getMetaInformation()->getName().c_str());
                    DEBUG_FUNCTION_LINE_VERBOSE("author: %s", plugin->getMetaInformation()->getAuthor().c_str());
                    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", plugin->getPluginData()->getHandle());
                    DEBUG_FUNCTION_LINE_VERBOSE("====");
                }
#endif

                if (PluginUtils::LoadAndLinkOnRestart(plugins) != 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to load & link");
                }

                free(loadAddress);
                free(inflatedData);

                _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
                return fileSize;
            } else {
                DEBUG_FUNCTION_LINE_ERR("Failed to parse plugin");
            }
        }
        free(inflatedData);
    } else {
        if (memcmp(loadAddress, "WUHB", 4) == 0) {
            DEBUG_FUNCTION_LINE("Try to load a .wuhb");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res       = FSUtils::saveBufferToFile(WUHB_TEMP_FILE, loadAddress, fileSize);
            file_path = WUHB_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (loadAddress[0x7] == 0xCA && loadAddress[0x8] == 0xFE) {
            DEBUG_FUNCTION_LINE("Try to load a rpx");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res       = FSUtils::saveBufferToFile(RPX_TEMP_FILE, loadAddress, fileSize);
            file_path = RPX_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (loadAddress[0x7] == 0xCA && loadAddress[0x8] == 0xFE && loadAddress[0x9] == 0x50) {
            OSFatal("Not implemented yet");
        }
    }

    free(loadAddress);

    if (!res) {
        DEBUG_FUNCTION_LINE_ERR("Failed to launch save a homebrew to the sd card");
        return NOT_ENOUGH_MEMORY;
    }

    if (loadedRPX) {
        if (!gLibRPXLoaderInitDone) {
            DEBUG_FUNCTION_LINE_ERR("RPXLoaderModule missing, failed to launch homebrew.");
            return NOT_SUPPORTED;
        }
        RPXLoaderStatus launchRes;
        if ((launchRes = RPXLoader_LaunchHomebrew(file_path)) != RPX_LOADER_RESULT_SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("Failed to start %s %s", file_path, RPXLoader_GetStatusStr(launchRes));
            return NOT_ENOUGH_MEMORY;
        }

        return fileSize;
    }

    SYSRelaunchTitle(0, nullptr);

    return fileSize;
}
