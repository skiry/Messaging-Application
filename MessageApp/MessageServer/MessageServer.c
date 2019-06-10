// MessageServer.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "CCHashTable.h"

// communication library
#include "communication_api.h"

#include <windows.h>
#include <strsafe.h>

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

HANDLE gAccountsFile;
CC_HASH_TABLE* gUsersMapping;
CC_HASH_TABLE* gUsersFiles;
CM_SERVER* gServer;
CM_ERROR RegisterClient(CM_SERVER_CLIENT**);
CM_ERROR ProcessEcho(CM_SERVER_CLIENT**);
CM_ERROR SendTextToClient(CM_SERVER_CLIENT**, TCHAR*); 
CM_ERROR SendExitToClient(CM_SERVER_CLIENT**, int);
CM_ERROR ReceiveText(CM_SERVER_CLIENT**, TCHAR**);
CM_ERROR LoginClient(CM_SERVER_CLIENT**);
CM_ERROR LogoutClient(CM_SERVER_CLIENT**);
CM_ERROR ListClients(CM_SERVER_CLIENT**);
CM_ERROR ProcessMsg(CM_SERVER_CLIENT**);
CM_ERROR ProcessBroadcast(CM_SERVER_CLIENT**);
CM_ERROR ProcessHistory(CM_SERVER_CLIENT**);
void OpenAccountsFile(HANDLE*);
void PrintBuffer(CM_DATA_BUFFER*);
void CloseFileHandles();
int CheckRegisteredUser(TCHAR*);
SRWLOCK g_srw_UsersMapping;
SRWLOCK g_srw_AccountsFile;
SRWLOCK g_srw_UsersFiles;
CRITICAL_SECTION g_cs_ConnectedUsers;
int gConnectedUsers;

DWORD WINAPI ServeOneClient(PVOID Client)
{
    DWORD result = 0;
    CM_SERVER_CLIENT *client = (CM_SERVER_CLIENT*)Client;
    CM_ERROR error = CM_SUCCESS;

    CM_DATA_BUFFER* dataToReceiveType = NULL;
    CM_SIZE dataToReceiveTypeSize = sizeof(int);
    int type;

    error = CreateDataBuffer(&dataToReceiveType, dataToReceiveTypeSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
        AbandonClient(client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    int clientExitted = 0;
    while (!clientExitted)
    {
        CM_SIZE receivedByteCount = 0;
        error = ReceiveDataFromClient(client, dataToReceiveType, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
            DestroyDataBuffer(dataToReceiveType);
            AbandonClient(client);
            DestroyServer(gServer);
            UninitCommunicationModule();
            return error;
        }
        type = *(int*)(dataToReceiveType->DataBuffer);

        switch (type)
        {
        case REGISTER:
            RegisterClient(&client);
            break;
        case EXIT:
            ++clientExitted;
            SendExitToClient(&client, EXIT);
            break;
        case ECHO:
            ProcessEcho(&client);
            break;
        case LOGIN:
            LoginClient(&client);
            break;
        case LOGOUT:
            LogoutClient(&client);
            break;
        case LIST:
            ListClients(&client);
            break;
        case EXITLOGGEDIN:
            ++clientExitted;
            SendExitToClient(&client, EXITLOGGEDIN);
            break;
        case MSG:
            ProcessMsg(&client);
            break;
        case BROADCAST:
            ProcessBroadcast(&client);
            break;
        case HISTORY:
            ProcessHistory(&client);
            break;
        default:
            break;
        }
    }
    DestroyDataBuffer(dataToReceiveType);

    EnterCriticalSection(&g_cs_ConnectedUsers);
    --gConnectedUsers;
    LeaveCriticalSection(&g_cs_ConnectedUsers);

    AbandonClient(client);
    return result;
}

int _tmain(int argc, TCHAR* argv[])
{
    (void)argc;
    (void)argv;
    TCHAR clientsNumber[24], toCheck[24];
    int clients;

    EnableCommunicationModuleLogger();

    if (argc == 2)
    {
        StringCbCopy(clientsNumber, 24, argv[1]);

        clients = _tstoi(clientsNumber);
        _itot_s(clients, toCheck, 24, 10);

        if (_tcscmp(clientsNumber, toCheck))
        {

            _tprintf_s(TEXT("Error: invalid maximum number of connections\n"));
            return -1;
        }
        else
        {
            _tprintf_s(TEXT("Success\n"));
        }
    }
    else
    {
        _tprintf_s(TEXT("Unexpected error: No maximum number of clients given\n"));
        return -1;
    }

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\n"), error);
        return -1;
    }

    gServer = NULL;
    error = CreateServer(&gServer);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CreateServer failed with err-code=0x%X!\n"), error);
        UninitCommunicationModule();
        return -1;
    }

    gUsersMapping = NULL;
    if (0 != HtCreate(&gUsersMapping))
    {
        printf("HtCreate failed!\n");
        DestroyServer(gServer);
        UninitCommunicationModule();
        return -1;
    }

    gUsersFiles = NULL;
    if (0 != HtCreate(&gUsersFiles))
    {
        printf("HtCreate failed!\n");
        HtDestroy(&gUsersMapping);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return -1;
    }

    OpenAccountsFile(&gAccountsFile);
    InitializeSRWLock(&g_srw_UsersMapping);
    InitializeSRWLock(&g_srw_AccountsFile);
    InitializeSRWLock(&g_srw_UsersFiles);
    InitializeCriticalSection(&g_cs_ConnectedUsers);

    int threadsSize = clients * 100;
    HANDLE* threads = (HANDLE*)calloc(sizeof(HANDLE), threadsSize);
    if (NULL == threads)
    {
        error = CM_NO_MEMORY;
        _tprintf_s(TEXT("Unexpected error: Not enough memory. err-code=0x%X!\n"), error);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return -1;
    }

    int threadCounter = 0;
    while (TRUE)
    {
        CM_SERVER_CLIENT* newClient = NULL;
        error = AwaitNewClient(gServer, &newClient);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: AwaitNewClient failed with err-code=0x%X!\n"), error);
            goto cleanup;
        }

        if (gConnectedUsers < clients)
        {
            if (threadCounter == threadsSize)
            {
                threadsSize *= 10;
                threads = (HANDLE*)realloc(threads, sizeof(HANDLE) * threadsSize);
                if (NULL == threads)
                {
                    error = CM_NO_MEMORY;
                    _tprintf_s(TEXT("Unexpected error: Not enough memory. err-code=0x%X!\n"), error);
                    goto cleanup;
                }
            }
            if (NULL == (threads[threadCounter++] = CreateThread(
                NULL,
                0,
                ServeOneClient,
                newClient,
                0,
                NULL)))
            {
                _tprintf_s(TEXT("Unexpected error: Could not create thread for a new client.\n"));
                goto cleanup;
            }
            EnterCriticalSection(&g_cs_ConnectedUsers);
            ++gConnectedUsers;
            LeaveCriticalSection(&g_cs_ConnectedUsers);
            SendTextToClient(&newClient, TEXT("Successful connection\n"));
        }
        else
        {
            SendTextToClient(&newClient, TEXT("Error: maximum concurrent connection count reached\n"));
            AbandonClient(newClient);
        }
    }

    WaitForMultipleObjects(
        threadCounter,
        threads,
        TRUE,
        INFINITE
    );

