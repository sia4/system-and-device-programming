#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <io.h>
#include <malloc.h>

#pragma warning(disable: 4996)

#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

#define END_ROW _T('\n')
#define SPACE _T(' ')
#define MAX_LEN 256
#define MAX_LEN_ROW 256
#define BUFF_SIZE 16

typedef struct {
	TCHAR path[MAX_LEN];
	TCHAR newPath[MAX_LEN];
} NEW_FILE;

typedef struct {
	NEW_FILE buffer[BUFF_SIZE];
	DWORD size;
	DWORD nE;
	DWORD in;
	DWORD out;
	HANDLE full;
	HANDLE empty;
	HANDLE meP;
	HANDLE meC;
} QUEUE;

QUEUE queue, queueFinalFile;
HANDLE hOutput;
INT nRowOutput;
BOOL end = FALSE;
INT nFinishedThreads = 0;
INT nThreads;

static DWORD FileType(LPWIN32_FIND_DATA pFileData);
static INT TraverseDirectoryRecursive(LPTSTR PathName, LPTSTR destPath);
DWORD WINAPI ThreadFunction(LPVOID);
DWORD WINAPI FileManagement(LPVOID);
VOID OrderFile(LPTSTR filePath, INT nRow, BOOL finalFile);
VOID pushFile(NEW_FILE file, QUEUE q);

INT _tmain(INT argc, LPTSTR argv[]) {

	LPHANDLE hT;
	HANDLE hTM;

	INT i;

	if (argc != 4) {
		_ftprintf(stderr, _T("Missing arguments. Usage: program N source destination"));
		return 1;
	}
	
	/*
		Queue
	*/
	queue.nE = 0;
	queue.in = 0;
	queue.out = 0;
	queue.size = BUFF_SIZE;

	/*
		Semaphores
	*/
	queue.full = CreateSemaphore(NULL, 0, BUFF_SIZE, NULL);
	queue.empty = CreateSemaphore(NULL, BUFF_SIZE, BUFF_SIZE, NULL);
	queue.meC = CreateSemaphore(NULL, 1, 1, NULL);
	queue.meP = CreateSemaphore(NULL, 1, 1, NULL);
	
	queueFinalFile.full = CreateSemaphore(NULL, 0, BUFF_SIZE, NULL);
	queueFinalFile.empty = CreateSemaphore(NULL, BUFF_SIZE, BUFF_SIZE, NULL);
	queueFinalFile.meC = CreateSemaphore(NULL, 1, 1, NULL);
	queueFinalFile.meP = CreateSemaphore(NULL, 1, 1, NULL);

	/*
		Output File
	*/
	hOutput = CreateFile(_T("output.txt"), GENERIC_WRITE,
		0, NULL, CREATE_ALWAYS, 0, NULL);
	nRowOutput = 0;

	/*
		Threads
	*/
	nThreads = _ttoi(argv[1]);
	hT = (LPHANDLE)malloc(nThreads * sizeof(HANDLE));
	for (i = 0; i < nThreads; i++) {
		hT[i] = CreateThread(NULL, 0, ThreadFunction, NULL, 0, NULL);
	}

	hTM = CreateThread(NULL, 0, FileManagement, NULL, 0, NULL);

	TraverseDirectoryRecursive(argv[2], argv[3]);
	end = TRUE;
	ReleaseSemaphore(queue.full, 1, NULL);

	/*
		End and close all
	*/
	WaitForMultipleObjects(nThreads, hT, TRUE, INFINITE);
	WaitForSingleObject(hTM, INFINITE);
	free(hT);
	CloseHandle(hOutput);

	OrderFile(_T("output.txt"), nRowOutput, TRUE);
	return 0;
}

