#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CThread.h"

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
    };

    explicit TcpReceiver(int32_t port);

    virtual ~TcpReceiver();

private:
    void executeThread() override;

    bool createSocket();
    void cleanupSocket();

    static eLoadResults loadToMemory(int32_t clientSocket, uint32_t ipAddress);

    static TcpReceiver::eLoadResults tryLoadWUHB(void *data, uint32_t fileSize, std::string &loadedPathOut);
    static TcpReceiver::eLoadResults tryLoadRPX(uint8_t *data, uint32_t fileSize, std::string &loadedPathOut);
    static TcpReceiver::eLoadResults tryLoadWPS(uint8_t *data, uint32_t fileSize);
    static TcpReceiver::eLoadResults loadBinary(void *data, uint32_t fileSize);
    static std::unique_ptr<uint8_t[]> receiveData(int32_t clientSocket, uint32_t fileSize, eLoadResults &err);
    static std::unique_ptr<uint8_t[]> uncompressData(uint32_t fileSize, uint32_t fileSizeUnc, std::unique_ptr<uint8_t[]> &&in_out_data, uint32_t &fileSizeOut, eLoadResults &err);

    bool exitRequested;
    int32_t serverPort;
    int32_t serverSocket;
};
