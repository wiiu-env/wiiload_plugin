#include "FSUtils.h"
#include "logger.h"
#include "utils.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <notifications/notifications.h>
#include <string>
#include <unistd.h>

bool FSUtils::CheckFile(const char *filepath) {
    if (!filepath || strlen(filepath) == 0) {
        return false;
    }

    struct stat filestat {};

    char dirnoslash[strlen(filepath) + 2];
    snprintf(dirnoslash, sizeof(dirnoslash), "%s", filepath);

    while (dirnoslash[strlen(dirnoslash) - 1] == '/') {
        dirnoslash[strlen(dirnoslash) - 1] = '\0';
    }

    char *notRoot = strrchr(dirnoslash, '/');
    if (!notRoot) {
        strcat(dirnoslash, "/");
    }

    if (stat(dirnoslash, &filestat) == 0) {
        return true;
    }

    return false;
}

bool FSUtils::CreateSubfolder(const char *fullpath) {
    if (!fullpath || strlen(fullpath) == 0) {
        return false;
    }

    bool result = false;

    char dirnoslash[strlen(fullpath) + 1];
    strcpy(dirnoslash, fullpath);

    auto pos = strlen(dirnoslash) - 1;
    while (dirnoslash[pos] == '/') {
        dirnoslash[pos] = '\0';
        pos--;
    }

    if (CheckFile(dirnoslash)) {
        return true;
    } else {
        char parentpath[strlen(dirnoslash) + 2];
        strcpy(parentpath, dirnoslash);
        char *ptr = strrchr(parentpath, '/');

        if (!ptr) {
            //!Device root directory (must be with '/')
            strcat(parentpath, "/");
            struct stat filestat {};
            if (stat(parentpath, &filestat) == 0) {
                return true;
            }

            return false;
        }

        ptr++;
        ptr[0] = '\0';

        result = CreateSubfolder(parentpath);
    }

    if (!result) {
        return false;
    }

    if (mkdir(dirnoslash, 0777) == -1) {
        return false;
    }

    return true;
}

bool FSUtils::saveBufferToFile(const char *path, void *buffer, uint32_t size, NotificationModuleHandle notificationHandle) {
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) {
        DEBUG_FUNCTION_LINE_ERR("Failed to open %s. %d", path, fd);
        return false;
    }
    auto sizeToWrite = size;
    auto *ptr        = buffer;
    int32_t curResult;
    int64_t totalSizeWritten = 0;
    while (sizeToWrite > 0) {
        if (notificationHandle != 0) {
            std::string progressStr = string_format("[Wiiload] Write data to file %.2f%%", static_cast<float>(totalSizeWritten) / static_cast<float>(size) * 100.0);
            NotificationModule_UpdateDynamicNotificationText(notificationHandle, progressStr.c_str());
        }
        curResult = write(fd, ptr, sizeToWrite);
        if (curResult < 0) {
            close(fd);
            return false;
        }
        if (curResult == 0) {
            break;
        }
        ptr = (void *) (((uint32_t) ptr) + curResult);
        totalSizeWritten += curResult;
        sizeToWrite -= curResult;
    }
    close(fd);

    if (notificationHandle != 0) {
        std::string progressStr = string_format("[Wiiload] Save data to file %.2f%%", static_cast<float>(totalSizeWritten) / static_cast<float>(size) * 100.0);
        NotificationModule_UpdateDynamicNotificationText(notificationHandle, "[Wiiload] Write data to file 100%");
    }

    return totalSizeWritten == size;
}
