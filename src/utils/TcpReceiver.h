#pragma once

#include "CThread.h"

#include <notifications/notification_defines.h>

#include <memory>
#include <string>

class ReadWriteStreamIF;

class TcpReceiver : public CThread {
public:
    enum eLoadResults {
        SUCCESS                 = 0,
        UNSUPPORTED_FORMAT      = -1,
        PLUGIN_PARSE_FAILED     = -2,
        PLUGIN_LOAD_LINK_FAILED = -3,
        FILE_SAVE_BUFFER_ERROR  = -4,
        FILE_UNCOMPRESS_ERROR   = -5,
        NOT_ENOUGH_MEMORY       = -6,
        RECV_ERROR              = -7,
        LAUNCH_FAILED           = -8,
        NO_ACTIVE_ACCOUNT       = -9,
        STREAM_ERROR            = -10,
    };

    explicit TcpReceiver(int32_t port);

    virtual ~TcpReceiver();

private:
    void executeThread() override;

    bool createSocket();
    void cleanupSocket();

    static eLoadResults loadToMemory(int32_t clientSocket, uint32_t ipAddress);

    static TcpReceiver::eLoadResults tryLoadWUHB(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle, std::string &loadedPathOut);
    static TcpReceiver::eLoadResults tryLoadRPX(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle, std::string &loadedPathOut);
    static TcpReceiver::eLoadResults tryLoadWPS(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle);
    static TcpReceiver::eLoadResults loadBinary(std::unique_ptr<ReadWriteStreamIF> &&inputStream, NotificationModuleHandle notificationHandle);
    static bool receiveData(int32_t clientSocket, const std::unique_ptr<ReadWriteStreamIF> &output, uint32_t fileSize, NotificationModuleHandle notificationHandle, eLoadResults &err);
    static std::unique_ptr<ReadWriteStreamIF> uncompressData(uint32_t fileSizeUnc, std::unique_ptr<ReadWriteStreamIF> &&inputStream, NotificationModuleHandle notificationHandle, bool toFile, eLoadResults &err);

    bool exitRequested;
    int32_t serverPort;
    int32_t serverSocket;
};
