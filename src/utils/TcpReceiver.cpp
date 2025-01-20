#include "TcpReceiver.h"

#include "FSUtils.h"
#include "ReadWriteStreamIF.h"
#include "net.h"
#include "utils.h"

#include <notifications/notifications.h>
#include <rpxloader/rpxloader.h>
#include <wups_backend/PluginUtils.h>
#include <wups_backend/import_defines.h>

#include <coreinit/cache.h>
#include <coreinit/title.h>
#include <nn/act.h>
#include <sysapp/launch.h>

#include <zlib.h>

#include <optional>
#include <string>
#include <vector>

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>

#define APPS_TEMP_PATH "fs:/vol/external01/wiiu/apps/"
#define TEMP_FILE_1    "fs:/vol/external01/wiiu/apps/wiiload1.tmp"
#define TEMP_FILE_2    "fs:/vol/external01/wiiu/apps/wiiload2.tmp"
#define TEMP_FILE_3    "fs:/vol/external01/wiiu/apps/wiiload3.tmp"
#define TEMP_FILE_4    "fs:/vol/external01/wiiu/apps/wiiload4.tmp"

static std::string USED_FILE_PATH = "";

namespace {
    class MemoryStreamFixedSize : public ReadWriteStreamIF {
    public:
        explicit MemoryStreamFixedSize(const size_t size)
            : ReadWriteStreamIF(), mBuffer(make_unique_nothrow<uint8_t[]>(size)), mSize(mBuffer ? size : 0), mPosition(0) {}


        MemoryStreamFixedSize(const MemoryStreamFixedSize &) = delete;
        MemoryStreamFixedSize &operator=(const MemoryStreamFixedSize &) = delete;

        bool isOpen() override {
            return mBuffer != nullptr;
        }

        bool isFile() override {
            return false;
        }

        bool isDataWrapper() override {
            return true;
        }

        std::string getPath() override {
            return "";
        }
        void *getData() override {
            return mBuffer.get();
        }

        size_t getSize() override {
            return mSize;
        }

        // Write data to the memory stream
        int64_t write(const void *data, const size_t size) override {
            size_t copySize = size;
            if (mPosition + size > mSize) {
                copySize = mSize - mPosition;
            }
            if (copySize == 0) {
                return 0;
            }
            std::memcpy(mBuffer.get() + mPosition, data, size);
            mPosition += size;
            return size;
        }

        // Read data from the memory stream
        int64_t read(void *output, const size_t size) override {
            size_t readSize = size;
            if (mPosition + size > mSize) {
                readSize = mSize - mPosition;
            }
            if (readSize == 0) {
                return 0;
            }
            std::memcpy(output, mBuffer.get() + mPosition, size);
            mPosition += size;
            return size;
        }

        // Seek to a specific position
        off_t seek(const int64_t offset, const int whence) override {
            int64_t newPos = mPosition;

            if (whence == SEEK_SET) {
                newPos = offset;
            } else if (whence == SEEK_CUR) {
                newPos += offset;
            } else if (whence == SEEK_END) {
                newPos = mSize + offset;
            }
            if (newPos < 0) {
                mPosition = 0;
            } else {
                mPosition = newPos;
            }
            if (mPosition > mSize) {
                mPosition = mSize;
            }

            return mPosition;
        }

        // Get the current position
        [[nodiscard]] off_t tell() const override {
            return mPosition;
        }

    private:
        std::unique_ptr<uint8_t[]> mBuffer;
        size_t mSize;
        size_t mPosition;
    };

    class FileStream final : public ReadWriteStreamIF {
    public:
        explicit FileStream(const std::string_view path, const std::string_view mode) : mPath(path) {
            mFile = ::fopen(path.data(), mode.data());
            if (mFile != nullptr) {
                setvbuf(mFile, nullptr, _IOFBF, 128 * 1024);
                ::fseek(mFile, 0, SEEK_END);
                mSize = ::ftell(mFile);
                ::fseek(mFile, 0, SEEK_SET);
                mPosition = 0;
            }
        }