cleanup:
    free(threads);
    HtDestroy(&gUsersFiles);
    HtDestroy(&gUsersMapping);
    CloseHandle(gAccountsFile);
    CloseFileHandles();
    DeleteCriticalSection(&g_cs_ConnectedUsers);
    DestroyServer(gServer);
    UninitCommunicationModule();

    return 0;
}

CM_ERROR RegisterClient(CM_SERVER_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    TCHAR* name = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == name)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        return error = CM_NO_MEMORY;
    }
    TCHAR* pass = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == pass)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        free(name);
        return error = CM_NO_MEMORY;
    }

    ReceiveText(Client, &name);
    ReceiveText(Client, &pass);

    if (CheckRegisteredUser(name)) 
    {
        SendTextToClient(Client, TEXT("Error: Username already registered\n"));
        goto cleanup;
    }

    TCHAR *toWrite = (TCHAR*)calloc(sizeof(TCHAR), 900);
    if (NULL == toWrite)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        error = CM_NO_MEMORY;
        goto cleanup;
    }

    DWORD writtenLen = 0;
    size_t toWriteLen = 0;

    StringCbCat(toWrite, 900, name);
    StringCbCat(toWrite, 900, TEXT(","));
    StringCbCat(toWrite, 900, pass);
    StringCbCat(toWrite, 900, TEXT("\r\n"));
    StringCbLength(toWrite, 900, &toWriteLen);

    AcquireSRWLockExclusive(&g_srw_AccountsFile);

    WriteFile(
        gAccountsFile,
        toWrite,
        toWriteLen,
        &writtenLen,
        NULL);

    ReleaseSRWLockExclusive(&g_srw_AccountsFile);

    SendTextToClient(Client, TEXT("Success\n"));

    free(toWrite);

cleanup:
    free(name);
    free(pass);

    return error;
}