DWORD WINAPI ThreadFunction(LPVOID lpParameters) {

	NEW_FILE file;
	HANDLE hOld;
	HANDLE hNew;
	TCHAR c;
	DWORD nR, nW;
	INT nChar;
	INT nRow;
	INT nWord;
	INT nWordRow;
	INT i;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };
	LARGE_INTEGER filePos;
	LONG start;
	INT startRow;


	while (!end) {
		WaitForSingleObject(queue.full, INFINITE);
		if (end) {
			ReleaseSemaphore(queue.full, 1, NULL);
			break;
		}
		WaitForSingleObject(queue.meC, INFINITE);
	
		file = queue.buffer[queue.out];
		queue.nE = queue.nE - 1;
		queue.out = (queue.out + 1) % queue.size;
	
		ReleaseSemaphore(queue.meC, 1, NULL);
		ReleaseSemaphore(queue.empty, 1, NULL);

		hOld = CreateFile(file.path, GENERIC_READ,
			0, NULL, OPEN_EXISTING, 0, NULL);
		hNew = CreateFile(file.newPath, GENERIC_WRITE,
			0, NULL, CREATE_ALWAYS, 0, NULL);

		nChar = 0;
		nRow = 0;
		nWord = 0;
		nWordRow = 0;
		i = 0;
		start = 4 * sizeof(INT) + 2 * sizeof(SPACE) + sizeof(END_ROW);
		startRow = 0;

		filePos.QuadPart = (start);
		SetFilePointerEx(hNew, filePos, 0, FILE_BEGIN);

		while (ReadFile(hOld, &c, sizeof(TCHAR), &nR, NULL) && nR > 0) {
			WriteFile(hNew, &c, sizeof(TCHAR), &nW, NULL);
			nChar++;
			
			if (c == SPACE) {
				nWord++;
				nWordRow++;
			}
			
			if (c == END_ROW) {
				nWord++;
				nWordRow++;
				nRow++;
				filePos.QuadPart = startRow * sizeof(TCHAR) + start;
				ov.Offset = filePos.LowPart;
				ov.OffsetHigh = filePos.HighPart;
				WriteFile(hNew, &nWordRow, sizeof(INT), &nW, &ov);
				SetFilePointer(hNew, sizeof(INT), 0, FILE_CURRENT);
				nWordRow = 0;
				startRow = nChar;
			}
		}

		SetFilePointer(hNew, 0, 0, FILE_BEGIN);
		c = SPACE;
		WriteFile(hNew, &nWordRow, sizeof(INT), &nW, NULL);
		WriteFile(hNew, &c, sizeof(TCHAR), &nW, NULL);
		WriteFile(hNew, &nWordRow, sizeof(INT), &nW, NULL);
		WriteFile(hNew, &c, sizeof(TCHAR), &nW, NULL);
		WriteFile(hNew, &nWordRow, sizeof(INT), &nW, NULL);

		CloseHandle(hOld);
		CloseHandle(hNew);

		OrderFile(file.newPath, nRow, FALSE);

		pushFile(file, queueFinalFile);

	}

	nFinishedThreads++;
	if (nFinishedThreads == nThreads) {
		ReleaseSemaphore(queueFinalFile.full, 1, NULL);
	}
	ExitThread(0);
}

DWORD WINAPI FileManagement(LPVOID lpParameters) {

	NEW_FILE file;
	HANDLE hF;
	LONG start;
	LARGE_INTEGER filePos;
	TCHAR c;
	DWORD nR, nW;

	while (nFinishedThreads < nThreads) {
		WaitForSingleObject(queueFinalFile.full, INFINITE);
		if (nFinishedThreads == nThreads) {
			break;
		}
		WaitForSingleObject(queueFinalFile.meC, INFINITE);

		file = queueFinalFile.buffer[queueFinalFile.out];
		queueFinalFile.nE = queueFinalFile.nE - 1;
		queueFinalFile.out = (queueFinalFile.out + 1) % queueFinalFile.size;

		ReleaseSemaphore(queueFinalFile.meC, 1, NULL);
		ReleaseSemaphore(queueFinalFile.empty, 1, NULL);

		hF = CreateFile(file.newPath, GENERIC_READ,
			0, NULL, OPEN_EXISTING, 0, NULL);

		start = 4 * sizeof(INT) + 2 * sizeof(SPACE) + sizeof(END_ROW);
		filePos.QuadPart = (start);
		SetFilePointerEx(hF, filePos, 0, FILE_BEGIN);

		while (ReadFile(hF, &c, sizeof(TCHAR), &nR, NULL) && nR > 0) {
			WriteFile(hOutput, &c, sizeof(TCHAR), &nW, NULL);
			if(c == END_ROW)
				nRowOutput++;

		}
		CloseHandle(hF);

	}

}


