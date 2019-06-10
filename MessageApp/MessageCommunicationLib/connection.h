#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "communication_error.h"
#include "communication_types.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <stdlib.h>

#define SERVER_PORT_BEGIN (UINT16)50010
#define SERVER_PORT_END (UINT16)50020
#define IPv4_LOCALHOST_ADDRESS_AS_STRING "127.0.0.1"

typedef struct _CM_CONNECTION
{
    SOCKET ConnectionSocket;
    UINT16 ConnectionPort;

}CM_CONNECTION;

CM_ERROR BuildConnection(CM_CONNECTION** NewConnection, SOCKET NewConnectionSocket, UINT16 NewConnectionPort);

CM_ERROR SendData(
    CM_CONNECTION* Connection
    , const CM_BYTE* InputDataBuffer
    , CM_SIZE DataBufferSize
    , CM_SIZE* SuccessfullySendBytesCount
);

CM_ERROR ReceiveData(
    CM_CONNECTION* Connection
    , CM_BYTE* OutputDataBuffer
    , CM_SIZE OutputDataBufferSize
    , CM_SIZE* SuccessfullyReceivedBytesCount
);

void CloseConnection(CM_CONNECTION* Connection);

#endif // !_CONNECTION_H_
