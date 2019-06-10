#ifndef _SERVER_COMMUNICATION_API_H_
#define _SERVER_COMMUNICATION_API_H_

#include "communication_error.h"
#include "communication_data.h"

/*
    Represent a server. It can satisfy multiple clients and requests.
*/
typedef struct _CM_SERVER CM_SERVER;

/*
    Represents a connected client. It can be used to resolve client requests.
*/
typedef struct _CM_SERVER_CLIENT CM_SERVER_CLIENT;

/*
    This function will create a new server entity. One should use this inside server application.
    After a successful execution the newly created server can be used to accept new client connections.
    One should not create a new server after a client has connected. The server entity is reusable after a new client is accepted.

    Parameters:
        NewServer - output parameter - here one will find the newly created server.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR CreateServer(CM_SERVER** NewServer);

/*
    This function will shutdown and free all resources associated with a server entity.
    One should destroy only servers created with CreateServer.

    Parameters:
        Server - input parameter - server to be closed & destroyed.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
void DestroyServer(CM_SERVER* Server);

/*
    This function will await a new client to connect.
    Beware the calling thread will be blocked until a new client connects.
    After a successful wait the newly connected client can be found in NewServerClient.
    Further handling of data exchange between server a client must be made through CM_SERVER_CLIENT entity - NewServerClient parameter.
    One can save the NewServerClient variable and process new requests for the connected client.
    One should not create a new server after a client has connected. The server entity is reusable after a new client is accepted.
    One should call AbandonClient when the client connection is no longer necessary.

    Parameters:
        Server - input parameter - server entity that awaits a new client.
        NewServerClient - output parameter - the newly connected client.

    Return:
        void

*/
CM_ERROR AwaitNewClient(CM_SERVER* Server, CM_SERVER_CLIENT** NewServerClient);

/*
    This function can be used to send data to a connected client.

    Parameters:
        Client - input parameter - target client for send call, previously returned by AwaitNewClient function.
        DataBufferToSend - input parameter - data to be send created with CreateDataBuffer or CreateDataBufferByCopy.
        SuccessfullySendBytesCount - output parameter - number of bytes that were successfully send to the client.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR SendDataToClient(CM_SERVER_CLIENT* Client, const CM_DATA_BUFFER* DataBufferToSend, CM_SIZE* SuccessfullySendBytesCount);

/*
    This function can be used to received data from a connected client.

    Parameters:
        Client - input parameter - target client for received call, previously returned by AwaitNewClient function.
        DataBufferToReceive - output parameter - data received from the client, it should be previously created with CreateDataBuffer.
        SuccessfullyReceivedBytesCount - output parameter - number of bytes that were successfully received form the client.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )

    Notes:
        You must create a valid CM_DATA_BUFFER with CreateDataBuffer. Let's assume you want to receive 100 bytes from the client.
        You will create CreateDataBuffer(&receiveDataBuffer, 100). This will create a CM_DATA_BUFFER with capacity (DataBufferSize) of 100.
        After a successful receive call you can read the actual size of received buffer inside CM_DATA_BUFFER->UsedCapacity or SuccessfullyReceivedBytesCount.
*/
CM_ERROR ReceiveDataFromClient(CM_SERVER_CLIENT* Client, CM_DATA_BUFFER* DataBufferToReceive, CM_SIZE* SuccessfullyReceivedBytesCount);

/*
    This function will close the connection with a connected client.
    One should only abandon a client that was created by calling AwaitNewClient method.

    Parameters:
        Client - input parameter - client to be destroyed & abandoned.

    Return:
        void
*/
void AbandonClient(CM_SERVER_CLIENT* Client);

#endif // !_SERVER_COMMUNICATION_API_H_
