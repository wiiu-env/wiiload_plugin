#pragma once

#include <string>

#include <cstdint>

class ReadWriteStreamIF {
public:
    ReadWriteStreamIF() = default;

    virtual ~ReadWriteStreamIF() = default;
    // Delete the copy constructor and copy assignment operator
    ReadWriteStreamIF(const ReadWriteStreamIF &) = delete;
    ReadWriteStreamIF &operator=(const ReadWriteStreamIF &) = delete;

    virtual bool isOpen() = 0;

    // must not return false if a path can be returned
    virtual bool isFile()         = 0;
    virtual std::string getPath() = 0;

    // must not return false if a data/size can be returned
    virtual bool isDataWrapper() = 0;
    virtual void *getData()      = 0;
    virtual size_t getSize()     = 0;

    virtual int64_t write(const void *data, size_t size) = 0;

    // Read data from the memory stream
    virtual int64_t read(void *output, size_t size) = 0;

    // Seek to a specific position
    virtual off_t seek(int64_t position, int whence) = 0;

    // Get the current position
    [[nodiscard]] virtual off_t tell() const = 0;
};