        ~FileStream() override {
            releaseFile();
        }

        FileStream(const FileStream &) = delete;
        FileStream &operator=(const FileStream &) = delete;

        FileStream(FileStream &&other) noexcept {
            releaseFile();
            mFile           = other.mFile;
            mPath           = std::move(other.mPath);
            mSize           = other.mSize;
            mPosition       = other.mPosition;
            other.mFile     = nullptr;
            other.mSize     = 0;
            other.mPosition = 0;
        }

        void releaseFile() {
            if (mFile != nullptr) {
                ::fclose(mFile);
                mFile = nullptr;
            }
        }

        FileStream &operator=(FileStream &&other) noexcept {
            if (this != &other) {
                releaseFile();
                mFile           = other.mFile;
                mPath           = std::move(other.mPath);
                mSize           = other.mSize;
                mPosition       = other.mPosition;
                other.mFile     = nullptr;
                other.mSize     = 0;
                other.mPosition = 0;
            }
            return *this;
        }

        bool isOpen() override {
            return mFile != nullptr;
        }

        bool isFile() override {
            return true;
        }

        bool isDataWrapper() override {
            return false;
        }

        std::string getPath() override {
            return mPath;
        }

        void *getData() override {
            return nullptr;
        }

        size_t getSize() override {
            return mSize;
        }

        // Write data to the memory stream
        int64_t write(const void *data, const size_t size) override {
            const auto res = ::fwrite(data, 1, size, mFile);
            mPosition += res;
            if (mPosition > mSize) {
                mSize = mPosition;
            }
            return res;
        }

        // Read data from the memory stream
        int64_t read(void *output, const size_t size) override {
            const auto res = ::fread(output, 1, size, mFile);
            mPosition += res;
            return res;
        }

        // Seek to a specific position
        off_t seek(const int64_t offset, const int whence) override {
            const auto res = ::fseek(mFile, offset, whence);
            mPosition      = res;
            return res;
        }

        // Get the current position
        [[nodiscard]] off_t tell() const override {
            return mPosition;
        }

    private:
        FILE *mFile = nullptr;
        std::string mPath;
        size_t mSize     = 0;
        size_t mPosition = 0;
    };

    std::unique_ptr<ReadWriteStreamIF> getNewFileOutputStream() {
        std::unique_ptr<ReadWriteStreamIF> dataStream;
        if (USED_FILE_PATH != TEMP_FILE_1) {
            if (dataStream = std::make_unique<FileStream>(TEMP_FILE_1, "w+"); dataStream->isOpen()) {
                return dataStream;
            }
        }
        if (USED_FILE_PATH != TEMP_FILE_2) {
            if (dataStream = std::make_unique<FileStream>(TEMP_FILE_2, "w+"); dataStream->isOpen()) {
                return dataStream;
            }
        }
        if (USED_FILE_PATH != TEMP_FILE_3) {
            if (dataStream = std::make_unique<FileStream>(TEMP_FILE_3, "w+"); dataStream->isOpen()) {
                return dataStream;
            }
        }
        if (USED_FILE_PATH != TEMP_FILE_4) {
            if (dataStream = std::make_unique<FileStream>(TEMP_FILE_4, "w+"); dataStream->isOpen()) {
                return dataStream;
            }
        }
        return {};
    }

    bool saveDataToFile(const std::unique_ptr<ReadWriteStreamIF> &inputStream, std::string &loadedPathOut, const NotificationModuleHandle notificationHandle) {
        if (USED_FILE_PATH != TEMP_FILE_1 && FSUtils::saveBufferToFile(TEMP_FILE_1, inputStream->getData(), inputStream->getSize(), notificationHandle)) {
            loadedPathOut = TEMP_FILE_1;
            return true;
        }
        if (USED_FILE_PATH != TEMP_FILE_2 && FSUtils::saveBufferToFile(TEMP_FILE_2, inputStream->getData(), inputStream->getSize(), notificationHandle)) {
            loadedPathOut = TEMP_FILE_2;
            return true;
        }
        if (USED_FILE_PATH != TEMP_FILE_3 && FSUtils::saveBufferToFile(TEMP_FILE_3, inputStream->getData(), inputStream->getSize(), notificationHandle)) {
            loadedPathOut = TEMP_FILE_3;
            return true;
        }
        if (USED_FILE_PATH != TEMP_FILE_4 && FSUtils::saveBufferToFile(TEMP_FILE_4, inputStream->getData(), inputStream->getSize(), notificationHandle)) {
            loadedPathOut = TEMP_FILE_4;
            return true;
        }
        return false;
    }

} // namespace

