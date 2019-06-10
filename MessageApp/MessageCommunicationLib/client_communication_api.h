#ifndef _CLIENT_COMMUNICATION_API_H_
#define _CLIENT_COMMUNICATION_API_H_

#include "communication_error.h"
#include "communication_data.h"

typedef struct _CM_CLIENT CM_CLIENT;

/*
    This function tries to connect to the server. It should be used in your client application.
    If it executes successfully you can use the newly create CM_CLIENT to send / receive data.
    When the client is no longer needed you can destroy with DestroyClient.

    Parameter:
        NewClient - output parameter - newly created client returned by the function after a successful execution.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR CreateClientConnectionToServer(CM_CLIENT** NewClient);

/*
    This function terminates the connection established with the server and frees all CM_CLIENT resources.
    One should call this when a previously created CM_CLIENT is no longer needed.
    One must not use a freed CM_CLIENT pointer.

    Parameters:
        Client - input parameter - client that must be destroyed.

    Return:
        void
*/
void DestroyClient(CM_CLIENT* Client);

/*
    This function will send some data to the server.
    One should call this function with a CM_CLIENT created with CreateClientConnectionToServer function.
    One should call this function with a properly initialized CM_DATA_BUFFER.

    Parameters:
        Client - input parameter - target client connection to send to ( created with CreateClientConnectionToServer ).
        DataBufferToSend - input parameter - data buffer that must be send, previously created with CreateDataBuffer or CreateDataBufferByCopy.
        SuccessfullySendBytesCount - output parameter - how many bytes were successfully send to the server.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR SendDataToServer(CM_CLIENT* Client, const CM_DATA_BUFFER* DataBufferToSend, CM_SIZE* SuccessfullySendBytesCount);

/*
    This function will receive some data from the server.
    One should call this function with a CM_CLIENT created with CreateClientConnectionToServer function.
    One should call this function with a properly initialized CM_DATA_BUFFER.

    Parameters:
        Client - input parameter - target client connection to receive form ( created with CreateClientConnectionToServer ).
        DataBufferToSend - output parameter - data buffer that must be received, previously created with CreateDataBuffer.
        SuccessfullySendBytesCount - output parameter - how many bytes were successfully received from the server.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )

    Notes:
        You must create a valid CM_DATA_BUFFER with CreateDataBuffer. Let's assume you want to receive 100 bytes from server.
        You will create CreateDataBuffer(&receiveDataBuffer, 100). This will create a CM_DATA_BUFFER with capacity (DataBufferSize) of 100.
        After a successful receive call you will read the actual size of received buffer inside CM_DATA_BUFFER->UsedCapacity or SuccessfullyReceivedBytesCount.
*/
CM_ERROR ReceiveDataFormServer(CM_CLIENT* Client, CM_DATA_BUFFER* DataBufferToReceive, CM_SIZE* SuccessfullyReceivedBytesCount);

#endif // !_CLIENT_COMMUNICATION_API_H_
