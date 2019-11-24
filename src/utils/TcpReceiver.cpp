#include <algorithm>
#include <string>
#include <vector>
#include <string.h>
#include <zlib.h>
#include <libgen.h>
#include <sysapp/launch.h>

#include <coreinit/messagequeue.h>
#include <coreinit/ios.h>

#include "TcpReceiver.h"
#include "fs/CFile.hpp"
#include "fs/FSUtils.h"
#include "utils/logger.h"
#include "utils/StringTools.h"
#include "utils/net.h"
#include "utils/utils.h"
#include "plugin/PluginInformationUtils.h"

#define WUPS_TEMP_PLUGIN_PATH "fs:/vol/external01/wiiu/plugins/temp/"
#define WUPS_TEMP_PLUGIN_FILE "fs:/vol/external01/wiiu/plugins/temp/temp.mod"

#define RPX_TEMP_PATH "fs:/vol/external01/wiiu/apps/"
#define RPX_TEMP_FILE "fs:/vol/external01/wiiu/apps/temp.rpx"
#define RPX_TEMP_FILE_EX "wiiu/apps/temp.rpx"

extern "C"{
    uint64_t _SYSGetSystemApplicationTitleId(int32_t);
    void _SYSLaunchTitleWithStdArgsInNoSplash(uint64_t, uint32_t);
}
TcpReceiver::TcpReceiver(int32_t port)
    : CThread(CThread::eAttributeAffCore0)
    , exitRequested(false)
    , serverPort(port)
    , serverSocket(-1) {

    resumeThread();
}

TcpReceiver::~TcpReceiver() {
    exitRequested = true;

    if(serverSocket >= 0) {
        shutdown(serverSocket, SHUT_RDWR);
    }
}

#define wiiu_geterrno()  (socketlasterr())

void TcpReceiver::executeThread() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (serverSocket < 0)
        return;

    DEBUG_FUNCTION_LINE("\n");

    uint32_t enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = serverPort;
    bindAddress.sin_addr.s_addr = INADDR_ANY;

    DEBUG_FUNCTION_LINE("\n");

    socklen_t len;
    int32_t ret;
    if ((ret = bind(serverSocket, (struct sockaddr *)&bindAddress, 16)) < 0) {
        socketclose(serverSocket);
        return;
    }

    DEBUG_FUNCTION_LINE("\n");

    if ((ret = listen(serverSocket, 1)) < 0) {
        socketclose(serverSocket);
        return;
    }

    DEBUG_FUNCTION_LINE("\n");

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    int32_t addrlen = sizeof(struct sockaddr);

    while(!exitRequested) {

        DEBUG_FUNCTION_LINE("\n");
        len = 16;
        int32_t clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
        if(clientSocket >= 0) {

            DEBUG_FUNCTION_LINE("\n");
            uint32_t ipAddress = clientAddr.sin_addr.s_addr;
            //serverReceiveStart(this, ipAddress);
            int32_t result = loadToMemory(clientSocket, ipAddress);
            //serverReceiveFinished(this, ipAddress, result);
            socketclose(clientSocket);

            if(result > 0)
                break;
        } else {
            DEBUG_FUNCTION_LINE("Server socket accept failed %i %d\n", clientSocket,wiiu_geterrno());
            OSSleepTicks(OSMicrosecondsToTicks(100000));
        }
    }

    socketclose(serverSocket);
}

typedef struct __attribute((packed)) {
    uint32_t command;
    uint32_t target;
    uint32_t filesize;
    uint32_t fileoffset;
    char path[256];
}LOAD_REQUEST;