TcpReceiver::TcpReceiver(int32_t port)
    : CThread(CThread::eAttributeAffCore1, 16, 0x20000, nullptr, nullptr, "Wiiload Thread"), exitRequested(false), serverPort(port), serverSocket(-1) {
    resumeThread();
}

TcpReceiver::~TcpReceiver() {
    exitRequested = true;
    OSMemoryBarrier();
    if (serverSocket >= 0) {
        cleanupSocket();
    }
}

bool TcpReceiver::createSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (serverSocket < 0) {
        return false;
    }

    uint32_t enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in bindAddress {};
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family      = AF_INET;
    bindAddress.sin_port        = serverPort;
    bindAddress.sin_addr.s_addr = INADDR_ANY;

    int32_t ret;
    if ((ret = bind(serverSocket, (struct sockaddr *) &bindAddress, 16)) < 0) {
        DEBUG_FUNCTION_LINE_WARN("Failed to bind socket: %d errno: %d", ret, errno);
        cleanupSocket();
        return false;
    }

    if ((ret = listen(serverSocket, 1)) < 0) {
        DEBUG_FUNCTION_LINE_WARN("Failed to bind socket: %d errno: %d", ret, errno);
        cleanupSocket();
        return false;
    }
    return true;
}

void TcpReceiver::cleanupSocket() {
    if (serverSocket >= 0) {
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }
}

void TcpReceiver::executeThread() {
    socklen_t len;
    while (!exitRequested) {
        if (serverSocket < 0) {
            if (!createSocket() && !exitRequested) {
                if (errno != EBUSY) {
                    DEBUG_FUNCTION_LINE_WARN("Create socket failed %d", errno);
                }
                OSSleepTicks(OSMillisecondsToTicks(10));
            }
            continue;
        }
        struct sockaddr_in clientAddr = {};
        len                           = 16;
        int32_t clientSocket          = accept(serverSocket, (struct sockaddr *) &clientAddr, &len);
        if (clientSocket >= 0) {
            uint32_t ipAddress = clientAddr.sin_addr.s_addr;
            DEBUG_FUNCTION_LINE("Waiting for wiiload connection");

            auto result = loadToMemory(clientSocket, ipAddress);
            close(clientSocket);

            switch (result) {
                case SUCCESS:
                    break;
                case FILE_UNCOMPRESS_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to decrompress recieved data. Launching will be aborted.");
                    break;
                case NOT_ENOUGH_MEMORY:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Not enough memory. Launching will be aborted.");
                    break;
                case RECV_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to receive data. Launching will be aborted.");
                    break;
                case UNSUPPORTED_FORMAT:
                    NotificationModule_AddErrorNotification("Wiiload plugin:  Tried to load an unsupported file. Launching will be aborted.");
                    break;
                case PLUGIN_PARSE_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to load plugin. Maybe unsupported version? Launching will be aborted.");
                    break;
                case PLUGIN_LOAD_LINK_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to load plugins. Maybe unsupported version? Launching will be aborted.");
                    break;
                case FILE_SAVE_BUFFER_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to save file to the sd card. Launching will be aborted.");
                    break;
                case LAUNCH_FAILED:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to launch homebrew. Launching will be aborted.");
                    break;
                case NO_ACTIVE_ACCOUNT:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to launch homebrew. No active account loaded.");
                    break;
                case STREAM_ERROR:
                    NotificationModule_AddErrorNotification("Wiiload plugin: Failed to launch homebrew. Invalid stream type");
                    break;
            }

            if (result == SUCCESS) {
                break;
            }
        } else if (!exitRequested) {
            if (errno != EBUSY) {
                DEBUG_FUNCTION_LINE_WARN("Accept failed %d", errno);
                cleanupSocket();
            }
            OSSleepTicks(OSMillisecondsToTicks(10));
        }
        OSSleepTicks(OSMillisecondsToTicks(1));
    }
    cleanupSocket();
    DEBUG_FUNCTION_LINE("Stopping wiiload server.");
}

