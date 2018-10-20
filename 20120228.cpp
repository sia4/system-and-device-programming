#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <io.h>
#include <float.h>

#pragma warning(disable: 4996)
#define MAX_EL 10

typedef struct _queue_el *LP_QUEUE_EL;
typedef struct _queue_el {
	TCHAR name[40 + 1];
	INT idC;
	INT id;
	FLOAT duration;
} QUEUE_EL;

typedef struct _queue *LPQUEUE;
typedef struct _queue{
	INT id;
	FLOAT min;
	FLOAT max;
	INT indexIn;
	INT indexOut;
	INT nEl;
	HANDLE meIn;
	HANDLE meOut;
	LP_QUEUE_EL fifo;
} QUEUE;

typedef struct {
	LPQUEUE buffer;
	INT nQueue;
} BUFFER;

typedef struct{
	BOOL val;
    CRITICAL_SECTION cs;
} END_STRUCT;

typedef struct {
	HANDLE sem;
	CRITICAL_SECTION cs;
} EF_STRUCT;

BUFFER b;
HANDLE hIn;
END_STRUCT endStruct;
EF_STRUCT emptyStruct;
EF_STRUCT fullStruct;

DWORD WINAPI client(LPVOID);
DWORD WINAPI server(LPVOID);
BOOL push_el(QUEUE_EL);
QUEUE_EL pull_el(VOID);

int _tmain(int argc, LPTSTR argv[]) {

	INT nServer;
	INT nClient;
	LPHANDLE hC, hS;
	LPINT ids;

	DWORD nR;
	INT i;
	
	if (argc != 2) {
		_ftprintf(stderr, _T("Missing parameters.\
			Usage: program inputFile M"));
		return -1;
	}

	//Inizialize all
	endStruct.val = FALSE;
	InitializeCriticalSection(&endStruct.cs);
	InitializeCriticalSection(&emptyStruct.cs);
	emptyStruct.sem = CreateSemaphore(NULL, MAX_EL, MAX_EL, NULL);
	fullStruct.sem = CreateSemaphore(NULL, 0, MAX_EL, NULL);
	//read file
	hIn = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error opening input file.\n"));
		return -1;
	}
	
	//first row
	ReadFile(hIn, &nServer, sizeof(INT), &nR, NULL);
	if (nR == 0) {
		_ftprintf(stderr, _T("Error reading input file.\n"));
		return -1;
	}
	ReadFile(hIn, &nClient, sizeof(INT), &nR, NULL);
	if (nR == 0) {
		_ftprintf(stderr, _T("Error reading input file.\n"));
		return -1;
	}
	ReadFile(hIn, &b.nQueue, sizeof(INT), &nR, NULL);
	if (nR == 0) {
		_ftprintf(stderr, _T("Error reading input file.\n"));
		return -1;
	}

	b.buffer = (LPQUEUE)malloc(b.nQueue * sizeof(QUEUE));
	
	//second row
	for (i = 0; i < b.nQueue; i++) {
		b.buffer[i].id = i;
		b.buffer[i].indexIn = 0;
		b.buffer[i].indexOut = 0;
		b.buffer[i].fifo = (LP_QUEUE_EL)malloc(MAX_EL * sizeof(QUEUE_EL));
		b.buffer[i].meIn = CreateSemaphore(NULL, 1, 1, NULL);
		b.buffer[i].meOut = CreateSemaphore(NULL, 1, 1, NULL);
		b.buffer[i].nEl = 0;

		ReadFile(hIn, &b.buffer[i].min, sizeof(FLOAT), &nR, NULL);
		if (nR == 0) {
			_ftprintf(stderr, _T("Error reading input file.\n"));
			return -1;
		}

		ReadFile(hIn, &b.buffer[i].max, sizeof(FLOAT), &nR, NULL);
		if (nR == 0) {
			_ftprintf(stderr, _T("Error reading input file.\n"));
			return -1;
		}

	}

	//create threads
	ids = (LPINT)malloc(nClient * sizeof(INT));
	hC = (LPHANDLE)malloc(nClient * sizeof(HANDLE));
	for (i = 0; i < nClient; i++) {
		ids[i] = i;
		hC[i] = CreateThread(NULL, 0, client, &ids[i], 0, NULL);
	}

	hS = (LPHANDLE)malloc(nServer * sizeof(HANDLE));
	for (i = 0; i < nServer; i++) {
		hS[i] = CreateThread(NULL, 0, server, 0, 0, NULL);
	}

	//wait threads
	WaitForMultipleObjects(nClient, hC, TRUE, INFINITE);
	WaitForMultipleObjects(nServer, hS, TRUE, INFINITE);

	//close and free all
	CloseHandle(hIn);
	DeleteCriticalSection(&endStruct.cs);
	DeleteCriticalSection(&emptyStruct.cs);
	free(hS);
	free(hC);

	for (i = 0; i < b.nQueue; i++) {
		free(b.buffer[i].fifo);
	}
	free(b.buffer);
	return 0;
}

