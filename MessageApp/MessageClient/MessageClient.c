// MessageClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// communication library
#include "communication_api.h"

#include <windows.h>
#include <strsafe.h>

enum InputType { Echo, Register, Login, Logout, Msg, Broadcast, Sendfile, List, Exit, History, UnknownInput };
enum InputType FindInputType(TCHAR*);
CM_ERROR ProcessEcho(TCHAR*, CM_CLIENT**);
CM_ERROR ProcessRegister(TCHAR*, CM_CLIENT**);
CM_ERROR ProcessExit(CM_CLIENT**);
CM_ERROR ProcessLogin(TCHAR *, CM_CLIENT**);
CM_ERROR ProcessLogout(CM_CLIENT**);
CM_ERROR ProcessList(CM_CLIENT**);
CM_ERROR ProcessMsg(TCHAR*, CM_CLIENT**);
CM_ERROR ProcessBroadcast(TCHAR*, CM_CLIENT**);
CM_ERROR ProcessHistory(TCHAR*, CM_CLIENT**);
CM_ERROR SendTextToServer(TCHAR*, CM_CLIENT**);
CM_ERROR SendCommandToServer(int, CM_CLIENT**);

#define ECHO 0
#define REGISTER 1
#define LOGIN 2
#define LOGOUT 3
#define MSG 4
#define BROADCAST 5
#define SENDFILE 6
#define LIST 7
#define EXIT 8
#define HISTORY 9
#define EXITLOGGEDIN 10
#define STANDARD_SIZE 10242880

int gLogged = 0, gExit = 0;
int gFirstTime = 0, gAllowConnection = 2;
int gLastOperation;
TCHAR* gLoggedUserName;
CRITICAL_SECTION gCs;

