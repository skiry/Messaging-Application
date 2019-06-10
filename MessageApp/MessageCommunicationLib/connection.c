#include "stdafx.h"
#include "connection.h"

#include "communication_logging.h"

CM_ERROR BuildConnection(CM_CONNECTION** NewConnection, SOCKET NewConnectionSocket, UINT16 NewConnectionPort)
{
    if (NewConnection == NULL)
        return CM_INVALID_PARAMETER;

    CM_CONNECTION* newConnection = (CM_CONNECTION*)malloc(sizeof(CM_CONNECTION));
    if (newConnection == NULL)
        return CM_NO_MEMORY;

    newConnection->ConnectionSocket = NewConnectionSocket;
    newConnection->ConnectionPort = NewConnectionPort;

    *NewConnection = newConnection;

    return CM_SUCCESS;
}

CM_ERROR SendData(CM_CONNECTION* Connection, const CM_BYTE* InputDataBuffer, CM_SIZE DataBufferSize, CM_SIZE* SuccessfullySendBytesCount)
{
    if (Connection == NULL)
        return CM_INVALID_CONNECTION;

    if (INVALID_SOCKET == Connection->ConnectionSocket)
        return CM_INVALID_PARAMETER;

    int sendResult = send(Connection->ConnectionSocket, (const char*) InputDataBuffer, DataBufferSize, 0);
    if (sendResult == SOCKET_ERROR)
    {
        CM_LOG_ERROR(TEXT("Unexpected error: send failed with err-code=0x%X!"), WSAGetLastError());
        return CM_CONNECTION_SEND_FAILED;
    }

    *SuccessfullySendBytesCount = (CM_SIZE)sendResult;

    return CM_SUCCESS;
}

CM_ERROR ReceiveData(CM_CONNECTION* Connection, CM_BYTE* OutputDataBuffer, CM_SIZE OutputDataBufferSize, CM_SIZE* SuccessfullyReceivedBytesCount)
{
    if (Connection == NULL)
        return CM_INVALID_CONNECTION;

    if (INVALID_SOCKET == Connection->ConnectionSocket)
        return CM_INVALID_PARAMETER;

    int receiveResult = recv(Connection->ConnectionSocket, (char*) OutputDataBuffer, OutputDataBufferSize, 0);
    if (receiveResult == SOCKET_ERROR)
    {
        CM_LOG_ERROR(TEXT("Unexpected error: recv failed with err-code=0x%X!"), WSAGetLastError());
        return CM_CONNECTION_RECEIVE_FAILED;
    }

    if (receiveResult == 0)
        return CM_CONNECTION_TERMINATED;

    *SuccessfullyReceivedBytesCount = (CM_SIZE)receiveResult;

    return CM_SUCCESS;
}

void CloseConnection(CM_CONNECTION* Connection)
{
    if (Connection == NULL)
        return;

    if (INVALID_SOCKET != Connection->ConnectionSocket)
        closesocket(Connection->ConnectionSocket);

    free(Connection);
}
