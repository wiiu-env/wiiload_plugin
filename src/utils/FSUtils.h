#pragma once

#include <wut_types.h>

class FSUtils {
public:
    static bool CreateSubfolder(const char *fullpath);

    static bool CheckFile(const char *filepath);

    static bool saveBufferToFile(const char *path, void *buffer, uint32_t size);
};