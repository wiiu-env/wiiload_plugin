#ifndef TCP_RECEIVER_H_
#define TCP_RECEIVER_H_

#include <vector>
#include <string>

#include "CThread.h"

class TcpReceiver : public CThread {
public:
    enum eLoadResults {
        SUCCESS = 0,
        INVALID_INPUT = -1,
        FILE_OPEN_FAILURE = -2,
        FILE_READ_ERROR = -3,
        NOT_ENOUGH_MEMORY = -4,
        NOT_A_VALID_PLUGIN = -5,
    };

    explicit TcpReceiver(int32_t port);

    ~TcpReceiver() override;

    //sigslot::signal2<GuiElement *, uint32_t> serverReceiveStart;
    //sigslot::signal3<GuiElement *, uint32_t, int32_t> serverReceiveFinished;

private:

    void executeThread() override;

    static int32_t loadToMemory(int32_t clientSocket, uint32_t ipAddress);

    bool exitRequested;
    int32_t serverPort;
    int32_t serverSocket;
};


#endif
