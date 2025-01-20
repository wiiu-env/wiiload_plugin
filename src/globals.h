#include <memory>

class TcpReceiver;

extern bool gLibRPXLoaderInitDone;
extern std::unique_ptr<TcpReceiver> gTcpReceiverThread;
extern bool gWiiloadServerEnabled;
