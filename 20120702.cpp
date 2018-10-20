#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif // !UNICODE

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif // !_CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <io.h>
#include <tchar.h>

#define LEN 128*sizeof(TCHAR)
#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

typedef struct {
	HANDLE hOut;
	CRITICAL_SECTION cs;
} OUTPUT_FILE;

typedef struct _handle_str *LP_HANDLE_STR;

typedef struct _handle_str {
	HANDLE hT;
	LP_HANDLE_STR next;
} HANDLE_STR, *LP_HANDLE_STR;

TCHAR virusPattern[LEN];
OUTPUT_FILE outputFile;

DWORD WINAPI t_func(LPVOID lpParameters);
static DWORD FileType(LPWIN32_FIND_DATA pFileData);
static DWORD FileType(LPWIN32_FIND_DATA pFileData);
BOOL SearchVirus(LPTSTR path, LPTSTR name);
BOOL WriteOnOutputFile(LPTSTR fileName);

INT _tmain(INT argc, LPTSTR argv[]) {

	HANDLE hIn;
	DWORD nR;
	HANDLE hT;
	TCHAR file[LEN];

	if (argc != 4) {
		_ftprintf(stderr, _T("Missing parameters. Usage: program inputFileName inputDirName outputDirName"));
		return -1;
	}

	//READING VIRUS PATTERN FROM INPUT FILE	
	hIn = CreateFile(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error opening input file.\n"));
		return -1;
	}
	ReadFile(hIn, virusPattern, LEN, &nR, NULL);
	CloseHandle(hIn);

	//SET OUTPUT FILE
	outputFile.hOut = CreateFile(argv[3], GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (outputFile.hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error opening output file.\n"));
		return -1;
	}
	InitializeCriticalSection(&outputFile.cs);

	SetCurrentDirectory(argv[2]);

	hT = CreateThread(NULL, 0, t_func, _T("."), 0, NULL);

	//CLOSE AND DELETE ALL 
	WaitForSingleObject(hT, INFINITE);
	DeleteCriticalSection(&outputFile.cs);

	//PRINT OUTPUT FILE
	SetFilePointer(outputFile.hOut, 0, 0, FILE_BEGIN);

	while (ReadFile(outputFile.hOut, file, LEN, &nR, NULL) && nR > 0) {
		_tprintf(_T("%s\n"), file);
	}

	CloseHandle(outputFile.hOut);

	return 0;
}

DWORD WINAPI t_func(LPVOID lpParameters) {

	LPTSTR path = (LPTSTR)lpParameters;
	TCHAR localPath[LEN];
	TCHAR pattern[LEN];

	HANDLE searchHandle;
	WIN32_FIND_DATA findData;
	DWORD fType;

	LP_HANDLE_STR hHead, hST;

	//INIT LIST
	hHead = NULL;

	//GENERIC FILE NAME IN "PATH" DIRECTORY
	_tcscpy(localPath, path);
	_tcscpy(pattern, path);
	_tcscat(pattern, _T("\\*"));

	searchHandle = FindFirstFile(pattern, &findData);

	do {
		fType = FileType(&findData);

		if (fType == TYPE_FILE) {
			//SEARCH FOR VIRUS AND WRITE FILE NAMES
			if (SearchVirus(path, findData.cFileName))
				WriteOnOutputFile(findData.cFileName);

		}
		if (fType == TYPE_DIR) {
			_tcscat(localPath, _T("/"));
			_tcscat(localPath, findData.cFileName);

			hST = (LP_HANDLE_STR)malloc(sizeof(HANDLE_STR));
			if (hST == NULL) {
				_ftprintf(stderr, _T("Allocation error.\n"));
				exit(-1);
			}

			hST->hT = CreateThread(NULL, 0, t_func, localPath, 0, NULL);
			if (hST->hT == NULL) {
				_ftprintf(stderr, _T("Create Thread Error (%d).\n"), GetLastError());
				ExitThread(-1);
			}

			hST->next = hHead;
			hHead = hST;

		}
	} while (FindNextFile(searchHandle, &findData));

	hST = hHead;
	while (hST != NULL) {
		WaitForSingleObject(hST->hT, INFINITE);
		hHead = hST->next;
		free(hST);
		hST = hHead;
	}

	FindClose(searchHandle);

	ExitThread(0);
}

static DWORD FileType(LPWIN32_FIND_DATA pFileData) {
	BOOL isDir;
	DWORD fType;
	fType = TYPE_FILE;
	isDir = (pFileData->dwFileAttributes &
		FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (isDir)
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0
			|| lstrcmp(pFileData->cFileName, _T("..")) == 0)
			fType = TYPE_DOT;
		else fType = TYPE_DIR;
		return fType;
}

BOOL SearchVirus(LPTSTR path, LPTSTR name) {

	HANDLE hF;
	TCHAR c;
	TCHAR fileName[LEN];
	DWORD nR;
	INT i = 0;

	wcscpy(fileName, path);
	wcscat(fileName, _T("/"));
	wcscat(fileName, name);

	hF = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hF == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Invalid handle while searching for virus\n"));
		return FALSE;
	}

	while (ReadFile(hF, &c, sizeof(TCHAR), &nR, NULL) && nR > 0) {
		if (c == virusPattern[i]) {
			i++;
			if (i == wcslen(virusPattern)) {
				CloseHandle(hF);
				return TRUE;
			}
		}
		else {
			i = 0;
		}
	}

	CloseHandle(hF);
	return FALSE;
}

BOOL WriteOnOutputFile(LPTSTR fileName) {

	DWORD nW;
	EnterCriticalSection(&outputFile.cs);

	WriteFile(outputFile.hOut, fileName, LEN, &nW, NULL);
	if (nW == 0) {
		_ftprintf(stderr, _T("Error writing onoutput file.\n"));
		return FALSE;
	}

	LeaveCriticalSection(&outputFile.cs);
	return TRUE;
}