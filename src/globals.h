#include "utils/TcpReceiver.h"
#include <cstdint>
#include <memory>

extern bool gLibRPXLoaderInitDone;
extern std::unique_ptr<TcpReceiver> gTcpReceiverThread;
extern bool gWiiloadServerEnabled;