int32_t TcpReceiver::loadToMemory(int32_t clientSocket, uint32_t ipAddress) {
    DEBUG_FUNCTION_LINE("Loading file from ip %08X\n", ipAddress);

    uint32_t fileSize = 0;
    uint32_t fileSizeUnc = 0;
    unsigned char haxx[8];
    memset(haxx, 0, sizeof(haxx));
    //skip haxx
    recvwait(clientSocket, haxx, sizeof(haxx));
    recvwait(clientSocket, (unsigned char*)&fileSize, sizeof(fileSize));

    if (haxx[4] > 0 || haxx[5] > 4) {
        recvwait(clientSocket, (unsigned char*)&fileSizeUnc, sizeof(fileSizeUnc)); // Compressed protocol, read another 4 bytes
    }

    struct in_addr in;
    uint32_t bytesRead = 0;
    in.s_addr = ipAddress;

    DEBUG_FUNCTION_LINE("transfer start\n");

    unsigned char* loadAddress = (unsigned char*)memalign(0x40, fileSize);
    if(!loadAddress) {
        OSSleepTicks(OSSecondsToTicks(1));
        return NOT_ENOUGH_MEMORY;
    }

    // Copy rpl in memory
    while(bytesRead < fileSize) {

        uint32_t blockSize = 0x1000;
        if(blockSize > (fileSize - bytesRead))
            blockSize = fileSize - bytesRead;

        int32_t ret = recv(clientSocket, loadAddress + bytesRead, blockSize, 0);
        if(ret <= 0) {
            DEBUG_FUNCTION_LINE("Failure on reading file\n");
            break;
        }

        bytesRead += ret;
    }

    if(bytesRead != fileSize) {
        free(loadAddress);
        DEBUG_FUNCTION_LINE("File loading not finished, %i of %i bytes received\n", bytesRead, fileSize);
        return FILE_READ_ERROR;
    }

    bool res = false;
    bool loadedRPX = false;

    // Do we need to unzip this thing?
    if (haxx[4] > 0 || haxx[5] > 4) {
        unsigned char* inflatedData = NULL;

        // We need to unzip...
        if (loadAddress[0] == 'P' && loadAddress[1] == 'K' && loadAddress[2] == 0x03 && loadAddress[3] == 0x04) {
            //! TODO:
            //! mhmm this is incorrect, it has to parse the zip

            // Section is compressed, inflate
            inflatedData = (unsigned char*)malloc(fileSizeUnc);
            if(!inflatedData) {
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
            s.next_in = (Bytef *)(&loadAddress[0]);

            s.avail_out = fileSizeUnc;
            s.next_out = (Bytef *)&inflatedData[0];

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
            inflatedData = (unsigned char*)malloc(fileSizeUnc);
            if(!inflatedData) {
                free(loadAddress);

                return NOT_ENOUGH_MEMORY;
            }

            uLongf f = fileSizeUnc;
            int32_t result = uncompress((Bytef*)&inflatedData[0], &f, (Bytef*)loadAddress, fileSize);
            if(result != Z_OK) {
                DEBUG_FUNCTION_LINE("uncompress failed %i\n", result);

                return FILE_READ_ERROR;
            }

            fileSizeUnc = f;
            fileSize = fileSizeUnc;
        }

        if(inflatedData[0x7] == 0xCA && inflatedData[0x8] == 0xFE){
            DEBUG_FUNCTION_LINE("Try to load a rpx\n");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(RPX_TEMP_FILE,inflatedData, fileSize);
            free(inflatedData);
            loadedRPX = true;
        }else{
            FSUtils::CreateSubfolder(WUPS_TEMP_PLUGIN_PATH);
            res = FSUtils::saveBufferToFile(WUPS_TEMP_PLUGIN_FILE,inflatedData, fileSize);
            free(inflatedData);
        }

    } else {
        if(loadAddress[0x7] == 0xCA && loadAddress[0x8] == 0xFE){
            DEBUG_FUNCTION_LINE("Try to load a rpx\n");
            FSUtils::CreateSubfolder(RPX_TEMP_PATH);
            res = FSUtils::saveBufferToFile(RPX_TEMP_FILE,loadAddress, fileSize);
            free(loadAddress);
            loadedRPX = true;
        }else{
            FSUtils::CreateSubfolder(WUPS_TEMP_PLUGIN_PATH);
            res = FSUtils::saveBufferToFile(WUPS_TEMP_PLUGIN_FILE,loadAddress, fileSize);
            free(loadAddress);
        }
    }

    if(!res) {
        return NOT_ENOUGH_MEMORY;
    }

    if(loadedRPX){
        LOAD_REQUEST request;
        memset(&request, 0, sizeof(request));

        log_printf("Loading file %s\n", RPX_TEMP_FILE_EX);
        request.command = 0xFC; // IPC_CUSTOM_LOAD_CUSTOM_RPX;
        request.target = 0;     // LOAD_FILE_TARGET_SD_CARD
        request.filesize = 0;   // unknown
        request.fileoffset = 0; //

        strncpy(request.path, RPX_TEMP_FILE_EX, 255);

        int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
        if(mcpFd >= 0) {
            int out = 0;
            IOS_Ioctl(mcpFd, 100, &request, sizeof(request), &out, sizeof(out));
            IOS_Close(mcpFd);
            if(out == 2) {

            }
        }

        uint64_t titleID = _SYSGetSystemApplicationTitleId(8);
        _SYSLaunchTitleWithStdArgsInNoSplash(titleID, 0);
        return fileSize;
    }


    PluginInformation * newFile = PluginInformationUtils::loadPluginInformation("sd:/wiiu/plugins/temp/temp.mod");
    if(newFile == NULL){
        return -1;
    }

    std::vector<PluginInformation *> alreadyLoaded =  PluginInformationUtils::getPluginsLoadedInMemory();

    std::vector<PluginInformation *> newList;

    newList.push_back(newFile);

    for (std::vector<PluginInformation *>::iterator it = alreadyLoaded.begin() ; it != alreadyLoaded.end(); ++it) {
        PluginInformation * curPlugin = *it;
        if(curPlugin->getPath().compare(newFile->getPath()) != 0){
            if(curPlugin->getName().compare(newFile->getName()) == 0 &&
               curPlugin->getAuthor().compare(newFile->getAuthor()) == 0
               ){
                   DEBUG_FUNCTION_LINE("Name and Author of the new plugin are identical to an old one. Loading the new one! %s %s\n",newFile->getName().c_str(),newFile->getAuthor().c_str());
                   continue;
               }
            newList.push_back(curPlugin);
        }else{
            DEBUG_FUNCTION_LINE("%s was overridden\n",newFile->getPath().c_str());
        }
    }

    PluginInformationUtils::loadAndLinkPluginsOnRestart(newList);

    alreadyLoaded.push_back(newFile);
    PluginInformationUtils::clearPluginInformation(alreadyLoaded);

    SYSRelaunchTitle(NULL,NULL);

    return fileSize;
}
