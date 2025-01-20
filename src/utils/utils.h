#pragma once

#include <malloc.h>
#include <memory>


template<class T, class... Args>
std::unique_ptr<T> make_unique_nothrow(Args &&...args) noexcept(noexcept(T(std::forward<Args>(args)...))) {
    return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}

template<typename T>
inline typename std::unique_ptr<T> make_unique_nothrow(size_t num) noexcept {
    return std::unique_ptr<T>(new (std::nothrow) std::remove_extent_t<T>[num]());
}

template<typename... Args>
std::string string_format(const std::string_view format, Args... args) {
    const int size_s = std::snprintf(nullptr, 0, format.data(), args...) + 1; // Extra space for '\0'
    const auto size  = static_cast<size_t>(size_s);
    const auto buf   = make_unique_nothrow<char[]>(size);
    if (!buf) {
        DEBUG_FUNCTION_LINE_ERR("string_format failed, not enough memory");
        OSFatal("string_format failed, not enough memory");
        return std::string("");
    }
    std::snprintf(buf.get(), size, format.data(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}
