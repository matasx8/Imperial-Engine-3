#pragma once

#include <android/log.h>


namespace imp
{
    int LogInfo(const char* format, ...) 
    {
        va_list args;
        va_start(args, format);
        int res = __android_log_vprint(ANDROID_LOG_INFO, "IMP", format, args);
        va_end(args);
        return res;
    }
}