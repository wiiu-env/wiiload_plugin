#include "globals.h"
#include "utils/TcpReceiver.h"

bool gLibRPXLoaderInitDone                      = false;
std::unique_ptr<TcpReceiver> gTcpReceiverThread = nullptr;
bool gWiiloadServerEnabled                      = true;