TcpReceiver::eLoadResults TcpReceiver::tryLoadWUHB(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle, std::string &loadedPathOut) {
    inputStream->seek(0, SEEK_SET);
    std::vector<uint8_t> header(4);
    inputStream->read(header.data(), 4);
    if (memcmp(header.data(), "WUHB", 4) == 0) {
        DEBUG_FUNCTION_LINE("Try to load a .wuhb");
        if (inputStream->isDataWrapper() && inputStream->getData()) {
            if (!FSUtils::CreateSubfolder(APPS_TEMP_PATH)) {
                DEBUG_FUNCTION_LINE_WARN("Failed to create directory: %s", APPS_TEMP_PATH);
                return FILE_SAVE_BUFFER_ERROR;
            }
            if (!saveDataToFile(inputStream, loadedPathOut, notificationHandle)) {
                DEBUG_FUNCTION_LINE_WARN("Failed to save .wuhb file to the sd card. Launching will be aborted.");
                return FILE_SAVE_BUFFER_ERROR;
            }
        } else if (inputStream->isFile()) {
            loadedPathOut = inputStream->getPath();

            if (loadedPathOut.empty()) {
                DEBUG_FUNCTION_LINE_WARN("Path was empty. Launching will be aborted.");
                return FILE_SAVE_BUFFER_ERROR;
            }
            // Release stream so we can access it!
            inputStream.reset();
        } else {
            DEBUG_FUNCTION_LINE_WARN("Invalid stream type detected. Launching will be aborted.");
            return FILE_SAVE_BUFFER_ERROR;
        }
        return SUCCESS;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a WUHB");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::tryLoadRPX(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle, std::string &loadedPathOut) {
    inputStream->seek(0, SEEK_SET);
    std::vector<uint8_t> header(16);
    inputStream->read(header.data(), 16);
    if (header[0x7] == 0xCA && header[0x8] == 0xFE && header[0x9] != 0x50 && header[0xA] != 0x4C) {
        DEBUG_FUNCTION_LINE("Try to load a .rpx");
        if (inputStream->isDataWrapper() && inputStream->getData()) {
            if (!FSUtils::CreateSubfolder(APPS_TEMP_PATH)) {
                DEBUG_FUNCTION_LINE_WARN("Failed to create directory: %s", APPS_TEMP_PATH);
                return FILE_SAVE_BUFFER_ERROR;
            }
            if (!saveDataToFile(inputStream, loadedPathOut, notificationHandle)) {
                DEBUG_FUNCTION_LINE_WARN("Failed to save .rpx file to the sd card. Launching will be aborted.");
                return FILE_SAVE_BUFFER_ERROR;
            }
        } else if (inputStream->isFile()) {
            loadedPathOut = inputStream->getPath();

            if (loadedPathOut.empty()) {
                DEBUG_FUNCTION_LINE_WARN("Path was empty. Launching will be aborted.");
                return FILE_SAVE_BUFFER_ERROR;
            }
            // Release stream so we can access it!
            inputStream.reset();
        } else {
            DEBUG_FUNCTION_LINE_WARN("Invalid stream type detected. Launching will be aborted.");
            return FILE_SAVE_BUFFER_ERROR;
        }
        return SUCCESS;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a RPX");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::tryLoadWPS(std::unique_ptr<ReadWriteStreamIF> &inputStream, NotificationModuleHandle notificationHandle) {
    inputStream->seek(0, SEEK_SET);
    std::vector<uint8_t> header(16);
    inputStream->read(header.data(), 16);
    if (header[0x7] == 0xCA && header[0x8] == 0xFE && header[0x9] == 0x50 && header[0xA] == 0x4C) {
        PluginBackendApiErrorType err;
        PluginBackendPluginParseError parseErr;
        std::optional<WUPSBackend::PluginContainer> newContainerOpt;
        if (inputStream->isFile()) {
            const auto path = inputStream->getPath();
            // Reset stream ptr so we can access the file!
            inputStream.reset();

            newContainerOpt = WUPSBackend::PluginUtils::getPluginForPath(path, err, parseErr);
            // We can delete file no from the sd card
            remove(path.c_str());
        } else if (inputStream->isDataWrapper()) {
            DEBUG_FUNCTION_LINE_ERR("Loading from buffer");
            newContainerOpt = WUPSBackend::PluginUtils::getPluginForBuffer(static_cast<char *>(inputStream->getData()), inputStream->getSize(), err, parseErr);
            // release the memory as early as possible.
            inputStream.reset();
        } else {
            DEBUG_FUNCTION_LINE_ERR("Invalid stream type. This should never happen!");
            return STREAM_ERROR;
        }

        if (newContainerOpt) {
            auto pluginList = WUPSBackend::PluginUtils::getLoadedPlugins(err);
            if (err != PLUGIN_BACKEND_API_ERROR_NONE) {
                DEBUG_FUNCTION_LINE_ERR("Failed to load plugin list.");
                return PLUGIN_PARSE_FAILED;
            }

            const auto &metaInformation = newContainerOpt->getMetaInformation();
            // remove plugins with the same name and author as our new plugin
            std::erase_if(pluginList,
                          [&metaInformation](auto &plugin) {
                              auto res = plugin.getMetaInformation().getName() == metaInformation.getName() &&
                                         plugin.getMetaInformation().getAuthor() == metaInformation.getAuthor();
                              return res;
                          });
            // add the new plugin
            pluginList.push_back(std::move(newContainerOpt.value()));
#ifdef VERBOSE_DEBUG
            for (const auto &plugin : pluginList) {
                DEBUG_FUNCTION_LINE_VERBOSE("name: %s", plugin.getMetaInformation().getName().c_str());
                DEBUG_FUNCTION_LINE_VERBOSE("author: %s", plugin.getMetaInformation().getAuthor().c_str());
                DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", plugin.getPluginData().getHandle());
                DEBUG_FUNCTION_LINE_VERBOSE("====");
            }
#endif
            if ((err = WUPSBackend::PluginUtils::LoadAndLinkOnRestart(pluginList)) != PLUGIN_BACKEND_API_ERROR_NONE) {
                DEBUG_FUNCTION_LINE_ERR("WUPSBackend::PluginUtils::LoadAndLinkOnRestart failed. %s", WUPSBackend::GetStatusStr(err));
                return PLUGIN_LOAD_LINK_FAILED;
            }
            return SUCCESS;
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to parse plugin. Error: %s", WUPSBackend::GetStatusStr(err));
            return PLUGIN_PARSE_FAILED;
        }
    }
    DEBUG_FUNCTION_LINE_VERBOSE("Loaded data is not a plugin");
    return UNSUPPORTED_FORMAT;
}

TcpReceiver::eLoadResults TcpReceiver::loadBinary(std::unique_ptr<ReadWriteStreamIF> &&inputStream, const NotificationModuleHandle notificationHandle) {
    std::string loadedPath;
    if (eLoadResults error; (error = tryLoadWUHB(inputStream, notificationHandle, loadedPath)) != UNSUPPORTED_FORMAT || (error = tryLoadRPX(inputStream, notificationHandle, loadedPath)) != UNSUPPORTED_FORMAT) {
        if (error != SUCCESS) {
            return error;
        }

        // lets save which file we don't want to override
        USED_FILE_PATH = loadedPath;

        const auto index = loadedPath.find("fs:/vol/external01/");
        if (index != std::string::npos) {
            loadedPath.erase(index, strlen("fs:/vol/external01/"));
        }

        DEBUG_FUNCTION_LINE_VERBOSE("Loaded file: other %s", loadedPath.c_str());

        nn::act::Initialize();
        bool accountLoaded = nn::act::IsSlotOccupied(nn::act::GetSlotNo());
        nn::act::Finalize();

        if (!accountLoaded) {
            return NO_ACTIVE_ACCOUNT;
        }

        if (const RPXLoaderStatus launchRes = RPXLoader_LaunchHomebrew(loadedPath.c_str()); launchRes != RPX_LOADER_RESULT_SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("Failed to start \"%s\": %s", loadedPath.c_str(), RPXLoader_GetStatusStr(launchRes));
            return LAUNCH_FAILED;
        }

        NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Launching homebrew...");
        return SUCCESS;
    } else if ((error = tryLoadWPS(inputStream, notificationHandle)) != UNSUPPORTED_FORMAT) {
        if (error != SUCCESS) {
            return error;
        }
        _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Reloading with new plugin...");
        return SUCCESS;
    }
    return UNSUPPORTED_FORMAT;
}

constexpr size_t CHUNK_SIZE = 128 * 1024; // Size of chunks to process

std::unique_ptr<ReadWriteStreamIF> TcpReceiver::uncompressData(uint32_t fileSizeUnc, std::unique_ptr<ReadWriteStreamIF> &&inputStream, const NotificationModuleHandle notificationHandle, const bool toFile, eLoadResults &err) {
    if (!inputStream->isOpen()) {
        DEBUG_FUNCTION_LINE_ERR("inputStream is not open");
        return {};
    }
    std::unique_ptr<ReadWriteStreamIF> outputStream = {};
    if (toFile) {
        outputStream = getNewFileOutputStream();
    } else {
        outputStream = std::make_unique<MemoryStreamFixedSize>(fileSizeUnc);
    }
    if (!outputStream || !outputStream->isOpen()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to open or create outputStream");
        return {};
    }

    inputStream->seek(0, SEEK_SET);
    outputStream->seek(0, SEEK_SET);

    std::vector<unsigned char> inBuffer(CHUNK_SIZE);
    std::vector<unsigned char> outBuffer(CHUNK_SIZE);

    z_stream strm = {};
    strm.zalloc   = Z_NULL;
    strm.zfree    = Z_NULL;
    strm.opaque   = Z_NULL;

    // Initialize the inflate state
    if (inflateInit(&strm) != Z_OK) {
        DEBUG_FUNCTION_LINE_ERR("Failed to call inflateInit");
        return {};
    }

    int ret = Z_STREAM_ERROR;
    do {
        // Read a chunk of compressed data
        auto readRes = inputStream->read(inBuffer.data(), CHUNK_SIZE);

        std::string progressStr = string_format("[Wiiload] Decompressing data... %.2f%%", static_cast<float>(inputStream->tell()) / static_cast<float>(inputStream->getSize()) * 100.0f);
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, progressStr.c_str());

        if (readRes <= 0) {
            break;
        }

        strm.avail_in = readRes;
        strm.next_in  = inBuffer.data();

        // Decompress in a loop
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out  = outBuffer.data();

            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR); /* state not clobbered */
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR; /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR: {
                    DEBUG_FUNCTION_LINE_ERR("inflate() failed");
                    (void) inflateEnd(&strm);
                    return {};
                }
            }

            auto have = CHUNK_SIZE - strm.avail_out;
            if (outputStream->write(outBuffer.data(), have) != have) {
                DEBUG_FUNCTION_LINE_ERR("write() failed");
                (void) inflateEnd(&strm);
                return {};
            }
        } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END);

    // clean up uncompressed file
    if (inputStream->isFile()) {
        const auto path = inputStream->getPath();
        inputStream.reset();
        remove(path.c_str());
    }

    // Clean up
    inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        DEBUG_FUNCTION_LINE_ERR("Failed to uncompress data");
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Decompressing data... failed");
        return {};
    }

    NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Decompressing data... 100.00%");
    return outputStream;
}