static INT TraverseDirectoryRecursive(LPTSTR PathName, LPTSTR destPath) {
	HANDLE SearchHandle;
	WIN32_FIND_DATA FindData;
	DWORD FType, i;
	TCHAR path[MAX_LEN];
	TCHAR newDestPath[MAX_LEN];
	NEW_FILE file;

	_tcscpy(path, PathName);
	_tcscat(path, _T("/*"));
	
	CreateDirectory(destPath, NULL);

	SearchHandle = FindFirstFile(path, &FindData);

	do {

		FType = FileType(&FindData);

		_stprintf(newDestPath, _T("%s\\%s"), destPath, FindData.cFileName);

		if (FType == TYPE_FILE) {
			
			_stprintf(file.path, _T("%s\\%s"), path, FindData.cFileName);
			_tcscpy(file.newPath, newDestPath);
			pushFile(file, queue);

		}
		if (FType == TYPE_DIR) {
			
			_tcscpy(path, PathName);
			_tcscat(path, _T("/"));
			_tcscat(path, FindData.cFileName);
			_tprintf(_T("path changed: %s\n"), path);
			TraverseDirectoryRecursive(path, newDestPath);

			_tcscpy(path, PathName);
		}

	} while (FindNextFile(SearchHandle, &FindData));

	FindClose(SearchHandle);
	return 0;
}


static DWORD FileType(LPWIN32_FIND_DATA pFileData) {
	BOOL isDir;
	DWORD fType;
	fType = TYPE_FILE;
	isDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (isDir)
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0 || lstrcmp(pFileData->cFileName, _T("..")) == 0)
			fType = TYPE_DOT;
		else fType = TYPE_DIR;
		return fType;
}

VOID pushFile(NEW_FILE file, QUEUE q) {

	WaitForSingleObject(q.empty, INFINITE);
	WaitForSingleObject(q.meP, INFINITE);
	
	q.buffer[q.in] = file;
	q.in = (q.in + 1) % q.size;
	q.nE = q.nE + 1;
	
	ReleaseSemaphore(q.meP, 1, NULL);
	ReleaseSemaphore(q.full, 1, NULL);

	return;
}

VOID OrderFile(LPTSTR filePath, INT nRow, BOOL finalFile) {

	INT i, k;
	INT temp;
	INT nWord1, nWord2;
	LPINT nWord;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };
	LARGE_INTEGER filePos;
	DWORD nW;
	FILE *fP;
	TCHAR row[MAX_LEN_ROW];
	LPTSTR *rows;

	rows = (LPTSTR *)malloc(nRow * sizeof(LPTSTR));
	for (i = 0; i < nRow; i++) {
		rows[i] = (LPTSTR)malloc(MAX_LEN_ROW * sizeof(TCHAR));
	}
	nWord = (LPINT)calloc(nRow, sizeof(INT));
	_tfopen_s(&fP, filePath, _T("wr"));

	if (!finalFile) {
		_fgetts(row, MAX_LEN_ROW, fP);
	}

	for (i = 0; i < nRow; i++) {
		_fgetts(rows[i], MAX_LEN_ROW, fP);
		_stscanf(rows[i], _T("%d"), nWord[i]);
	}

	for (i = 0; i < nRow - 1; i++) {
		for (k = 0; k < nRow - 1 - i; k++) {			
			if (nWord[k] > nWord[k+i]) {
				_tcscpy(row, rows[k]);
				_tcscpy(rows[k], rows[k + 1]);
				_tcscpy(rows[k + 1], row);
			}

		}
	}

	return;
}
