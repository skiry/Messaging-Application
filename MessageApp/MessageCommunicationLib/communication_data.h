#ifndef _COMMUNICATION_DATA_H_
#define _COMMUNICATION_DATA_H_

#include "communication_error.h"
#include "communication_types.h"

typedef struct _CM_DATA_BUFFER
{
    CM_BYTE* DataBuffer;    // continuous buffer of bytes
    CM_SIZE DataBufferSize; // total buffer size( capacity )
    CM_SIZE UsedBufferSize; // currently used buffer size

}CM_DATA_BUFFER;

/*
    This function create a new data buffer.
    After a successful call the newly create CM_DATA_BUFFER will contain:
        DataBuffer - allocated continuous buffer of bytes of size NewDataBufferSize, which is filled with zeros.
        DataBufferSize - number of allocated bytes ( capacity ) which is equal to the input parameter NewDataBufferSize.
        UsedBufferSize - will be 0 because currently the buffer does not contain any data.

    Parameters:
        NewDataBuffer - output parameter - after a successful call you can find a fully initialized CM_DATA_BUFFER here.
        NewDataBufferSize - input parameter - desired size( capacity ) for the newly created buffer.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR CreateDataBuffer(CM_DATA_BUFFER** NewDataBuffer, CM_SIZE NewDataBufferSize);

/*
    This function will create a new data buffer from one that is already constructed.
    All members will be copied, including the buffer contents.
    After a successful call the output buffer will be an identical copy of the source.

    Parameters:
        NewDataBuffer - output parameter - after a successful call you can find a fully initialized CM_DATA_BUFFER here.
        SourcedataBuffer - input parameter - the source buffer which will be used to initialize the new buffer.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )

*/
CM_ERROR CreateDataBufferByCopy(CM_DATA_BUFFER** NewDataBuffer, const CM_DATA_BUFFER* SourcedataBuffer);

/*
    This function will destroy a data buffer previously created with CreateDataBuffer or CreateDataBufferByCopy.
    One should not free the contained buffer ( DataBuffer member ) outside this function.
    All resources associated with CM_DATA_BUFFER are freed inside this function.

    Parameters:
        DataBuffer - input parameter - data buffer to be destroyed.

    Return:
        void
*/
void DestroyDataBuffer(CM_DATA_BUFFER* DataBuffer);

/*
    This function will copy some contents into the buffer of a previously initialized CM_DATA_BUFFER.
    This function will check if CM_DATA_BUFFER instance has enough capacity to copy the source data.
    This function will update the UsedBufferSize member after a successful execution.
    After a successful call the newly create CM_DATA_BUFFER will contain:
        DataBuffer - allocated continuous buffer of bytes of size DataBufferSize, which contains same bytes as Data parameter.
        DataBufferSize - number of allocated bytes ( capacity ), one that was set when the buffer was created.
        UsedBufferSize - new used buffer size, equal to DataSize parameter.

    Parameters:
        DataBuffer - input parameter - target buffer used as a destination for copy operation.
        Data - input parameter - source data buffer used during copy parameter.
        DataSize - input parameter - source data buffer size.

    Return:
        CM_SUCCESS on success
        Error on failure ( consult communication_error.h )
*/
CM_ERROR CopyDataIntoBuffer(CM_DATA_BUFFER* DataBuffer, const CM_BYTE* Data, CM_SIZE DataSize);

#endif // !_COMMUNICATION_DATA_H_
