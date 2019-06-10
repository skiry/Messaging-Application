#pragma once
#include "communication_api.h"

typedef struct _HTNODE {
    struct _HTNODE* PNext;
    struct _HTNODE* PBack;
    TCHAR* Key;
    void* Value;
} HTNODE, *PHTNODE;

typedef struct _CC_HASH_TABLE {
    PHTNODE* Elems;
    int Size;
} CC_HASH_TABLE;

int HtCreate(CC_HASH_TABLE** HashTable);
int HtDestroy(CC_HASH_TABLE** HashTable);

int HtSetKeyValue(CC_HASH_TABLE* HashTable, TCHAR* Key, void* Value); //add pair <key,value>
int HtGetKeyValue(CC_HASH_TABLE* HashTable, TCHAR* Key, void** Value); //get value from pair <key,value>
int HtRemoveKey(CC_HASH_TABLE* HashTable, TCHAR* Key); //delete <key,value>
int HtHasKey(CC_HASH_TABLE* HashTable, TCHAR* Key);  //return 1 if key in hash table, 0 otherwise
int HtGetNthKey(CC_HASH_TABLE* HashTable, int Index, TCHAR** Key);
int HtClear(CC_HASH_TABLE* HashTable);
int HtGetKeyCount(CC_HASH_TABLE* HashTable);
int HtHash(TCHAR* Key); //computes the location in the hash table for a string