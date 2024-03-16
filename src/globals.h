#include "utils/TcpReceiver.h"
#include <memory>
#include <cstdint>

extern bool gLibRPXLoaderInitDone;
extern std::unique_ptr<TcpReceiver> gTcpReceiverThread;
extern bool gWiiloadServerEnabled;
