#include <algorithm>
#include <string>
#include <vector>
#include <string.h>
#include <zlib.h>
#include <sysapp/launch.h>

#include <coreinit/dynload.h>
#include <coreinit/title.h>

#include <coreinit/messagequeue.h>
#include <coreinit/ios.h>

#include "TcpReceiver.h"
#include "fs/FSUtils.h"
#include "utils/net.h"
#include "utils/utils.h"
#include <wups_backend/PluginUtils.h>
#include <coreinit/debug.h>
#include <coreinit/cache.h>
#include <rpxloader.h>

#define RPX_TEMP_PATH "fs:/vol/external01/wiiu/apps/"
#define RPX_TEMP_FILE "fs:/vol/external01/wiiu/apps/temp.rpx"
#define WUHB_TEMP_FILE "fs:/vol/external01/wiiu/apps/temp.wuhb"
#define WUHB_TEMP_FILE_2 "fs:/vol/external01/wiiu/apps/temp2.wuhb"
#define RPX_TEMP_FILE_EX "wiiu/apps/temp.rpx"
#define WUHB_TEMP_FILE_EX "wiiu/apps/temp.wuhb"
#define WUHB_TEMP_FILE_2_EX "wiiu/apps/temp2.wuhb"

extern "C" {
uint64_t _SYSGetSystemApplicationTitleId(int32_t);
void _SYSLaunchTitleWithStdArgsInNoSplash(uint64_t, uint32_t);
}

TcpReceiver::TcpReceiver(int32_t port)
        : CThread(CThread::eAttributeAffCore1, 16,0x20000), exitRequested(false), serverPort(port), serverSocket(-1) {

    resumeThread();
}

TcpReceiver::~TcpReceiver() {
    exitRequested = true;

    if (serverSocket >= 0) {
        shutdown(serverSocket, SHUT_RDWR);
    }
}

#define wiiu_geterrno()  (socketlasterr())

void TcpReceiver::executeThread() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (serverSocket < 0) {
        return;
    }

    uint32_t enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = serverPort;
    bindAddress.sin_addr.s_addr = INADDR_ANY;

    socklen_t len;
    int32_t ret;
    if ((ret = bind(serverSocket, (struct sockaddr *) &bindAddress, 16)) < 0) {
        close(serverSocket);
        return;
    }

    if ((ret = listen(serverSocket, 1)) < 0) {
        close(serverSocket);
        return;
    }

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    int32_t addrlen = sizeof(struct sockaddr);

    while (!exitRequested) {
        len = 16;
        int32_t clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &len);
        if (clientSocket >= 0) {
            uint32_t ipAddress = clientAddr.sin_addr.s_addr;
            //serverReceiveStart(this, ipAddress);
            int32_t result = loadToMemory(clientSocket, ipAddress);
            //serverReceiveFinished(this, ipAddress, result);
            close(clientSocket);

            if (result > 0)
            if (result >= 0){
                break;
            }
        } else {
            DEBUG_FUNCTION_LINE("Server socket accept failed %i %d", clientSocket, errno);
            OSSleepTicks(OSMicrosecondsToTicks(100000));
        }
    }

    close(serverSocket);
}