CM_ERROR SendExitToClient(CM_SERVER_CLIENT** Client, int LoggedIn)
{
    CM_ERROR error = CM_SUCCESS;

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_SIZE dataToSendSize = sizeof(int);
    CM_SIZE receivedByteCount = 0;
    int toSendLength = sizeof(int), exit = 0;

    if (EXITLOGGEDIN == LoggedIn)
    {
        TCHAR* userName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
        if (NULL == userName)
        {
            _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
            return error = CM_NO_MEMORY;
        }
        ReceiveText(Client, &userName);
           
        AcquireSRWLockExclusive(&g_srw_UsersMapping);
        HtRemoveKey(gUsersMapping, userName);
        ReleaseSRWLockExclusive(&g_srw_UsersMapping);

        free(userName);
    }

    //SEND SIZE OF INT. IT SPECIFIES THAT THE NEXT THING WE SEND IS AN INTEGER
    error = CreateDataBuffer(&dataToSend, dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)&toSendLength, (CM_SIZE)dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    error = SendDataToClient(*Client, dataToSend, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    DestroyDataBuffer(dataToSend);

    //SEND THE ACTUAL VALUE
    error = CreateDataBuffer(&dataToSend, dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)&exit, (CM_SIZE)dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    error = SendDataToClient(*Client, dataToSend, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    DestroyDataBuffer(dataToSend);
    return error;
}

CM_ERROR ReceiveText(CM_SERVER_CLIENT** Client, TCHAR** Text)
{
    CM_ERROR error = CM_SUCCESS;
    CM_DATA_BUFFER* dataToReceive = NULL;
    CM_SIZE dataToReceiveSize = sizeof(int);
    CM_SIZE receivedByteCount = 0;
    int length;

    error = CreateDataBuffer(&dataToReceive, dataToReceiveSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = ReceiveDataFromClient(*Client, dataToReceive, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToReceive);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    length = *(int*)(dataToReceive->DataBuffer);

    DestroyDataBuffer(dataToReceive);

    error = CreateDataBuffer(&dataToReceive, length);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = ReceiveDataFromClient(*Client, dataToReceive, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToReceive);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    StringCbCopy(*Text, length + 2, (STRSAFE_LPCWSTR)dataToReceive->DataBuffer);
    //a TCHAR is 2 bytes. we must consider also the null character.
        
    DestroyDataBuffer(dataToReceive);
    return error;
}

CM_ERROR SendTextToClient(CM_SERVER_CLIENT** Client, TCHAR* Text)
{
    CM_ERROR error = CM_SUCCESS;

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_SIZE dataToSendSize = sizeof(size_t);
    CM_SIZE receivedByteCount = 0;
    size_t toSendLength;

    StringCbLength(Text, STANDARD_SIZE, &toSendLength);

    //SEND THE SIZE OF THE TEXT. 
    error = CreateDataBuffer(&dataToSend, dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)&toSendLength, (CM_SIZE)dataToSendSize);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    error = SendDataToClient(*Client, dataToSend, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    DestroyDataBuffer(dataToSend);

    //SEND THE ACTUAL VALUE
    error = CreateDataBuffer(&dataToSend, toSendLength);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Text, (CM_SIZE)toSendLength);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return error;
    }

    error = SendDataToClient(*Client, dataToSend, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        AbandonClient(*Client);
        DestroyServer(gServer);
        UninitCommunicationModule();
        return error;
    }

    DestroyDataBuffer(dataToSend);
    return error;
}

CM_ERROR ProcessEcho(CM_SERVER_CLIENT** Client)
{
    TCHAR* text = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == text)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    ReceiveText(Client, &text);
    StringCbCat(text, 400, TEXT("\n"));
    _tprintf_s(TEXT("%s"), text);
    SendTextToClient(Client, text);

    free(text);
    return CM_SUCCESS;
}

void PrintBuffer(CM_DATA_BUFFER* Buffer)
{
    unsigned int i = 0;
    while (i < Buffer->UsedBufferSize)
    {
        printf("%c", Buffer->DataBuffer[i]);
        i += 2;
    }

    printf("\n");
}

