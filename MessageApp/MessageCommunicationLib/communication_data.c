#include "stdafx.h"
#include "communication_data.h"

#include <stdlib.h>
#include <string.h>

CM_ERROR CreateDataBuffer(CM_DATA_BUFFER** NewDataBuffer, CM_SIZE NewDataBufferSize)
{
    CM_ERROR error = CM_SUCCESS;
    CM_DATA_BUFFER* newDataBuffer = NULL;
    CM_BYTE* newBuffer = NULL;

    __try
    {
        newBuffer = (CM_BYTE*)malloc(sizeof(CM_BYTE) * NewDataBufferSize);
        if (newBuffer == NULL)
        {
            error = CM_NO_MEMORY;
            __leave;
        }

        newDataBuffer = (CM_DATA_BUFFER*)malloc(sizeof(CM_DATA_BUFFER));
        if (newDataBuffer == NULL)
        {
            error = CM_NO_MEMORY;
            __leave;
        }

        memset(newBuffer, 0, sizeof(CM_BYTE) * NewDataBufferSize);

        newDataBuffer->DataBuffer = newBuffer;
        newDataBuffer->DataBufferSize = NewDataBufferSize;
        newDataBuffer->UsedBufferSize = 0;

        *NewDataBuffer = newDataBuffer;
    }
    __finally
    {
        if (CM_IS_ERROR(error))
        {
            if (newBuffer != NULL)
                free(newBuffer);

        }
    }

    return error;
}

CM_ERROR CreateDataBufferByCopy(CM_DATA_BUFFER** NewDataBuffer, const CM_DATA_BUFFER* SourcedataBuffer)
{
    if (NewDataBuffer == NULL || SourcedataBuffer == NULL)
        return CM_INVALID_PARAMETER;

    CM_ERROR error = CM_SUCCESS;
    CM_DATA_BUFFER* newDataBuffer = NULL;

    __try
    {
        error = CreateDataBuffer(&newDataBuffer, SourcedataBuffer->DataBufferSize);
        if (CM_IS_ERROR(error))
            __leave;

        error = CopyDataIntoBuffer(newDataBuffer, SourcedataBuffer->DataBuffer, SourcedataBuffer->UsedBufferSize);
        if (CM_IS_ERROR(error))
            __leave;

        *NewDataBuffer = newDataBuffer;
    }
    __finally
    {
        if (CM_IS_ERROR(error))
        {
            if (newDataBuffer != NULL)
                DestroyDataBuffer(newDataBuffer);
        }
    }

    return error;
}

void DestroyDataBuffer(CM_DATA_BUFFER* DataBuffer)
{
    if (DataBuffer == NULL)
        return;

    if (DataBuffer->DataBuffer != NULL)
        free(DataBuffer->DataBuffer);

    free(DataBuffer);
}

CM_ERROR CopyDataIntoBuffer(CM_DATA_BUFFER* DataBuffer, const CM_BYTE* Data, CM_SIZE DataSize)
{
    if (DataBuffer == NULL)
        return CM_INVALID_PARAMETER;

    if (DataSize > DataBuffer->DataBufferSize)
        return CM_INSUFFICIENT_BUFFER;

    memcpy(DataBuffer->DataBuffer, Data, DataSize);
    DataBuffer->UsedBufferSize = DataSize;

    return CM_SUCCESS;
}