DWORD WINAPI client(LPVOID lpParameters) {

	INT i;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };
	LARGE_INTEGER filePos;
	QUEUE_EL localEl;
	LPINT id = (LPINT)lpParameters;
	DWORD nR;
	BOOL end;

	i = 0;
	while (TRUE) {
		filePos.QuadPart = i++ * sizeof(QUEUE_EL);
		ov.Offset = filePos.LowPart;
		ov.OffsetHigh = filePos.HighPart;

		ReadFile(hIn, &localEl, sizeof(QUEUE_EL), &nR, &ov); //lock?
		if (nR == 0) {
			break;
		}

		if (localEl.idC == *id) {
			push_el(localEl);

		}
	}

	EnterCriticalSection(&endStruct.cs);
	if (!endStruct.val)
		endStruct.val = TRUE;
	LeaveCriticalSection(&endStruct.cs);

	return 0;
}

DWORD WINAPI server(LPVOID lpParameters) {

	QUEUE_EL localEl;
	BOOL end;

	while (TRUE) {
		EnterCriticalSection(&endStruct.cs);
		end = endStruct.val;
		LeaveCriticalSection(&endStruct.cs);
		if (end)
			break;
		localEl = pull_el();
		Sleep(localEl.duration);
	}
	return 0;

}

BOOL push_el(QUEUE_EL el) {

	INT i;
	BOOL done = FALSE;

	WaitForSingleObject(emptyStruct.sem, INFINITE);
	
	for (i = 0; i < b.nQueue; i++) {
		if (b.buffer[i].min < el.duration && b.buffer[i].max > el.duration) {

			WaitForSingleObject(b.buffer[i].meIn, INFINITE);
			if (b.buffer[i].nEl == MAX_EL) {
				ReleaseSemaphore(b.buffer[i].meIn, 1, NULL);
			}
			else {
				b.buffer[i].fifo[b.buffer[i].indexIn] = el;
				b.buffer[i].indexIn = (b.buffer[i].indexIn + 1) % MAX_EL;
				b.buffer[i].nEl++;

				ReleaseSemaphore(b.buffer[i].meIn, 1, NULL);
				done = TRUE;
			}
		}
	}

	ReleaseSemaphore(fullStruct.sem, 1, NULL);

	if (!done) {
		//wait sulla prima
	}

	return TRUE;
}


QUEUE_EL pull_el(VOID) {

	QUEUE_EL el;
	INT i;

	while (TRUE) {

		WaitForSingleObject(fullStruct.sem, INFINITE);
		for (i = 0; i < b.nQueue; i++) {

			if (b.buffer[i].nEl > 0) {
				WaitForSingleObject(b.buffer[i].meOut, INFINITE);
				el = b.buffer[i].fifo[b.buffer[i].indexOut];
				b.buffer[i].indexIn = (b.buffer[i].indexIn + 1) % MAX_EL;
				b.buffer[i].nEl--;
				ReleaseSemaphore(b.buffer[i].meOut, 1, NULL);
			}

		}


		ReleaseSemaphore(emptyStruct.sem, 1, NULL);


	}

	return el;
}