CM_ERROR LoginClient(CM_SERVER_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;

    TCHAR* name = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == name)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        return error = CM_NO_MEMORY;
    }
    TCHAR* pass = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == pass)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        free(name);
        return error = CM_NO_MEMORY;
    }

    ReceiveText(Client, &name);
    ReceiveText(Client, &pass);

    DWORD readLength = 0;
    TCHAR* userName;
    int eof = 0, validCombination = 0, goodName = 0;

    AcquireSRWLockExclusive(&g_srw_AccountsFile);
    int size = min(GetFileSize(gAccountsFile, NULL), STANDARD_SIZE);
    size += 2;

    TCHAR* accounts = (TCHAR*)malloc(size);
    if (NULL == accounts)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        free(name);
        free(pass);
        return error = CM_NO_MEMORY;
    }
    if (INVALID_SET_FILE_POINTER == SetFilePointer(
        gAccountsFile,
        0,
        NULL,
        FILE_BEGIN
    ))
    {
        _tprintf_s(TEXT("Unexpected error: Could not move cursor to the beggining of file!\n"));
        return -1;
    }
    ReleaseSRWLockExclusive(&g_srw_AccountsFile);

    AcquireSRWLockShared(&g_srw_AccountsFile);
    while (1)
    {
        ReadFile(
            gAccountsFile,
            accounts,
            size,
            &readLength,
            NULL
        );

        if (readLength < 6)
        {
            break;
        }

        accounts[(int)readLength / sizeof(accounts[0])] = '\0';

        int toAdd = 0, userLen = 0, firstTime = 1;

        while (!eof)
        {
            userName = _tcsstr(accounts + toAdd, TEXT(","));

            if (NULL == userName)
            {
                eof = 1;
                break;
            }
            userName[0] = '\0';

            if (!firstTime)
            {
                ++toAdd;
            }
            else
            {
                firstTime = 0;
                --toAdd;
            }
            ++toAdd;

            userLen = _tcslen(accounts + toAdd);

            if (_tcscmp(accounts + toAdd, name) == 0)
            {
                goodName = 1;
            }

            int firstHere = toAdd;
            int i, passCounter = 0, allowCounter = 0;

            for (i = toAdd; accounts[i] != 13 && !eof; ++i)
            {
                if (allowCounter)
                {
                    ++passCounter;
                }
                if (accounts[i] == 0)
                {
                    ++allowCounter;
                }

                if (accounts[i] == '\0' && firstHere + (int)userLen != toAdd)
                {
                    eof = 1;
                }
                ++toAdd;
            }
            
            if (goodName)
            {
                accounts[i] = 0;

                if (_tcscmp(accounts + toAdd - passCounter, pass) == 0)
                {
                    ++validCombination;
                    ++eof;
                    break;
                }

                accounts[i] = 13;
                goodName = 0;
            }
        }
    }

    ReleaseSRWLockShared(&g_srw_AccountsFile);

    if (!validCombination)
    {
        SendTextToClient(Client, TEXT("Error: Invalid username / password combination\n"));
        goto cleanup;
    }

    AcquireSRWLockShared(&g_srw_UsersMapping);
    int exists = HtHasKey(gUsersMapping, name);
    ReleaseSRWLockShared(&g_srw_UsersMapping);

    if (1 == exists)
    {
        SendTextToClient(Client, TEXT("Error: User already logged in\n"));
        goto cleanup;
    }
    else
    {
        AcquireSRWLockExclusive(&g_srw_UsersMapping);
        HtSetKeyValue(gUsersMapping, name, *Client);
        ReleaseSRWLockExclusive(&g_srw_UsersMapping);
    }

    SendTextToClient(Client, TEXT("Success\n"));

    free(accounts);

    TCHAR* path = (TCHAR*)calloc(sizeof(TCHAR), 300);
    if (NULL == path)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    StringCbCat(path, 300, TEXT("C:\\Users\\"));
    //StringCbCat(path, 300, userName);
    //StringCbCat(path, 300, TEXT("\\"));
    StringCbCat(path, 300, name);
    StringCbCat(path, 300, TEXT(".txt"));

    size = min(GetFileSize(gAccountsFile, NULL), STANDARD_SIZE);
    size += 2;

    accounts = (TCHAR*)malloc(size);
    if (NULL == accounts)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory. 0x%X!\n"), error);
        return error = CM_NO_MEMORY;
    }

    readLength = 0;

    HANDLE* userFile = (HANDLE*)malloc(sizeof(HANDLE));
    HANDLE cFile;

    AcquireSRWLockShared(&g_srw_UsersFiles);
    
    if (HtHasKey(gUsersFiles, name) == 0)
    {
        ReleaseSRWLockShared(&g_srw_UsersFiles);
        AcquireSRWLockExclusive(&g_srw_UsersFiles);

        if (!HtHasKey(gUsersFiles, name))
        {
            while (INVALID_HANDLE_VALUE == (cFile = CreateFile(
                (LPCWSTR)path,
                GENERIC_READ | FILE_APPEND_DATA,
                0,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL)));
            *userFile = cFile;
            HtSetKeyValue(gUsersFiles, name, userFile);
        }
        ReleaseSRWLockExclusive(&g_srw_UsersFiles);
    }
    else
    {
        ReleaseSRWLockShared(&g_srw_UsersFiles);
        free(userFile);
    }

    HANDLE* file = NULL;

    AcquireSRWLockShared(&g_srw_UsersFiles);
    HtGetKeyValue(gUsersFiles, name, (void**)&file);
    ReleaseSRWLockShared(&g_srw_UsersFiles);

    AcquireSRWLockExclusive(&g_srw_UsersFiles);
    if (INVALID_SET_FILE_POINTER == SetFilePointer(
        *file,
        0,
        NULL,
        FILE_BEGIN
    ))
    {
        _tprintf_s(TEXT("Unexpected error: Could not move cursor to the beggining of file!\n"));
        free(accounts);
        return -1;
    }
    ReleaseSRWLockExclusive(&g_srw_UsersFiles);

    while (1)
    {
        ReadFile(
            *file,
            accounts,
            size,
            &readLength,
            NULL
        );
        accounts[(int)readLength / sizeof(accounts[0])] = '\0';
        if (!readLength)
        {
            break;
        }
        SendTextToClient(Client, accounts);
    }

    CloseHandle(*file);
    DeleteFile(path);
    AcquireSRWLockExclusive(&g_srw_UsersFiles);
    HtRemoveKey(gUsersFiles, name);
    ReleaseSRWLockExclusive(&g_srw_UsersFiles);

cleanup:
    free(accounts);
    free(pass);
    free(name);
    return error;
}

