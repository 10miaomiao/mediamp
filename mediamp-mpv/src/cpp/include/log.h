//
// Created by StageGuard on 12/28/2024.
//

#ifndef MEDIAMP_LOG_H
#define MEDIAMP_LOG_H

#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
#define LOG_TAG "mediampv"
#if (defined(__APPLE__) && defined(__MACH__)) || (defined(__linux__) && !defined(__ANDROID__)) || (defined(_WIN32) || defined(_WIN64))
#include <cstdio>
#include <cstdlib>
static FILE* _log_file() {
    static FILE* f = nullptr;
    if (!f) {
        const char* tmp = getenv("TEMP");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = "/tmp";
        char path[512];
        snprintf(path, sizeof(path), "%s/mediampv_native.log", tmp);
        f = fopen(path, "a");
    }
    return f;
}
#define LOG(...) do { \
    FILE* _f = _log_file(); \
    if (_f) { fprintf(_f, __VA_ARGS__); fprintf(_f, "\n"); fflush(_f); } \
    printf(__VA_ARGS__); printf("\n"); fflush(stdout); \
} while(0)
#else
#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#endif // defined(__APPLE__) && defined(__MACH__) || (defined(__linux__) && !defined(__ANDROID__))
#else
#define LOG(...) ((void *) 0)
#endif // ENABLE_LOGGING

#include <iostream>
struct function_printer_t {
#ifdef ENABLE_LOGGING
    std::string name;
    explicit function_printer_t(const std::string &name) : name(name) {
        LOG("Function %s started", name.c_str());
    }
#else
    explicit function_printer_t(const std::string &_) {}
#endif

    ~function_printer_t() {
#ifdef ENABLE_LOGGING
        LOG("Function %s ended", name.c_str());
#endif
    }
};

#define FP function_printer_t _fp(__FUNCTION__)

#endif //MEDIAMP_LOG_H
