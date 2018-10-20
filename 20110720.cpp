#define UNICODE
#define _UNICODE

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <io.h>
#include <malloc.h>
#include <limits>

#define LEN 128 

typedef struct {
	HANDLE hIn;
	DWORD nRow;
	CRITICAL_SECTION cs;
} INPUT_FILE;

typedef struct {
	DWORD n;
	DOUBLE max;
	DOUBLE min;
	DOUBLE avg;
} TH_STR;

typedef struct _output_file *LP_OUTPUT_FILE;
typedef struct _output_file {
	TCHAR name[LEN];
	HANDLE hF;
	CRITICAL_SECTION cs;
	LP_OUTPUT_FILE next;
} OUTPUT_FILE, *LP_OUTPUT_FILE;

typedef struct {
	TCHAR inputFile[LEN];
	TCHAR outputFile[LEN];
} RECORD;

INPUT_FILE inputFile;
LP_OUTPUT_FILE head = NULL;
CRITICAL_SECTION listCS;

DWORD WINAPI t_func(LPVOID);
LP_OUTPUT_FILE SearchOrAddOutputFile(LPTSTR);
LP_OUTPUT_FILE AddOutputFile(LPTSTR);

INT _tmain(INT argc, LPTSTR argv[]) {
	
	INT nThread, i;
	LPHANDLE hT;
	LP_OUTPUT_FILE supp1, supp2;

	//canc
	HANDLE h1;
	TH_STR r;
	DWORD n;

	if (argc != 3) {
		_ftprintf(stderr, _T("Missing parameters.\
			Usage: program inputFile M"));
		return -1;
	}

	//Set up inputFile struct
	inputFile.hIn = CreateFile(argv[1], GENERIC_READ, 
		FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (inputFile.hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error opening input file.\n"));
		return -1;
	}

	InitializeCriticalSection(&inputFile.cs);
	inputFile.nRow = 0;

	//Output file list
	InitializeCriticalSection(&listCS);

	//Create M thread
	nThread = _wtoi(argv[2]);
	hT = (LPHANDLE)malloc(nThread * sizeof(HANDLE));

	for (i = 0; i < nThread; i++) {
		hT[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) t_func, NULL, 0, NULL);
	}

	WaitForMultipleObjects(nThread, hT, TRUE, INFINITE);

	//Close and free all
	CloseHandle(inputFile.hIn);
	DeleteCriticalSection(&inputFile.cs);
	supp1 = head;
	while (supp1 != NULL) {
		DeleteCriticalSection(&supp1->cs);
		CloseHandle(supp1->hF);
		supp2 = supp1;
		supp1 = supp1->next;
		free(supp2);
	}
	DeleteCriticalSection(&listCS);

	h1 = CreateFile(_T("test2.txt"), GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	while (ReadFile(h1, &r, sizeof(TH_STR), &n, NULL) && n > 0) {
		_tprintf(_T("%d %lf %lf %lf \n"), r.n, r.max, r.min, r.avg);
	}

	return 0;
}


DWORD WINAPI t_func(LPVOID lpParameters) {

	DOUBLE supp;
	TH_STR thStr;
	INT row;
	DWORD nR, nW;
	HANDLE hF;
	RECORD record;
	OVERLAPPED ov = { 0, 0, 0, NULL };
	LARGE_INTEGER filePos;
	DOUBLE sum;
	LP_OUTPUT_FILE lpOF;

	thStr.n = 0;
	thStr.max = DBL_MIN;
	sum = 0;
	thStr.min = DBL_MAX;

	while (TRUE) {
		
		EnterCriticalSection(&inputFile.cs);
		row = inputFile.nRow++;
		LeaveCriticalSection(&inputFile.cs);
		
		filePos.QuadPart = row * sizeof(RECORD);
		ov.Offset = filePos.LowPart;
		ov.OffsetHigh = filePos.HighPart;

		ReadFile(inputFile.hIn, &record, sizeof(RECORD),
			&nR, &ov);

		if (nR == 0) {
			return(0);
		}

		hF = CreateFile(record.inputFile, GENERIC_READ,
			FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		
		while (ReadFile(hF, &supp, sizeof(DOUBLE),
			&nR, NULL) && nR > 0) {

			thStr.n++;
			if (supp > thStr.max) {
				thStr.max = supp;
			}
			if (supp < thStr.min) {
				thStr.min = supp;
			}
			sum += supp;
		}

		thStr.avg = sum / thStr.n;

		lpOF = SearchOrAddOutputFile(record.outputFile);
		
		EnterCriticalSection(&lpOF->cs);
		WriteFile(lpOF->hF, &thStr, sizeof(TH_STR), &nW, NULL);
		LeaveCriticalSection(&lpOF->cs);

	}

}

LP_OUTPUT_FILE SearchOrAddOutputFile(LPTSTR name) {

	LP_OUTPUT_FILE supp;

	EnterCriticalSection(&listCS);

	supp = head;
	while (supp != NULL) {
		if (_tcscmp(supp->name, name) == 0) {
			LeaveCriticalSection(&listCS);
			return supp;
		}
		supp = supp->next;
	}

	supp = AddOutputFile(name);
	LeaveCriticalSection(&listCS);
	return supp;

}

LP_OUTPUT_FILE AddOutputFile(LPTSTR name) {

	LP_OUTPUT_FILE supp;

	supp = (LP_OUTPUT_FILE)malloc(sizeof(OUTPUT_FILE));
	_tcscpy(supp->name, name);
	InitializeCriticalSection(&supp->cs);

	supp->hF = CreateFile(name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, 0, NULL);
	if (supp->hF == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Error opening file %s"), name);
		exit(1);
	}

	supp->next = head;
	head = supp;
	
	return supp;
}