void OpenAccountsFile(HANDLE* File)
{
    LPCWSTR fileName = TEXT("C:\\registration.txt");

    while (INVALID_HANDLE_VALUE == (*File = CreateFile(
        fileName,
        GENERIC_READ | FILE_APPEND_DATA,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL)));
}

CM_ERROR LogoutClient(CM_SERVER_CLIENT** Client)
{
    TCHAR* userName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == userName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory.!\n"));
        return CM_NO_MEMORY;
    }

    ReceiveText(Client, &userName);

    AcquireSRWLockExclusive(&g_srw_UsersMapping);
    HtRemoveKey(gUsersMapping, userName);
    ReleaseSRWLockExclusive(&g_srw_UsersMapping);

    SendTextToClient(Client, TEXT("Success\n"));

    free(userName);
    return CM_SUCCESS;
}

CM_ERROR ListClients(CM_SERVER_CLIENT** Client)
{
    int i = -1;
    TCHAR* tUserName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == tUserName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    AcquireSRWLockShared(&g_srw_UsersMapping);
    while (++i < HtGetKeyCount(gUsersMapping))
    {
        HtGetNthKey(gUsersMapping, i, &tUserName);
        StringCbCat(tUserName, 400, TEXT("\n"));
        SendTextToClient(Client, tUserName);
    }
    ReleaseSRWLockShared(&g_srw_UsersMapping);

    free(tUserName);
    return CM_SUCCESS;
}

CM_ERROR ProcessMsg(CM_SERVER_CLIENT** Client)
{
    int noUser = 0;
    TCHAR* userName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == userName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    TCHAR* text = (TCHAR*)malloc(sizeof(TCHAR) * 800);
    if (NULL == text)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(userName);
        return CM_NO_MEMORY;
    }

    TCHAR* fromUserName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == fromUserName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(userName);
        free(text);
        return CM_NO_MEMORY;
    }

    ReceiveText(Client, &fromUserName);
    ReceiveText(Client, &userName);
    ReceiveText(Client, &text);

    //write to history
    TCHAR* historyPath = (TCHAR*)malloc(sizeof(TCHAR) * 900);
    if (NULL == historyPath)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(fromUserName);
        free(userName);
        free(text);
        return CM_NO_MEMORY;
    }

    TCHAR* historyText = (TCHAR*)malloc(sizeof(TCHAR) * 800);
    if (NULL == historyText)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(historyPath);
        free(fromUserName);
        free(userName);
        free(text);
        return CM_NO_MEMORY;
    }

    StringCbCopy(historyPath, 900, TEXT("C:\\Users\\"));

    if (_tcscmp(fromUserName, userName) < 0)
    {
        StringCbCat(historyPath, 900, fromUserName);
        StringCbCat(historyPath, 900, userName);
    }
    else
    {
        StringCbCat(historyPath, 900, userName);
        StringCbCat(historyPath, 900, fromUserName);
    }

    size_t toWriteLen = 0;
    StringCbCat(historyPath, 900, TEXT(".txt"));
    StringCbCopy(historyText, 800, text);
    StringCbCat(historyText, 800, TEXT("\r\n"));
    StringCbLength(historyText, 800, &toWriteLen);

    HANDLE cFile;
    AcquireSRWLockExclusive(&g_srw_UsersFiles);
    while (INVALID_HANDLE_VALUE == (cFile = CreateFile(
        (LPCWSTR)historyPath,
        GENERIC_READ | FILE_APPEND_DATA,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL)));

    DWORD writtenLen = 0;

    WriteFile(
        cFile,
        historyText,
        toWriteLen,
        &writtenLen,
        NULL);

    CloseHandle(cFile);
    ReleaseSRWLockExclusive(&g_srw_UsersFiles);

    if (_tcscmp(fromUserName, userName) == 0)
    {
        // if the sender sent himself a message
        SendTextToClient(Client, TEXT("Error: No such user\n"));
        goto cleanup;
    }

    AcquireSRWLockShared(&g_srw_UsersMapping);
    if (1 == HtHasKey(gUsersMapping, userName))
    {
        CM_SERVER_CLIENT* clientToSend;
        HtGetKeyValue(gUsersMapping, userName, &clientToSend);
        ReleaseSRWLockShared(&g_srw_UsersMapping);
        StringCbCat(text, 800, TEXT("\n"));
        SendTextToClient(&clientToSend, text);
    }
    else
    {
        ReleaseSRWLockShared(&g_srw_UsersMapping);
        if (0 == CheckRegisteredUser(userName))
        {
            SendTextToClient(Client, TEXT("Error: No such user\n"));
            ++noUser;
        }
        else
        {
            HANDLE* userFile = (HANDLE*)malloc(sizeof(HANDLE));
            TCHAR* path = (TCHAR*)calloc(sizeof(TCHAR), 300);
            if (NULL == path)
            {
                _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
                free(userName);
                free(text);
                free(fromUserName);
                return CM_NO_MEMORY;
            }
            StringCbCat(path, 300, TEXT("C:\\Users\\"));
            //StringCbCat(path, 300, userName);
            //StringCbCat(path, 300, TEXT("\\"));
            StringCbCat(path, 300, userName);
            StringCbCat(path, 300, TEXT(".txt"));

            AcquireSRWLockShared(&g_srw_UsersFiles);
            if (HtHasKey(gUsersFiles, userName) == 0)
            {
                ReleaseSRWLockShared(&g_srw_UsersFiles);
                AcquireSRWLockExclusive(&g_srw_UsersFiles);

                if (!HtHasKey(gUsersFiles, userName))
                {
                    while (INVALID_HANDLE_VALUE == (cFile = CreateFile(
                        (LPCWSTR)path,
                        GENERIC_READ | FILE_APPEND_DATA,
                        0,
                        NULL,
                        OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL)));
                    *userFile = cFile;
                    HtSetKeyValue(gUsersFiles, userName, userFile);
                }
                ReleaseSRWLockExclusive(&g_srw_UsersFiles);
            }
            else
            {
                ReleaseSRWLockShared(&g_srw_UsersFiles);
            }
            
            HANDLE* file = NULL;
            AcquireSRWLockShared(&g_srw_UsersFiles);
            HtGetKeyValue(gUsersFiles, userName, (void**)&file);
            ReleaseSRWLockShared(&g_srw_UsersFiles);

            writtenLen = 0;
            toWriteLen = 0;

            StringCbCat(text, 800, TEXT("\r\n"));
            StringCbLength(text, 800, &toWriteLen);

            AcquireSRWLockExclusive(&g_srw_UsersFiles);

            WriteFile(
                *file,
                text,
                toWriteLen,
                &writtenLen,
                NULL);

            ReleaseSRWLockExclusive(&g_srw_UsersFiles);

            free(path);
        }
    }

    if (!noUser) 
    {
        SendTextToClient(Client, TEXT("Success\n"));
    }