DWORD WINAPI FromUser(PVOID Client)
{
    CM_CLIENT* client = (CM_CLIENT*)Client;
    DWORD result = 0;
    TCHAR* fromStdin = (TCHAR*)malloc(sizeof(TCHAR) * 1100);
    CM_ERROR error = CM_SUCCESS;

    if (NULL == fromStdin)
    {
        error = CM_NO_MEMORY;
        _tprintf_s(TEXT("Unexpected error: Not enough memory. err-code=0x%X!\n"), error);
        DestroyClient(client);
        UninitCommunicationModule();
        return result;
    }
    enum InputType input;
    int reading = 1;

    while (reading)
    {
        StringCbGets(fromStdin, 1100);
        input = FindInputType(fromStdin);

        switch (input)
        {
        case Echo:
            if (CM_SUCCESS != ProcessEcho(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, echo failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Register:
            if (CM_SUCCESS != ProcessRegister(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, register failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Login:
            if (CM_SUCCESS != ProcessLogin(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, login failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Logout:
            if (CM_SUCCESS != ProcessLogout(&client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, logout failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Msg:
            if (CM_SUCCESS != ProcessMsg(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, msg failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Broadcast:
            if (CM_SUCCESS != ProcessBroadcast(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, broadcast failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Sendfile:
            break;
        case List:
            if (CM_SUCCESS != ProcessList(&client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, list failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        case Exit:
            reading = 0;
            gExit = 1;
            if (CM_SUCCESS != ProcessExit(&client))
            {
                _tprintf_s(TEXT("Unexpected error: Exit failed with err-code=0x%X!\n"), error);
            }
            break;
        case History:
            if (CM_SUCCESS != ProcessHistory(fromStdin, &client))
            {
                _tprintf_s(TEXT("Unexpected error: Bad input given, history failed with err-code=0x%X!\n"), error);
                reading = 0;
            }
            break;
        default:
            break;

        }
    }
    free(fromStdin);

    return result;
}

DWORD WINAPI FromServer(PVOID Client)
{
    CM_CLIENT* client = (CM_CLIENT*)Client;
    DWORD result = 0;

    CM_DATA_BUFFER* dataToReceiveLength = NULL;
    CM_SIZE dataToReceiveLengthSize = sizeof(int);

    CM_DATA_BUFFER* dataToReceive = NULL;

    CM_ERROR error = CM_SUCCESS;
    CM_SIZE receivedByteCount = 0;
    TCHAR* receivedText = (TCHAR*)malloc(sizeof(TCHAR) * STANDARD_SIZE + 4);

    int length, exitted;

    error = CreateDataBuffer(&dataToReceiveLength, dataToReceiveLengthSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
        free(receivedText);
        return error;
    }

    while (!gExit && gAllowConnection)
    {
        error = ReceiveDataFormServer(client, dataToReceiveLength, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: ReceiveDataFormServer ?? failed with err-code=0x%X!\n"), error);
            free(receivedText);
            DestroyDataBuffer(dataToReceiveLength);
            return error;
        }

        length = *(int*)(dataToReceiveLength->DataBuffer);

        error = CreateDataBuffer(&dataToReceive, length);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
            free(receivedText);
            return error;
        }

        error = ReceiveDataFormServer(client, dataToReceive, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: ReceiveDataFormServer failed with err-code=0x%X!\n"), error);
            free(receivedText);
            DestroyDataBuffer(dataToReceiveLength);
            return error;
        }

        exitted = *(int*)(dataToReceive->DataBuffer);
        if (exitted)
        {
            StringCbCopy(receivedText, length + 2, (STRSAFE_LPCWSTR)dataToReceive->DataBuffer);
            _tprintf_s(TEXT("%s"), receivedText);
            if (gLastOperation == LOGIN)
            {
                if (_tcscmp(receivedText, TEXT("Success\n")) == 0)
                {
                    ++gLogged;
                }
                gLastOperation = -1;
            }
            if (!gFirstTime && _tcscmp(receivedText, TEXT("Error: maximum concurrent connection count reached\n")) == 0)
            {
                ++gFirstTime;
                gAllowConnection = 0;
            }
            else
            {
                gAllowConnection = 1;
            }
        }
        DestroyDataBuffer(dataToReceive);
    }

    free(receivedText);
    DestroyDataBuffer(dataToReceiveLength);
    return result;
}

int _tmain(int argc, TCHAR* argv[])
{
    (void)argc;
    (void)argv;

    EnableCommunicationModuleLogger();

    if (argc != 1)
    {
        _tprintf_s(TEXT("Unexpected error: too many parameters given\n"));
        return -1;
    }

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\n"), error);
        return -1;
    }

    CM_CLIENT* client = NULL;
    error = CreateClientConnectionToServer(&client);

    if (CM_IS_ERROR(error))
    {
        if (CM_NO_RESOURCES_FOR_SERVER_CLIENT == error)
        {
            _tprintf_s(TEXT("Error: no running server found\n"));
        }
        else
        {
            _tprintf_s(TEXT("Unexpected error: The server is on and there are free slots. However, you could not connect\n"));
        }
        UninitCommunicationModule();
        return -1;
    }

    HANDLE userThread, serverThread;
    gLoggedUserName = (TCHAR*)malloc(sizeof(TCHAR) * 400);

    if (NULL == (serverThread = CreateThread(
        NULL,
        0,
        FromServer,
        (PVOID)client,
        0,
        NULL
    )))
    {
        _tprintf_s(TEXT("Unexpected error: Could not create second thread for client.\n"));
        goto cleanup;
    }

    while (gAllowConnection == 2);
    //as long as the client did not receive the approval or denial of the connection

    if (gAllowConnection)
    {
        if (NULL == (userThread = CreateThread(
            NULL,
            0,
            FromUser,
            (PVOID)client,
            0,
            NULL
        )))
        {
            _tprintf_s(TEXT("Unexpected error: Could not create thread for client.\n"));
            goto cleanup;
        }

        WaitForSingleObject(userThread, INFINITE);
    }

    WaitForSingleObject(serverThread, INFINITE);

cleanup:
    free(gLoggedUserName);
    DestroyClient(client);
    UninitCommunicationModule();

    return 0;
}

enum InputType FindInputType(TCHAR* Input)
{
    if (_tcscmp(Input, TEXT("exit")) == 0)
    {
        return Exit;
    }
    else if (_tcscmp(Input, TEXT("list")) == 0)
    {
        return List;
    }
    else if (_tcscmp(Input, TEXT("logout")) == 0)
    {
        return Logout;
    }
    else if (_tcsstr(Input, TEXT("echo")) == Input)
    {
        return Echo;
    }
    else if (_tcsstr(Input, TEXT("register")) == Input)
    {
        return Register;
    }
    else if (_tcsstr(Input, TEXT("login")) == Input)
    {
        return Login;
    }
    else if (_tcsstr(Input, TEXT("msg")) == Input)
    {
        return Msg;
    }
    else if (_tcsstr(Input, TEXT("broadcast")) == Input)
    {
        return Broadcast;
    }
    else if (_tcsstr(Input, TEXT("sendfile")) == Input)
    {
        return Sendfile;
    }
    else if (_tcsstr(Input, TEXT("history")) == Input)
    {
        return History;
    }
    return UnknownInput;
}

CM_ERROR ProcessEcho(TCHAR* Text, CM_CLIENT** Client)
{
    Text = Text + 5;
    SendCommandToServer(ECHO, Client);
    SendTextToServer(Text, Client);

    return CM_SUCCESS;
}

CM_ERROR ProcessRegister(TCHAR* Text, CM_CLIENT **Client)
{
    CM_ERROR error = CM_SUCCESS;
    TCHAR* name = _tcsstr(Text, TEXT(" "));
    name = name + 1;

    size_t nameLength = 0, passLength = 0;

    StringCchLength(name, 255, &nameLength);
    if (nameLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR* pass = _tcsstr(name, TEXT(" "));
    pass[0] = L'\0';
    pass = pass + 1;

    StringCchLength(pass, 255, &passLength);
    if (passLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    for (size_t i = 0; name[i]; ++i)
    {
        if (!isalnum(name[i]))
        {
            _tprintf_s(TEXT("Error: Invalid username\n"));
            return error;
        }
    }

    if (_tcsstr(pass, TEXT(" ")) || _tcsstr(pass, TEXT(",")))
    {
        _tprintf_s(TEXT("Error: Invalid password\n"));
        return error;
    }

    int upperChecker = 0, nonalphaChecker = 0;
    for (size_t i = 0; pass[i]; ++i)
    {
        if (isupper(pass[i]))
        {
            ++upperChecker;
        }
        else if (!isalnum(pass[i]))
        {
            ++nonalphaChecker;
        }
    }

    if (!upperChecker || !nonalphaChecker || passLength < 5)
    {
        _tprintf_s(TEXT("Error: Password too weak\n"));
        return error;
    }

    if (gLogged)
    {
        _tprintf_s(TEXT("Error: User already logged in\n"));
        return error;
    }

    SendCommandToServer(REGISTER, Client);
    SendTextToServer(name, Client);
    SendTextToServer(pass, Client);

    return error;
}

CM_ERROR ProcessExit(CM_CLIENT **Client)
{
    if (gLogged)
    {
        SendCommandToServer(EXITLOGGEDIN, Client);
        SendTextToServer(gLoggedUserName, Client);
    }
    else 
    {
        SendCommandToServer(EXIT, Client);
    }

    return CM_SUCCESS;
}

CM_ERROR SendTextToServer(TCHAR* Text, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;
    size_t toSendLength;

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_SIZE dataToSendSize = sizeof(int);

    StringCbLength(Text, STANDARD_SIZE, &toSendLength);

    //SEND LENGTH
    error = CreateDataBuffer(&dataToSend, dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)&toSendLength, (CM_SIZE)sizeof(size_t));
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    CM_SIZE sendBytesCount = 0;
    error = SendDataToServer(*Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }
    DestroyDataBuffer(dataToSend);

    //SEND TEXT
    error = CreateDataBuffer(&dataToSend, toSendLength);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Text, (CM_SIZE)toSendLength);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    error = SendDataToServer(*Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }
    DestroyDataBuffer(dataToSend);

    return error;
}

CM_ERROR SendCommandToServer(int Command, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_SIZE dataToSendSize = sizeof(int);

    error = CreateDataBuffer(&dataToSend, dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)&Command, (CM_SIZE)dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    CM_SIZE sendBytesCount = 0;
    error = SendDataToServer(*Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }
    DestroyDataBuffer(dataToSend);

    return error;
}

CM_ERROR ProcessLogin(TCHAR * Text, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;
    TCHAR* name = _tcsstr(Text, TEXT(" "));
    name = name + 1;

    size_t nameLength = 0, passLength = 0;

    StringCchLength(name, 255, &nameLength);
    if (nameLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR* pass = _tcsstr(name, TEXT(" "));
    pass[0] = L'\0';
    pass = pass + 1;

    StringCchLength(pass, 255, &passLength);
    if (passLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    if (gLogged)
    {
        _tprintf_s(TEXT("Error: Another user already logged in\n"));
        return error;
    }

    gLastOperation = LOGIN;
    SendCommandToServer(LOGIN, Client);
    SendTextToServer(name, Client);
    SendTextToServer(pass, Client);

    size_t sz;
    StringCbLength(name, 400, &sz);
    StringCbCopy(gLoggedUserName, sz + 2, name);

    return error;
}

CM_ERROR ProcessLogout(CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    if (!gLogged)
    {
        _tprintf_s(TEXT("Error: No user currently logged in\n"));
        return error;
    }

    --gLogged;

    SendCommandToServer(LOGOUT, Client);
    SendTextToServer(gLoggedUserName, Client);

    return error;
}

CM_ERROR ProcessList(CM_CLIENT** Client)
{
    SendCommandToServer(LIST, Client);
    return CM_SUCCESS;
}

CM_ERROR ProcessMsg(TCHAR* Text, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    if (!gLogged)
    {
        _tprintf_s(TEXT("Error: No user currently logged in\n"));
        return error;
    }

    TCHAR* name = _tcsstr(Text, TEXT(" "));
    name = name + 1;

    size_t nameLength = 0, textLength = 0;

    StringCchLength(name, 255, &nameLength);
    if (nameLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR* text = _tcsstr(name, TEXT(" "));
    text[0] = L'\0';
    text = text + 1;

    StringCchLength(text, 255, &textLength);
    if (textLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR *toWrite = (TCHAR*)calloc(sizeof(TCHAR), 800);
    if (NULL == toWrite)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        return error = CM_NO_MEMORY;
    }

    StringCbCat(toWrite, 790, TEXT("Message from "));
    StringCbCat(toWrite, 760, gLoggedUserName);
    StringCbCat(toWrite, 500, TEXT(": "));
    StringCbCat(toWrite, 399, text);

    SendCommandToServer(MSG, Client);
    SendTextToServer(gLoggedUserName, Client);
    SendTextToServer(name, Client);
    SendTextToServer(toWrite, Client);

    free(toWrite);
    return error;
}

CM_ERROR ProcessBroadcast(TCHAR* Text, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    if (!gLogged)
    {
        _tprintf_s(TEXT("Error: No user currently logged in\n"));
        return error;
    }

    TCHAR* text = _tcsstr(Text, TEXT(" "));
    text = text + 1;

    size_t textLength = 0;

    StringCchLength(text, 255, &textLength);
    if (textLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR *toWrite = (TCHAR*)calloc(sizeof(TCHAR), 800);
    if (NULL == toWrite)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        return error = CM_NO_MEMORY;
    }

    StringCbCat(toWrite, 790, TEXT("Broadcast from "));
    StringCbCat(toWrite, 760, gLoggedUserName);
    StringCbCat(toWrite, 500, TEXT(": "));
    StringCbCat(toWrite, 399, text);

    SendCommandToServer(BROADCAST, Client);
    SendTextToServer(gLoggedUserName, Client);
    SendTextToServer(toWrite, Client);

    free(toWrite);
    return error;
}

CM_ERROR ProcessHistory(TCHAR* Text, CM_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    if (!gLogged)
    {
        _tprintf_s(TEXT("Error: No user currently logged in\n"));
        return error;
    }

    TCHAR* name = _tcsstr(Text, TEXT(" "));
    name = name + 1;

    size_t nameLength = 0;

    StringCchLength(name, 255, &nameLength);
    if (nameLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    TCHAR* number = _tcsstr(name, TEXT(" "));
    number[0] = '\0';
    number = number + 1;

    size_t numberLength = 0;

    StringCchLength(number, 255, &numberLength);
    if (numberLength < 1)
    {
        return error = CM_INVALID_PARAMETER;
    }

    SendCommandToServer(HISTORY, Client);
    SendTextToServer(gLoggedUserName, Client);
    SendTextToServer(name, Client);
    SendTextToServer(number, Client);

    return error;
}