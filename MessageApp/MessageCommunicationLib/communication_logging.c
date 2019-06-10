#include "stdafx.h"
#include "communication_logging.h"

#define LOGGING_INACTIVE 0
#define LOGGING_ACTIVE 1

int _IsLoggingActive = 0;

void EnableLogging()
{
    _IsLoggingActive = LOGGING_ACTIVE;
}

void DisableLogging()
{
    _IsLoggingActive = LOGGING_INACTIVE;
}