cleanup:
    free(historyText);
    free(historyPath);
    free(fromUserName);
    free(text);
    free(userName);
    return CM_SUCCESS;
}

int CheckRegisteredUser(TCHAR* Name)
{
    int size = min(GetFileSize(gAccountsFile, NULL), STANDARD_SIZE);
    size += 2;

    TCHAR* accounts = (TCHAR*)malloc(size);
    if (NULL == accounts)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return -1;
    }

    DWORD readLength = 0;
    TCHAR* userName;
    int eof = 0, userExists = 0;


    AcquireSRWLockExclusive(&g_srw_AccountsFile);
    if (INVALID_SET_FILE_POINTER == SetFilePointer(
        gAccountsFile,
        0,
        NULL,
        FILE_BEGIN
    ))
    {
        _tprintf_s(TEXT("Unexpected error: Could not move cursor to the beggining of file!\n"));
        free(accounts);
        return -1;
    }
    ReleaseSRWLockExclusive(&g_srw_AccountsFile);

    AcquireSRWLockShared(&g_srw_AccountsFile);
    while (1)
    {
        ReadFile(
            gAccountsFile,
            accounts,
            size,
            &readLength,
            NULL
        );

        if (readLength < 6)
        {
            break;
        }

        accounts[(int)readLength / sizeof(accounts[0])] = '\0';
        int toAdd = 0, userLen = 0, firstTime = 1;

        while (!eof)
        {
            userName = _tcsstr(accounts + toAdd, TEXT(","));
            if (NULL == userName)
            {
                eof = 1;
                break;
            }
            userName[0] = '\0';

            if (!firstTime)
            {
                ++toAdd;
            }
            else
            {
                firstTime = 0;
                --toAdd;
            }
            ++toAdd;

            userLen = _tcslen(accounts + toAdd);

            if (_tcscmp(accounts + toAdd, Name) == 0)
            {
                userExists = 1;
                eof = 1;
                break;
            }

            int firstHere = toAdd;
            for (int i = toAdd; accounts[i] != 13 && !eof; ++i)
            {
                if (accounts[i] == '\0' && firstHere + (int)userLen != toAdd)
                {
                    eof = 1;
                }
                ++toAdd;
            }
        }
    }

    ReleaseSRWLockShared(&g_srw_AccountsFile);

    free(accounts);

    return userExists;
}

void CloseFileHandles()
{
    int i = -1;
    HANDLE* file;
    TCHAR* userName = (TCHAR*)malloc(sizeof(TCHAR) * 300);
    if (NULL == userName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return;
    }

    while (++i < HtGetKeyCount(gUsersFiles))
    {
        HtGetNthKey(gUsersFiles, i, &userName);
        HtGetKeyValue(gUsersFiles, userName, (void**)&file);
        CloseHandle(*file);
        free(*file);
    }

    free(userName);
}