bool TcpReceiver::receiveData(const int32_t clientSocket, const std::unique_ptr<ReadWriteStreamIF> &output, const uint32_t fileSize, const NotificationModuleHandle notificationHandle, eLoadResults &err) {
    uint32_t bytesRead = 0;
    std::vector<unsigned char> inBuffer(CHUNK_SIZE);
    // Copy binary in memory
    while (bytesRead < fileSize) {
        std::string progressStr = string_format("[Wiiload] Receiving data... %.2f%%", static_cast<float>(bytesRead) / static_cast<float>(fileSize) * 100.0f);
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, progressStr.c_str());

        uint32_t blockSize = CHUNK_SIZE;
        if (blockSize > (fileSize - bytesRead))
            blockSize = fileSize - bytesRead;

        const int32_t ret = recv(clientSocket, inBuffer.data(), blockSize, 0);
        if (ret <= 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to receive file");
            break;
        }
        int64_t writtenData = 0;
        while (writtenData < ret) {
            const int64_t toWrite = ret - writtenData;
            const auto res        = output->write(inBuffer.data() + writtenData, toWrite);
            if (res <= 0) {
                if (res < 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to receive file");
                }
                break;
            }
            writtenData += res;
        }
        if (writtenData != ret) {
            DEBUG_FUNCTION_LINE_ERR("Failed to write to file");
            break;
        }

        bytesRead += ret;
    }

    if (bytesRead != fileSize) {
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Receiving data... failed");
        DEBUG_FUNCTION_LINE_ERR("File loading not finished, %i of %i bytes received", bytesRead, fileSize);
        err = RECV_ERROR;
        return false;
    }

    NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Receiving data... 100.00%");

    err = SUCCESS;
    return true;
}

