#ifndef _COMMUNICATION_API_H_
#define _COMMUNICATION_API_H_

/*
    General rules:
        Every Create / Init / Enable call made will allocate / acquire some resources. Those resources must be freed using the corresponding function.
        If something fails inside library functions, one can enable the logging to gather extra information.
        Additional information collected from logging can be searched in Microsoft Docs.
        The error codes printed ( HEX format ) can be found at https://docs.microsoft.com/en-us/windows/desktop/winsock/windows-sockets-error-codes-2.
*/

// here you can find library errors
#include "communication_error.h"

// here you can find functions necessary for server side
#include "server_communication_api.h"

// here you can find functions necessary for client side
#include "client_communication_api.h"

// you must call this function before any other function ( everything else will fail if you don't initialize the library )
CM_ERROR InitCommunicationModule();

// you must call this before you application exit to free resources associated with communication library
void UninitCommunicationModule();

// you can call those functions anytime ( even before InitCommunicationModule ) to enable / disable printing of debug messages
void EnableCommunicationModuleLogger();
void DisableCommunicationModuleLogger();

#endif // !_COMMUNICATION_API_H_