CM_ERROR ProcessBroadcast(CM_SERVER_CLIENT** Client)
{
    TCHAR* text = (TCHAR*)malloc(sizeof(TCHAR) * 800);
    if (NULL == text)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    TCHAR* textForFile = (TCHAR*)malloc(sizeof(TCHAR) * 800);
    if (NULL == text)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(text);
        return CM_NO_MEMORY;
    }

    TCHAR* fromUserName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == fromUserName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(textForFile);
        free(text);
        return CM_NO_MEMORY;
    }

    ReceiveText(Client, &fromUserName);
    ReceiveText(Client, &text);
    StringCbCopy(textForFile, 800, text);

    StringCbCat(text, 800, TEXT("\n"));
    StringCbCat(textForFile, 800, TEXT("\r\n"));

    int size = min(GetFileSize(gAccountsFile, NULL), STANDARD_SIZE);
    size += 2;

    TCHAR* accounts = (TCHAR*)malloc(size);
    if (NULL == accounts)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return -1;
    }

    DWORD readLength = 0;
    TCHAR* userName;
    int eof = 0;


    AcquireSRWLockExclusive(&g_srw_AccountsFile);
    if (INVALID_SET_FILE_POINTER == SetFilePointer(
        gAccountsFile,
        0,
        NULL,
        FILE_BEGIN
    ))
    {
        _tprintf_s(TEXT("Unexpected error: Could not move cursor to the beggining of file!\n"));
        free(accounts);
        return -1;
    }
    ReleaseSRWLockExclusive(&g_srw_AccountsFile);

    AcquireSRWLockShared(&g_srw_AccountsFile);
    while (1)
    {
        ReadFile(
            gAccountsFile,
            accounts,
            size,
            &readLength,
            NULL
        );

        if (readLength < 6)
        {
            break;
        }

        accounts[(int)readLength / sizeof(accounts[0])] = '\0';
        int toAdd = 0, userLen = 0, firstTime = 1;

        while (!eof)
        {
            userName = _tcsstr(accounts + toAdd, TEXT(","));
            if (NULL == userName)
            {
                eof = 1;
                break;
            }
            userName[0] = '\0';

            if (!firstTime)
            {
                ++toAdd;
            }
            else
            {
                firstTime = 0;
                --toAdd;
            }
            ++toAdd;

            userLen = _tcslen(accounts + toAdd);

            if (_tcscmp(accounts + toAdd, fromUserName))
            {
                AcquireSRWLockShared(&g_srw_UsersMapping);
                if (HtHasKey(gUsersMapping, accounts + toAdd))
                {
                    CM_SERVER_CLIENT* clientToSend;
                    HtGetKeyValue(gUsersMapping, accounts + toAdd, &clientToSend);
                    ReleaseSRWLockShared(&g_srw_UsersMapping);
                    SendTextToClient(&clientToSend, text);
                }
                else
                {
                    ReleaseSRWLockShared(&g_srw_UsersMapping);

                    HANDLE* userFile = (HANDLE*)malloc(sizeof(HANDLE));
                    HANDLE cFile;
                    TCHAR* path = (TCHAR*)calloc(sizeof(TCHAR), 300);
                    if (NULL == path)
                    {
                        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
                        free(text);
                        free(fromUserName);
                        free(accounts);
                        return CM_NO_MEMORY;
                    }
                    StringCbCat(path, 300, TEXT("C:\\Users\\"));
                    //StringCbCat(path, 300, userName);
                    //StringCbCat(path, 300, TEXT("\\"));
                    StringCbCat(path, 300, accounts + toAdd);
                    StringCbCat(path, 300, TEXT(".txt"));

                    AcquireSRWLockShared(&g_srw_UsersFiles);
                    if (HtHasKey(gUsersFiles, accounts + toAdd) == 0)
                    {
                        ReleaseSRWLockShared(&g_srw_UsersFiles);
                        AcquireSRWLockExclusive(&g_srw_UsersFiles);

                        if (!HtHasKey(gUsersFiles, accounts + toAdd))
                        {
                            while (INVALID_HANDLE_VALUE == (cFile = CreateFile(
                                (LPCWSTR)path,
                                GENERIC_READ | FILE_APPEND_DATA,
                                0,
                                NULL,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL)));
                            *userFile = cFile;
                            HtSetKeyValue(gUsersFiles, accounts + toAdd, userFile);
                        }
                        ReleaseSRWLockExclusive(&g_srw_UsersFiles);
                    }
                    else
                    {
                        ReleaseSRWLockShared(&g_srw_UsersFiles);
                    }

                    HANDLE* file = NULL;
                    AcquireSRWLockShared(&g_srw_UsersFiles);
                    HtGetKeyValue(gUsersFiles, accounts + toAdd, (void**)&file);
                    ReleaseSRWLockShared(&g_srw_UsersFiles);

                    DWORD writtenLen = 0;
                    size_t toWriteLen = 0;

                    StringCbLength(textForFile, 800, &toWriteLen);

                    AcquireSRWLockExclusive(&g_srw_UsersFiles);

                    WriteFile(
                        *file,
                        textForFile,
                        toWriteLen,
                        &writtenLen,
                        NULL);

                    ReleaseSRWLockExclusive(&g_srw_UsersFiles);
                    free(path);
                }
            }

            int firstHere = toAdd;
            for (int i = toAdd; accounts[i] != 13 && !eof; ++i)
            {
                if (accounts[i] == '\0' && firstHere + (int)userLen != toAdd)
                {
                    eof = 1;
                }
                ++toAdd;
            }
        }
    }

    ReleaseSRWLockShared(&g_srw_AccountsFile);

    free(accounts);

    SendTextToClient(Client, TEXT("Success\n"));

    free(fromUserName);
    free(textForFile);
    free(text);
    return CM_SUCCESS;
}