TcpReceiver::eLoadResults TcpReceiver::loadToMemory(int32_t clientSocket, uint32_t ipAddress) {
    DEBUG_FUNCTION_LINE("Loading file from ip %08X", ipAddress);
    uint32_t fileSize    = 0;
    uint32_t fileSizeUnc = 0;
    uint8_t haxx[8]      = {};
    // read header
    if (recvwait(clientSocket, haxx, sizeof(haxx)) != 0) {
        return RECV_ERROR;
    }
    if (recvwait(clientSocket, (void *) &fileSize, sizeof(fileSize)) != 0) {
        return RECV_ERROR;
    }
    bool compressedData = (haxx[4] > 0 || haxx[5] > 4);
    if (compressedData) {
        if (recvwait(clientSocket, (void *) &fileSizeUnc, sizeof(fileSizeUnc)) != 0) { // Compressed protocol, read another 4 bytes
            return RECV_ERROR;
        }
    }
    eLoadResults err = UNSUPPORTED_FORMAT;

    NotificationModuleHandle notificationHandle;
    if (NotificationModule_AddDynamicNotification("[Wiiload] Receiving binary via network", &notificationHandle) != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        notificationHandle = 0;
    }

    const bool asFile = fileSize > 1;
    std::unique_ptr<ReadWriteStreamIF> dataStream;
    if (asFile) {
        dataStream = getNewFileOutputStream();
    } else {
        dataStream = std::make_unique<MemoryStreamFixedSize>(fileSize);
    }

    if (!dataStream || !receiveData(clientSocket, dataStream, fileSize, notificationHandle, err) || err != SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("ERROR: Failed to receive file");
        NotificationModule_FinishDynamicNotification(notificationHandle, 0.5f);
        return err;
    }

    if (compressedData) {
        dataStream = uncompressData(fileSizeUnc, std::move(dataStream), notificationHandle, asFile, err);
        if (!dataStream || err != SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("ERROR: Failed to uncompress file");
            NotificationModule_FinishDynamicNotification(notificationHandle, 0.5f);
            return err;
        }
    }
    dataStream->seek(0, SEEK_SET);

    const auto res = loadBinary(std::move(dataStream), notificationHandle);
    NotificationModule_FinishDynamicNotification(notificationHandle, 0.5f);
    return res;
}
