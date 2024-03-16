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
    static TcpReceiver::eLoadResults uncompressIfNeeded(const uint8_t *haxx, uint32_t fileSize, uint32_t fileSizeUnc, std::unique_ptr<uint8_t> &&in_data, std::unique_ptr<uint8_t> &out_data, uint32_t &fileSizeOut);

    bool exitRequested;
    int32_t serverPort;
    int32_t serverSocket;
};
