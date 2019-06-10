#include "stdafx.h"
#include "server_communication_api.h"

#include "communication_error.h"
#include "connection.h"
#include "communication_logging.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct _CM_SERVER
{
    SOCKET ServerSocket;
    UINT16 ServerPort;
}CM_SERVER;

typedef struct _CM_SERVER_CLIENT
{
    CM_CONNECTION* ClientConnection;
}CM_SERVER_CLIENT;

static CM_ERROR _TryServerBindOnPortRange(CM_SERVER* Server, const char* IPv4ServerAddress, UINT16 BeginPort, UINT16 EndPort);

CM_ERROR CreateServer(CM_SERVER** NewServer)
{
    if (NewServer == NULL)
        return CM_INVALID_PARAMETER;

    CM_ERROR error = CM_SUCCESS;
    CM_SERVER* newServer = NULL;

    __try
    {
        newServer = (CM_SERVER*)malloc(sizeof(CM_SERVER));
        if (newServer == NULL)
        {
            error = CM_NO_MEMORY;
            __leave;
        }

        newServer->ServerSocket = INVALID_SOCKET;
        newServer->ServerPort = 0;

        SOCKET newServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (newServerSocket == INVALID_SOCKET)
        {
            error = CM_NO_RESOURCES_FOR_SERVER;
            __leave;
        }

        newServer->ServerSocket = newServerSocket;

        error = _TryServerBindOnPortRange(newServer, IPv4_LOCALHOST_ADDRESS_AS_STRING, SERVER_PORT_BEGIN, SERVER_PORT_END);
        if (CM_IS_ERROR(error))
        {
            CM_LOG_ERROR(TEXT("Failed to find a server port in range [50010, 50020]!"));
            __leave;
        }

        int listenResult = listen(newServer->ServerSocket, SOMAXCONN);
        if (listenResult == SOCKET_ERROR)
        {
            error = CM_NO_RESOURCES_FOR_SERVER;
            __leave;
        }

        *NewServer = newServer;
    }
    __finally
    {
        if (CM_IS_ERROR(error))
            DestroyServer(newServer);
    }

    return error;
}

void DestroyServer(CM_SERVER* Server)
{
    if (Server == NULL)
        return;

    if (Server->ServerSocket != INVALID_SOCKET)
        closesocket(Server->ServerSocket);

    free(Server);
}

CM_ERROR AwaitNewClient(CM_SERVER* Server, CM_SERVER_CLIENT** NewServerClient)
{
    if (NewServerClient == NULL)
        return CM_INVALID_PARAMETER;

    CM_ERROR error = CM_SUCCESS;
    SOCKET newClientSocket = INVALID_SOCKET;
    CM_CONNECTION* newClientConnection = NULL;
    CM_SERVER_CLIENT* newServerClient = NULL;

    __try
    {
        newClientSocket = accept(Server->ServerSocket, NULL, NULL);
        if (newClientSocket == INVALID_SOCKET)
        {
            error = CM_NO_RESOURCES_FOR_SERVER_CLIENT;
            __leave;
        }

        error = BuildConnection(&newClientConnection, newClientSocket, 0);
        if (CM_IS_ERROR(error))
            __leave;

        newClientSocket = INVALID_SOCKET; // newClientConnections owns the SOCKET now

        newServerClient = (CM_SERVER_CLIENT*)malloc(sizeof(CM_SERVER_CLIENT));
        if (newServerClient == NULL)
        {
            error = CM_NO_MEMORY;
            __leave;
        }

        newServerClient->ClientConnection = newClientConnection;

        *NewServerClient = newServerClient;
    }
    __finally
    {
        if (CM_IS_ERROR(error))
        {
            if (newClientSocket != INVALID_SOCKET)
                closesocket(newClientSocket);

            if (newClientConnection != NULL)
                CloseConnection(newClientConnection);
        }
    }

    return error;
}

CM_ERROR SendDataToClient(CM_SERVER_CLIENT* Client, const CM_DATA_BUFFER* DataBufferToSend, CM_SIZE* SuccessfullySendBytesCount)
{
    if (Client == NULL || DataBufferToSend == NULL || SuccessfullySendBytesCount == NULL)
        return CM_INVALID_PARAMETER;

    return SendData(Client->ClientConnection, DataBufferToSend->DataBuffer, DataBufferToSend->UsedBufferSize, SuccessfullySendBytesCount);
}

CM_ERROR ReceiveDataFromClient(CM_SERVER_CLIENT* Client, CM_DATA_BUFFER* DataBufferToReceive, CM_SIZE* SuccessfullyReceivedBytesCount)
{
    if (Client == NULL || DataBufferToReceive == NULL || SuccessfullyReceivedBytesCount == NULL)
        return CM_INVALID_PARAMETER;

    CM_ERROR error = ReceiveData(
        Client->ClientConnection
        , DataBufferToReceive->DataBuffer
        , DataBufferToReceive->DataBufferSize
        , SuccessfullyReceivedBytesCount
    );
    if (CM_IS_ERROR(error))
        return error;

    DataBufferToReceive->UsedBufferSize = *SuccessfullyReceivedBytesCount;

    return error;
}

void AbandonClient(CM_SERVER_CLIENT* Client)
{
    if (Client == NULL)
        return;

    CloseConnection(Client->ClientConnection);

    free(Client);
}

CM_ERROR _TryServerBindOnPortRange(CM_SERVER* Server, const char* IPv4ServerAddress, UINT16 BeginPort, UINT16 EndPort)
{
    SOCKADDR_IN serverInfo;
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(IPv4ServerAddress);

    while (BeginPort <= EndPort)
    {
        serverInfo.sin_port = htons(BeginPort);

        int bindResult = bind(Server->ServerSocket, (SOCKADDR*) &serverInfo, sizeof(SOCKADDR_IN));
        if (bindResult == 0)
        {
            Server->ServerPort = BeginPort;
            return CM_SUCCESS;
        }

        CM_LOG_ERROR(TEXT("bind failed with err-code=0x%X!"), WSAGetLastError());
        ++BeginPort;
    }

    return CM_NO_RESOURCES_FOR_SERVER;
}
