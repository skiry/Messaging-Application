#ifndef _COMMUNICATION_LOGGING_H_
#define _COMMUNICATION_LOGGING_H_

#include <tchar.h>

extern int _IsLoggingActive;

void EnableLogging();
void DisableLogging();

#define CM_LOG(Format, ...) if(_IsLoggingActive) _tprintf_s(Format TEXT("\n"), __VA_ARGS__)

#define CM_LOG_ERROR(Format, ...) CM_LOG(TEXT("[ERROR]") Format, __VA_ARGS__)
#define CM_LOG_INFO(Format, ...) CM_LOG(TEXT("[INFO]") Format, __VA_ARGS__)

#endif // !_COMMUNICATION_LOGGING_H_
