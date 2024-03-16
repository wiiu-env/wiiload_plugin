#include "globals.h"

bool gLibRPXLoaderInitDone                      = false;
std::unique_ptr<TcpReceiver> gTcpReceiverThread = nullptr;
bool gWiiloadServerEnabled                      = true;