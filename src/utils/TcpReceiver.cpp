#include "TcpReceiver.h"
#include "FSUtils.h"
#include "globals.h"
#include "utils/net.h"
#include "utils/utils.h"
#include <algorithm>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <coreinit/title.h>
#include <cstring>
#include <notifications/notifications.h>
#include <rpxloader/rpxloader.h>
#include <sysapp/launch.h>
#include <vector>
#include <wups_backend/PluginUtils.h>
#include <zlib.h>

#define APPS_TEMP_PATH      "fs:/vol/external01/wiiu/apps/"
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
        DEBUG_FUNCTION_LINE_WARN("Failed to bind socket: %d errno: %d", ret, errno);
        cleanupSocket();
        return false;
    }

    if ((ret = listen(serverSocket, 1)) < 0) {
        DEBUG_FUNCTION_LINE_WARN("Failed to bind socket: %d errno: %d", ret, errno);
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
        struct sockaddr_in clientAddr = {};
        len                           = 16;
        int32_t clientSocket          = accept(serverSocket, (struct sockaddr *) &clientAddr, &len);
        if (clientSocket >= 0) {
            uint32_t ipAddress = clientAddr.sin_addr.s_addr;
            DEBUG_FUNCTION_LINE("Waiting for wiiload connection");
            auto result = loadToMemory(clientSocket, ipAddress);
            close(clientSocket);

            switch (result) {
                case SUCCESS:
                    break;
                case FILE_UNCOMPRESS_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to decrompress recieved data. Launching will be aborted.");
                    break;
                case NOT_ENOUGH_MEMORY:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Not enough memory. Launching will be aborted.");
                    break;
                case RECV_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to receive data. Launching will be aborted.");
                    break;
                case UNSUPPORTED_FORMAT:
                    NotificationModule_AddErrorNotification("Wiiload plugin:  Tried to load an unsupported file. Launching will be aborted.");
                    break;
                case PLUGIN_PARSE_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to load plugin. Maybe unsupported version? Launching will be aborted.");
                    break;
                case PLUGIN_LOAD_LINK_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to load plugins. Maybe unsupported version? Launching will be aborted.");
                    break;
                case FILE_SAVE_BUFFER_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to save file to the sd card. Launching will be aborted.");
                    break;
                case LAUNCH_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to launch homebrew. Launching will be aborted.");
                    break;
            }

            if (result == SUCCESS) {
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

TcpReceiver::eLoadResults TcpReceiver::tryLoadWUHB(void *data, uint32_t fileSize, std::string &loadedPathOut) {
    if (memcmp(data, "WUHB", 4) == 0) {
        DEBUG_FUNCTION_LINE("Try to load a .wuhb");
        if (!FSUtils::CreateSubfolder(APPS_TEMP_PATH)) {
            DEBUG_FUNCTION_LINE_WARN("Failed to create directory: %s", APPS_TEMP_PATH);
            return FILE_SAVE_BUFFER_ERROR;
        }
        if (FSUtils::saveBufferToFile(WUHB_TEMP_FILE, data, fileSize)) {
            loadedPathOut = WUHB_TEMP_FILE_EX;
        } else if (FSUtils::saveBufferToFile(WUHB_TEMP_FILE_2, data, fileSize)) {
            loadedPathOut = WUHB_TEMP_FILE_2_EX;
        } else {
            DEBUG_FUNCTION_LINE_WARN("Failed to save .wuhb file to the sd card. Launching will be aborted.");
            return FILE_SAVE_BUFFER_ERROR;
        }
        return SUCCESS;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a WUHB");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::tryLoadRPX(uint8_t *data, uint32_t fileSize, std::string &loadedPathOut) {
    if (data[0x7] == 0xCA && data[0x8] == 0xFE && data[0x9] != 0x50 && data[0xA] != 0x4C) {
        DEBUG_FUNCTION_LINE("Try to load a .rpx");
        if (!FSUtils::CreateSubfolder(APPS_TEMP_PATH)) {
            DEBUG_FUNCTION_LINE_WARN("Failed to create directory: %s", APPS_TEMP_PATH);
            return FILE_SAVE_BUFFER_ERROR;
        }
        if (FSUtils::saveBufferToFile(RPX_TEMP_FILE, data, fileSize)) {
            loadedPathOut = RPX_TEMP_FILE_EX;
        } else {
            DEBUG_FUNCTION_LINE_WARN("Failed to save .rpx file to the sd card. Launching will be aborted.");
            return FILE_SAVE_BUFFER_ERROR;
        }
        return SUCCESS;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a RPX");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::tryLoadWPS(uint8_t *data, uint32_t fileSize) {
    if (data[0x7] == 0xCA && data[0x8] == 0xFE && data[0x9] == 0x50 && data[0xA] == 0x4C) {
        auto newContainer = WUPSBackend::PluginUtils::getPluginForBuffer((char *) data, fileSize);
        if (newContainer) {
            auto plugins = WUPSBackend::PluginUtils::getLoadedPlugins(32);

            auto &metaInformation = newContainer.value()->getMetaInformation();

            // remove plugins with the same name and author as our new plugin
            plugins.erase(std::remove_if(plugins.begin(), plugins.end(),
                                         [metaInformation](auto &plugin) {
                                             return plugin->getMetaInformation()->getName() == metaInformation->getName() &&
                                                    plugin->getMetaInformation()->getAuthor() == metaInformation->getAuthor();
                                         }),
                          plugins.end());

            // add the new plugin
            plugins.push_back(std::move(newContainer.value()));

#ifdef VERBOSE_DEBUG
            for (auto &plugin : plugins) {
                DEBUG_FUNCTION_LINE_VERBOSE("name: %s", plugin->getMetaInformation()->getName().c_str());
                DEBUG_FUNCTION_LINE_VERBOSE("author: %s", plugin->getMetaInformation()->getAuthor().c_str());
                DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", plugin->getPluginData()->getHandle());
                DEBUG_FUNCTION_LINE_VERBOSE("====");
            }
#endif

            if (WUPSBackend::PluginUtils::LoadAndLinkOnRestart(plugins) != 0) {
                DEBUG_FUNCTION_LINE_ERR("WUPSBackend::PluginUtils::LoadAndLinkOnRestart failed");
                return PLUGIN_LOAD_LINK_FAILED;
            }
            return SUCCESS;
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to parse plugin for buffer: %08X size %d", data, fileSize);
            return PLUGIN_PARSE_FAILED;
        }
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a plugin");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::loadBinary(void *data, uint32_t fileSize) {
    std::string loadedPath;
    eLoadResults error;
    if ((error = tryLoadWUHB(data, fileSize, loadedPath)) != UNSUPPORTED_FORMAT || (error = tryLoadRPX((uint8_t *) data, fileSize, loadedPath)) != UNSUPPORTED_FORMAT) {
        if (error == SUCCESS) {
            RPXLoaderStatus launchRes;
            if ((launchRes = RPXLoader_LaunchHomebrew(loadedPath.c_str())) != RPX_LOADER_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to start %s %s", loadedPath.c_str(), RPXLoader_GetStatusStr(launchRes));
                return LAUNCH_FAILED;
            }
        }

        return error;
    } else if ((error = tryLoadWPS((uint8_t *) data, fileSize)) != UNSUPPORTED_FORMAT) {
        if (error == SUCCESS) {
            _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
        }
        return error;
    }
    return UNSUPPORTED_FORMAT;
}

std::unique_ptr<uint8_t[]> TcpReceiver::uncompressData(uint32_t fileSize, uint32_t fileSizeUnc, std::unique_ptr<uint8_t[]> &&inData, uint32_t &fileSizeOut, eLoadResults &err) {
    std::unique_ptr<uint8_t[]> inflatedData;
    uint8_t *in_data_raw = inData.get();
    // We need to unzip...
    if (in_data_raw[0] == 'P' && in_data_raw[1] == 'K' && in_data_raw[2] == 0x03 && in_data_raw[3] == 0x04) {
        // Section is compressed, inflate
        inflatedData = make_unique_nothrow<uint8_t[]>(fileSizeUnc);
        if (!inflatedData) {
            DEBUG_FUNCTION_LINE_ERR("malloc failed");
            err = NOT_ENOUGH_MEMORY;
            return {};
        }

        int32_t ret;
        z_stream s = {};

        s.zalloc = Z_NULL;
        s.zfree  = Z_NULL;
        s.opaque = Z_NULL;

        ret = inflateInit(&s);
        if (ret != Z_OK) {
            DEBUG_FUNCTION_LINE_ERR("inflateInit failed %i", ret);
            err = FILE_UNCOMPRESS_ERROR;
            return {};
        }

        s.avail_in = fileSize;
        s.next_in  = (Bytef *) inflatedData.get();

        s.avail_out = fileSizeUnc;
        s.next_out  = (Bytef *) inflatedData.get();

        ret = inflate(&s, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            DEBUG_FUNCTION_LINE_ERR("inflate failed %i", ret);
            err = FILE_UNCOMPRESS_ERROR;
            return {};
        }

        inflateEnd(&s);
    } else {
        // Section is compressed, inflate
        inflatedData = make_unique_nothrow<uint8_t[]>(fileSizeUnc);
        if (!inflatedData) {
            DEBUG_FUNCTION_LINE_ERR("malloc failed");
            err = NOT_ENOUGH_MEMORY;
            return {};
        }

        uLongf f       = fileSizeUnc;
        int32_t result = uncompress((Bytef *) inflatedData.get(), &f, (Bytef *) in_data_raw, fileSize);
        if (result != Z_OK) {
            DEBUG_FUNCTION_LINE_ERR("uncompress failed %i", result);
            err = FILE_UNCOMPRESS_ERROR;
            return {};
        }

        fileSizeUnc = f;
    }

    fileSizeOut = fileSizeUnc;
    err         = SUCCESS;
    return inflatedData;
}

std::unique_ptr<uint8_t[]> TcpReceiver::receiveData(int32_t clientSocket, uint32_t fileSize, eLoadResults &err) {
    uint32_t bytesRead = 0;
    auto dataOut       = make_unique_nothrow<uint8_t[]>(fileSize);
    if (!dataOut) {
        err = NOT_ENOUGH_MEMORY;
        return {};
    }

    // Copy binary in memory
    while (bytesRead < fileSize) {
        uint32_t blockSize = 0x1000;
        if (blockSize > (fileSize - bytesRead))
            blockSize = fileSize - bytesRead;

        int32_t ret = recv(clientSocket, dataOut.get() + bytesRead, blockSize, 0);
        if (ret <= 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to receive file");
            break;
        }

        bytesRead += ret;
    }

    if (bytesRead != fileSize) {
        DEBUG_FUNCTION_LINE_ERR("File loading not finished, %i of %i bytes received", bytesRead, fileSize);
        err = RECV_ERROR;
        return {};
    }
    err = SUCCESS;
    return dataOut;
}

TcpReceiver::eLoadResults TcpReceiver::loadToMemory(int32_t clientSocket, uint32_t ipAddress) {
    DEBUG_FUNCTION_LINE("Loading file from ip %08X", ipAddress);

    uint32_t fileSize    = 0;
    uint32_t fileSizeUnc = 0;
    uint8_t haxx[8]      = {};
    // read header
    if (recvwait(clientSocket, haxx, sizeof(haxx)) != 0) {
        return RECV_ERROR;
    }
    if (recvwait(clientSocket, (void *) &fileSize, sizeof(fileSize)) != 0) {
        return RECV_ERROR;
    }
    bool compressedData = (haxx[4] > 0 || haxx[5] > 4);
    if (compressedData) {
        if (recvwait(clientSocket, (void *) &fileSizeUnc, sizeof(fileSizeUnc)) != 0) { // Compressed protocol, read another 4 bytes
            return RECV_ERROR;
        }
    }
    TcpReceiver::eLoadResults err = UNSUPPORTED_FORMAT;
    auto receivedData             = receiveData(clientSocket, fileSize, err);
    if (err != SUCCESS) {
        return err;
    } else if (compressedData) {
        receivedData = uncompressData(fileSize, fileSizeUnc, std::move(receivedData), fileSize, err);
        if (!receivedData || err != SUCCESS) {
            return err;
        }
    }

    return loadBinary(receivedData.get(), fileSize);
}
