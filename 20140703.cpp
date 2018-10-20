#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <io.h>
#include <malloc.h>

#pragma warning(disable: 4996)

typedef struct _person *LP_PERSON;
typedef struct _person{
	TCHAR cf[15 + 1];
	TCHAR surname[30 + 1];
	TCHAR name[30 + 1];
	TCHAR sex;
	TCHAR arrivalTime[5 + 1];
	DWORD timeToRegister;
	DWORD timeToVote;
} PERSON, *LP_PERSON;

typedef struct {
	TCHAR cf[15 + 1];
	TCHAR votingStation[30 + 1];
	TCHAR endTime[5 + 1];
} OUTPUT;

typedef struct {
	LP_PERSON buffer;
	INT size;
	HANDLE empty;
	HANDLE full;
	HANDLE meC;
	HANDLE meP;
	INT in;
	INT out;
	INT nE;
} QUEUE;

typedef struct {
	HANDLE hOut;
	INT row = 0;
	CRITICAL_SECTION cs;
} OUTPUT_FILE;

QUEUE registrationQueue;
QUEUE voteQueue;
TCHAR time[5 + 1];
OUTPUT_FILE out;

DWORD WINAPI RegisterStation(LPVOID);
DWORD WINAPI VoteStation(LPVOID);
VOID PushPerson(PERSON p, QUEUE q);
PERSON PopPerson(QUEUE q);
VOID ReadInputFile(LPTSTR filePath);

INT _tmain(INT argc, LPTSTR argv[]) {

	HANDLE hTM, hTF;
	TCHAR M = _T('M');
	TCHAR F = _T('F');
	INT nVote, i;
	LPHANDLE hTV;
	LPINT id;

	if (argc != 5) {
		_ftprintf(stderr, _T("Missing parameters. Usage: program inputFile n m outputFile"));
		ExitProcess(0);
	}
	
	/*
	QUEUES AND SEMAPHORES
	*/
	registrationQueue.size = _ttoi(argv[2]);
	registrationQueue.buffer = (LP_PERSON)malloc(registrationQueue.size * sizeof(PERSON));
	registrationQueue.full = CreateSemaphore(NULL, 0, registrationQueue.size, NULL);
	registrationQueue.empty = CreateSemaphore(NULL, registrationQueue.size, registrationQueue.size, NULL);
	registrationQueue.meP = CreateSemaphore(NULL, 1, 1, NULL);
	registrationQueue.meC = CreateSemaphore(NULL, 1, 1, NULL);

	voteQueue.size = _ttoi(argv[2]);
	voteQueue.buffer = (LP_PERSON)malloc(voteQueue.size * sizeof(PERSON));
	voteQueue.full = CreateSemaphore(NULL, 0, voteQueue.size, NULL);
	voteQueue.empty = CreateSemaphore(NULL, voteQueue.size, voteQueue.size, NULL);
	voteQueue.meP = CreateSemaphore(NULL, 1, 1, NULL);
	voteQueue.meC = CreateSemaphore(NULL, 1, 1, NULL);

	/*
		OUTPUT FILE
	*/
	out.hOut = CreateFile(argv[4], GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, 0, NULL);
	if (out.hOut == INVALID_HANDLE_VALUE) {
		ExitProcess(0);
	}
	out.row = 0;
	InitializeCriticalSection(&out.cs);

	/*
		REGISTER STATION
	*/
	hTM = CreateThread(NULL, 0, RegisterStation, &M, 0, NULL);
	if (hTM == NULL) {
		ExitProcess(0);
	}
	hTF = CreateThread(NULL, 0, RegisterStation, &F, 0, NULL);
	if (hTF == NULL) {
		ExitProcess(0);
	}

	/*
		VOTE STATION
	*/
	nVote = _ttoi(argv[3]);
	hTV = (LPHANDLE)malloc(nVote * sizeof(HANDLE));
	id = (LPINT)malloc(nVote * sizeof(INT));
	for (i = 0; i < nVote; i++) {
		id[i] = i;
		hTV[i] = CreateThread(NULL, 0, VoteStation, &id, 0, NULL);
		if (hTF == NULL) {
			ExitProcess(0);
		}
	}

	ReadInputFile(argv[1]);

	free(registrationQueue.buffer);
	free(hTV);
	free(id);
	CloseHandle(out.hOut);
	DeleteCriticalSection(&out.cs);

	return 1;
}

VOID ReadInputFile(LPTSTR filePath) {

	HANDLE hF;
	DWORD nR;
	PERSON person;

	hF = CreateFile(filePath, GENERIC_READ,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if (hF == INVALID_HANDLE_VALUE) {
		ExitProcess(0);
	}

	while (ReadFile(hF, &person, sizeof(PERSON), &nR, NULL) && nR > 0) {
		PushPerson(person, registrationQueue);
	}

	CloseHandle(hF);
	return;
}

DWORD WINAPI RegisterStation(LPVOID lpParameters) {

	LPTSTR sex = (LPTSTR) lpParameters;
	PERSON person;
	while (TRUE) {
		person = PopPerson(registrationQueue);

		Sleep(person.timeToRegister);

		PushPerson(person, voteQueue);

	}

	ExitThread(0);

}

DWORD WINAPI VoteStation(LPVOID lpParameters) {

	LPINT id = (LPINT)lpParameters;
	PERSON person;
	OUTPUT output;
	DWORD nW;
	TCHAR idString[5];
	INT row;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };
	LARGE_INTEGER filePos;

	while (TRUE) {
		person = PopPerson(voteQueue);

		Sleep(person.timeToVote);
		_tcscpy(output.cf, person.cf);
		_stprintf(output.votingStation, _T("Voting_Station_%d"), id);
		
		EnterCriticalSection(&out.cs);
		row = out.row++;
		LeaveCriticalSection(&out.cs);

		filePos.QuadPart = row * sizeof(OUTPUT);
		ov.Offset = filePos.LowPart;
		ov.OffsetHigh = filePos.HighPart;

		LockFileEx(out.hOut, LOCKFILE_EXCLUSIVE_LOCK, 0, sizeof(OUTPUT), 0, &ov);
		WriteFile(out.hOut, &output, sizeof(OUTPUT), &nW, &ov);
		UnlockFileEx(out.hOut, 0, sizeof(OUTPUT), 0, &ov);

	}

	ExitThread(0);
}

VOID PushPerson(PERSON p, QUEUE q) {

	WaitForSingleObject(q.empty, INFINITE);
	WaitForSingleObject(q.meP, INFINITE);
	q.buffer[q.in] = p;
	q.in = (q.in + 1) % q.size;
	q.nE = q.nE + 1;
	ReleaseSemaphore(q.meP, 1, NULL);
	ReleaseSemaphore(q.full, 1, NULL);

}

PERSON PopPerson(QUEUE q) {

	PERSON p;
	WaitForSingleObject(q.full, INFINITE);
	WaitForSingleObject(q.meC, INFINITE);
	p = q.buffer[q.out];
	q.nE = q.nE - 1;
	q.out = (q.out + 1) % q.size;
	ReleaseSemaphore(q.meC, 1, NULL);
	ReleaseSemaphore(q.empty, 1, NULL);

	return p;
}