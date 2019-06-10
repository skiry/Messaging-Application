#include "stdafx.h"
#include "communication_api.h"

#include "communication_logging.h"

#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>

CM_ERROR InitCommunicationModule()
{
    const BYTE majorWSALibVersion = 2;
    const BYTE minorWSALibVersion = 2;

    setbuf(stdout, NULL);

    WORD wVersionRequested = MAKEWORD(majorWSALibVersion, minorWSALibVersion); // WinSock version 2.2
    WSADATA wsaData;

    ZeroMemory(&wsaData, sizeof(WSADATA));

    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        CM_LOG_ERROR(TEXT("WSAStartup failed with err-code=0x%X!"), WSAGetLastError());
        return CM_LIB_INIT_FAILED;
    }

    if (LOBYTE(wsaData.wVersion) != minorWSALibVersion || HIBYTE(wsaData.wVersion) != majorWSALibVersion)
    {
        CM_LOG_ERROR(TEXT("Could not find WSA_LIB32 version 2.2!"));
        WSACleanup();
        return CM_LIB_INIT_FAILED;
    }

    //CM_LOG_INFO(TEXT("Communication lib initialized successfully!"));

    return CM_SUCCESS;
}

void UninitCommunicationModule()
{
    WSACleanup();
    //CM_LOG_INFO(TEXT("Communication lib uninitialized successfully!"));

}

void EnableCommunicationModuleLogger()
{
    EnableLogging();
}

void DisableCommunicationModuleLogger()
{
    DisableLogging();
}
