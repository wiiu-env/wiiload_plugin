#pragma once

#include <string>
#include <vector>

#include "CThread.h"

class TcpReceiver : public CThread {
public:
    enum eLoadResults {
        SUCCESS            = 0,
        INVALID_INPUT      = -1,
        FILE_OPEN_FAILURE  = -2,
        FILE_READ_ERROR    = -3,
        NOT_ENOUGH_MEMORY  = -4,
        NOT_A_VALID_PLUGIN = -5,
        NOT_SUPPORTED      = -6,
    };

    explicit TcpReceiver(int32_t port);

    virtual ~TcpReceiver();

private:
    void executeThread() override;

    bool createSocket();
    void cleanupSocket();

    static int32_t loadToMemory(int32_t clientSocket, uint32_t ipAddress);

    bool exitRequested;
    int32_t serverPort;
    int32_t serverSocket;
};