extern bool gDoRelaunch;
int32_t TcpReceiver::loadToMemory(int32_t clientSocket, uint32_t ipAddress) {
    DEBUG_FUNCTION_LINE("Loading file from ip %08X", ipAddress);

    uint32_t fileSize = 0;
    uint32_t fileSizeUnc = 0;
    unsigned char haxx[8];
    memset(haxx, 0, sizeof(haxx));
    //skip haxx
    recvwait(clientSocket, haxx, sizeof(haxx));
    recvwait(clientSocket, (unsigned char *) &fileSize, sizeof(fileSize));

    if (haxx[4] > 0 || haxx[5] > 4) {
        recvwait(clientSocket, (unsigned char *) &fileSizeUnc, sizeof(fileSizeUnc)); // Compressed protocol, read another 4 bytes
    }

    struct in_addr in;
    uint32_t bytesRead = 0;
    in.s_addr = ipAddress;

    DEBUG_FUNCTION_LINE("transfer start");

    unsigned char *loadAddress = (unsigned char *) memalign(0x40, fileSize);
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
            DEBUG_FUNCTION_LINE("Failure on reading file");
            break;
        }

        bytesRead += ret;
    }

    if (bytesRead != fileSize) {
        free(loadAddress);
        DEBUG_FUNCTION_LINE("File loading not finished, %i of %i bytes received", bytesRead, fileSize);
        return FILE_READ_ERROR;
    }

    bool res = false;
    bool loadedRPX = false;
    const char *file_path = nullptr;

    // Do we need to unzip this thing?
    if (haxx[4] > 0 || haxx[5] > 4) {
        unsigned char *inflatedData = NULL;

        // We need to unzip...
        if (loadAddress[0] == 'P' && loadAddress[1] == 'K' && loadAddress[2] == 0x03 && loadAddress[3] == 0x04) {
            //! TODO:
            //! mhmm this is incorrect, it has to parse the zip

            // Section is compressed, inflate
            inflatedData = (unsigned char *) malloc(fileSizeUnc);
            if (!inflatedData) {
                free(loadAddress);

                return NOT_ENOUGH_MEMORY;
            }

            int32_t ret = 0;
            z_stream s;
            memset(&s, 0, sizeof(s));

            s.zalloc = Z_NULL;
            s.zfree = Z_NULL;
            s.opaque = Z_NULL;

            ret = inflateInit(&s);
            if (ret != Z_OK) {
                free(loadAddress);
                free(inflatedData);

                return FILE_READ_ERROR;
            }

            s.avail_in = fileSize;
            s.next_in = (Bytef *) (&loadAddress[0]);

            s.avail_out = fileSizeUnc;
            s.next_out = (Bytef *) &inflatedData[0];

            ret = inflate(&s, Z_FINISH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
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
                free(loadAddress);

                return NOT_ENOUGH_MEMORY;
            }

            uLongf f = fileSizeUnc;
            int32_t result = uncompress((Bytef *) &inflatedData[0], &f, (Bytef *) loadAddress, fileSize);
            if (result != Z_OK) {
                DEBUG_FUNCTION_LINE("uncompress failed %i", result);

                return FILE_READ_ERROR;
            }

            fileSizeUnc = f;
            fileSize = fileSizeUnc;
        }

        if (memcmp(inflatedData, "WUHB", 4) == 0) {
            DEBUG_FUNCTION_LINE("Try to load a .wuhb");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(WUHB_TEMP_FILE, inflatedData, fileSize);
            file_path = WUHB_TEMP_FILE_EX;
            if(!res){
                // temp.wuhb might be mounted, let's try temp2.wuhb
                res = FSUtils::saveBufferToFile(WUHB_TEMP_FILE_2, inflatedData, fileSize);
                file_path = WUHB_TEMP_FILE_2_EX;
            }

            loadedRPX = true;
        } else if (inflatedData[0x7] == 0xCA && inflatedData[0x8] == 0xFE && inflatedData[0x9] != 0x50 && inflatedData[0xA] != 0x4C) {
            DEBUG_FUNCTION_LINE("Try to load a .rpx");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(RPX_TEMP_FILE, inflatedData, fileSize);
            file_path = RPX_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (inflatedData[0x7] == 0xCA && inflatedData[0x8] == 0xFE && inflatedData[0x9] == 0x50 && inflatedData[0xA] == 0x4C) {
            auto newContainer = PluginUtils::getPluginForBuffer((char *) inflatedData, fileSize);
            if (newContainer) {
                auto oldPlugins = PluginUtils::getLoadedPlugins(8);
                std::vector<PluginContainer> finalList;

                finalList.push_back(newContainer.value());
                for (auto &plugin : oldPlugins) {
                    if (plugin.metaInformation.getName() == newContainer->metaInformation.getName() &&
                        plugin.metaInformation.getAuthor() == newContainer->metaInformation.getAuthor()
                            ) {
                        DEBUG_FUNCTION_LINE("Skipping duplicate");
                        PluginUtils::destroyPluginContainer(plugin);
                        continue;
                    } else {
                        finalList.push_back(plugin);
                    }
                }

                for (auto &plugin : finalList) {
                    DEBUG_FUNCTION_LINE("name: %s", plugin.getMetaInformation().getName().c_str());
                    DEBUG_FUNCTION_LINE("author: %s", plugin.getMetaInformation().getAuthor().c_str());
                    DEBUG_FUNCTION_LINE("handle: %08X", plugin.getPluginData().getHandle());
                    DEBUG_FUNCTION_LINE("====");
                }

                if (PluginUtils::LoadAndLinkOnRestart(finalList) != 0) {
                    DEBUG_FUNCTION_LINE("Failed to load & link");
                } else {
                    //gDoRelaunch = true;
                    //DCFlushRange(&gDoRelaunch, sizeof(gDoRelaunch));
                }
                PluginUtils::destroyPluginContainer(finalList);

                free(loadAddress);
                free(inflatedData);

                _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), 0);
                return fileSize;
            } else {
                DEBUG_FUNCTION_LINE("Failed to parse plugin");
            }
        }
        free(inflatedData);
    } else {
        if (memcmp(loadAddress, "WUHB", 4) == 0) {
            DEBUG_FUNCTION_LINE("Try to load a .wuhb");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(WUHB_TEMP_FILE, loadAddress, fileSize);
            file_path = WUHB_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (loadAddress[0x7] == 0xCA && loadAddress[0x8] == 0xFE) {
            DEBUG_FUNCTION_LINE("Try to load a rpx");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(RPX_TEMP_FILE, loadAddress, fileSize);
            file_path = RPX_TEMP_FILE_EX;
            loadedRPX = true;
        } else if (loadAddress[0x7] == 0xCA && loadAddress[0x8] == 0xFE && loadAddress[0x9] == 0x50) {
            OSFatal("Not implemented yet");
        }
    }

    free(loadAddress);

    if (!res) {
        return NOT_ENOUGH_MEMORY;
    }

    if (loadedRPX) {
        DEBUG_FUNCTION_LINE("Starting a homebrew title!");
        RL_LoadFromSDOnNextLaunch(file_path);

        uint64_t titleID = _SYSGetSystemApplicationTitleId(8);
        _SYSLaunchTitleWithStdArgsInNoSplash(titleID, 0);
        return fileSize;
    }

    SYSRelaunchTitle(0, nullptr);

    return fileSize;
}
