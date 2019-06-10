#include "stdafx.h"
#include "client_communication_api.h"

#include "connection.h"
#include "communication_logging.h"

#include <stdlib.h>

typedef struct _CM_CLIENT
{
    CM_CONNECTION* ClientConnection;

}CM_CLIENT;

static CM_ERROR _CreateClient(CM_CLIENT** NewClient);
static CM_ERROR _TryServerConnectionOnPortRange(SOCKET ClientSocket
    , const char* IPv4ServerAddress
    , UINT16 BeginPort
    , UINT16 EndPort
    , UINT16* ConnectionPort
);

CM_ERROR CreateClientConnectionToServer(CM_CLIENT** NewClient)
{
    CM_ERROR error = CM_SUCCESS;
    CM_CLIENT* newClient = NULL;
    CM_CONNECTION* newConnection = NULL;
    SOCKET newSocket = INVALID_SOCKET;

    __try
    {
        error = _CreateClient(&newClient);
        if (CM_IS_ERROR(error))
            __leave;

        newSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (newSocket == INVALID_SOCKET)
        {
            //CM_LOG_ERROR(TEXT("socket failed with err-code=0x%X!"), WSAGetLastError());
            error = CM_NO_RESOURCES_FOR_SERVER_CLIENT;
            __leave;
        }
        UINT16 connectionPort = 0;
        error = _TryServerConnectionOnPortRange(newSocket, IPv4_LOCALHOST_ADDRESS_AS_STRING, SERVER_PORT_BEGIN, SERVER_PORT_END, &connectionPort);
        if (CM_IS_ERROR(error))
        {
            //CM_LOG_ERROR(TEXT("Failed to connect to server! Tried entire port range [50010, 50020]! Probably server is not running!"));
            __leave;
        }

        error = BuildConnection(&newConnection, newSocket, connectionPort);
        if (CM_IS_ERROR(error))
            __leave;

        newSocket = INVALID_SOCKET; // newConnection owns the socket now

        newClient->ClientConnection = newConnection;

        *NewClient = newClient;
    }
    __finally
    {
        if (CM_IS_ERROR(error))
        {
            if (newSocket != INVALID_SOCKET)
                closesocket(newSocket);

            if (newClient != NULL)
                DestroyClient(newClient);
        }
    }

    return error;
}

void DestroyClient(CM_CLIENT* Client)
{
    if (Client == NULL)
        return;

    CloseConnection(Client->ClientConnection);

    free(Client);
}

CM_ERROR SendDataToServer(CM_CLIENT* Client, const CM_DATA_BUFFER* DataBufferToSend, CM_SIZE* SuccessfullySendBytesCount)
{
    if (Client == NULL || DataBufferToSend == NULL || SuccessfullySendBytesCount == NULL)
        return CM_INVALID_PARAMETER;

    return SendData(Client->ClientConnection, DataBufferToSend->DataBuffer, DataBufferToSend->UsedBufferSize, SuccessfullySendBytesCount);
}

CM_ERROR ReceiveDataFormServer(CM_CLIENT* Client, CM_DATA_BUFFER* DataBufferToReceive, CM_SIZE* SuccessfullyReceivedBytesCount)
{
    if (Client == NULL || DataBufferToReceive == NULL || SuccessfullyReceivedBytesCount == NULL)
        return CM_INVALID_PARAMETER;

    CM_ERROR error = ReceiveData(
        Client->ClientConnection
        , DataBufferToReceive->DataBuffer
        , DataBufferToReceive->DataBufferSize
        , SuccessfullyReceivedBytesCount
    );

    DataBufferToReceive->UsedBufferSize = *SuccessfullyReceivedBytesCount;

    return error;
}

CM_ERROR _CreateClient(CM_CLIENT** NewClient)
{
    CM_CLIENT* newClient = (CM_CLIENT*)malloc(sizeof(CM_CLIENT));
    if (newClient == NULL)
        return CM_NO_MEMORY;

    newClient->ClientConnection = NULL;

    *NewClient = newClient;

    return CM_SUCCESS;
}

CM_ERROR _TryServerConnectionOnPortRange(SOCKET ClientSocket
    , const char* IPv4ServerAddress
    , UINT16 BeginPort
    , UINT16 EndPort
    , UINT16* ConnectionPort
)
{
    SOCKADDR_IN clientInfo;
    clientInfo.sin_family = AF_INET;
    clientInfo.sin_addr.s_addr = inet_addr(IPv4ServerAddress);

    while (BeginPort <= EndPort)
    {
        clientInfo.sin_port = htons(BeginPort);
        int connectResult = connect(ClientSocket, (SOCKADDR*)&clientInfo, sizeof(SOCKADDR_IN));
        if (connectResult == 0)
        {
            *ConnectionPort = BeginPort;
            return CM_SUCCESS;
        }
        ++BeginPort;
    }

    return CM_NO_RESOURCES_FOR_SERVER_CLIENT;
}