CM_ERROR ProcessHistory(CM_SERVER_CLIENT** Client)
{
    CM_ERROR error = CM_SUCCESS;
    
    TCHAR* userName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == userName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return CM_NO_MEMORY;
    }

    TCHAR* number = (TCHAR*)malloc(sizeof(TCHAR) * 100);
    if (NULL == number)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(userName);
        return CM_NO_MEMORY;
    }

    TCHAR* fromUserName = (TCHAR*)malloc(sizeof(TCHAR) * 400);
    if (NULL == fromUserName)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(userName);
        free(number);
        return CM_NO_MEMORY;
    }

    ReceiveText(Client, &fromUserName);
    ReceiveText(Client, &userName);
    ReceiveText(Client, &number);

    int numberInt = _ttoi(number);
     
    if (!CheckRegisteredUser(userName))
    {
        SendTextToClient(Client, TEXT("Error: No such user\n"));
        return error;
    }

    //read from history
    TCHAR* historyPath = (TCHAR*)malloc(sizeof(TCHAR) * 900);
    if (NULL == historyPath)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        free(fromUserName);
        free(userName);
        free(number);
        return CM_NO_MEMORY;
    }

    StringCbCopy(historyPath, 900, TEXT("C:\\Users\\"));

    if (_tcscmp(fromUserName, userName) < 0)
    {
        StringCbCat(historyPath, 900, fromUserName);
        StringCbCat(historyPath, 900, userName);
    }
    else
    {
        StringCbCat(historyPath, 900, userName);
        StringCbCat(historyPath, 900, fromUserName);
    }

    StringCbCat(historyPath, 900, TEXT(".txt"));

    HANDLE cFile;
    AcquireSRWLockExclusive(&g_srw_UsersFiles);
    while (INVALID_HANDLE_VALUE == (cFile = CreateFile(
        (LPCWSTR)historyPath,
        GENERIC_READ | FILE_APPEND_DATA,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL)));

    int size = min(GetFileSize(cFile, NULL), STANDARD_SIZE);
    size += 2;

    TCHAR* accounts = (TCHAR*)malloc(size);
    if (NULL == accounts)
    {
        _tprintf_s(TEXT("Unexpected error: Not enough memory!\n"));
        return -1;
    }

    DWORD readLength = 0;
    int eof = 0, foundSth = 0, overAll = 0;

    AcquireSRWLockShared(&g_srw_AccountsFile);

    while (1)
    {
        ReadFile(
            cFile,
            accounts,
            size,
            &readLength,
            NULL
        );

        if (readLength < 6)
        {
            break;
        }
        ++foundSth;

        accounts[(int)readLength / sizeof(accounts[0])] = '\0';
        int toAdd = 0, userLen = 0, firstTime = 1;

        while (!eof)
        {
            if (!firstTime)
            {
                ++toAdd;
            }
            else
            {
                firstTime = 0;
                --toAdd;
            }
            ++toAdd;

            if (accounts[toAdd] == 0)
            {
                eof = 1;
                break;
            }
            userLen = _tcslen(accounts + toAdd);

            int firstHere = toAdd;
            for (int i = toAdd; accounts[i] != 13 && !eof; ++i)
            {
                if (accounts[i] == '\0' && firstHere + (int)userLen != toAdd)
                {
                    eof = 1;
                }
                ++toAdd;
            }
            ++overAll;
        }

        if (overAll <= numberInt)
        {
            SendTextToClient(Client, accounts);
        }
        else
        {
            int counter = 0;
            toAdd = 0, userLen = 0, firstTime = 1, eof = 0;

            while (!eof)
            {
                if (!firstTime)
                {
                    ++toAdd;
                }
                else
                {
                    firstTime = 0;
                    --toAdd;
                }
                ++toAdd;
                if (overAll - counter == numberInt)
                {
                    SendTextToClient(Client, accounts + toAdd);
                    eof = 1;
                    break;
                }
                if (accounts[toAdd] == 0)
                {
                    eof = 1;
                    break;
                }
                userLen = _tcslen(accounts + toAdd);

                int firstHere = toAdd;
                for (int i = toAdd; accounts[i] != 13 && !eof; ++i)
                {
                    if (accounts[i] == '\0' && firstHere + (int)userLen != toAdd)
                    {
                        eof = 1;
                    }
                    ++toAdd;
                }
                ++counter;
            }
        }
    }

    ReleaseSRWLockShared(&g_srw_AccountsFile);

    free(accounts);
    CloseHandle(cFile);
    ReleaseSRWLockExclusive(&g_srw_UsersFiles);

    if (!foundSth)
    {
        SendTextToClient(Client, TEXT("\n"));
    }

    return error;
}