﻿#pragma comment(lib, "shlwapi")
#pragma comment(lib, "winmm")
#pragma comment(lib, "comctl32")

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>
#include "zip.h"
#include "unzip.h"
#include "resource.h"

#define WM_EXITTHREAD WM_APP

TCHAR szClassName[] = TEXT("RUNPYTHON");
WNDPROC lpfnOldClassProc;

BOOL DeleteDirectory(LPCTSTR lpPathName)
{
	if (0 == lpPathName) return FALSE;
	TCHAR szDirectoryPathName[MAX_PATH];
	wcsncpy_s(szDirectoryPathName, MAX_PATH, lpPathName, _TRUNCATE);
	if (TEXT('\\') == szDirectoryPathName[wcslen(szDirectoryPathName) - 1])
	{
		szDirectoryPathName[wcslen(szDirectoryPathName) - 1] = TEXT('\0');
	}
	szDirectoryPathName[wcslen(szDirectoryPathName) + 1] = TEXT('\0');
	SHFILEOPSTRUCT fos = { 0 };
	fos.wFunc = FO_DELETE;
	fos.pFrom = szDirectoryPathName;
	fos.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
	return !SHFileOperation(&fos);
}

BOOL CreateTempDirectory(LPTSTR pszDir)
{
	DWORD dwSize = GetTempPath(0, 0);
	if (dwSize == 0 || dwSize > MAX_PATH - 14) return FALSE;
	LPTSTR pTmpPath = (LPTSTR)GlobalAlloc(GPTR, sizeof(TCHAR)*(dwSize + 1));
	GetTempPath(dwSize + 1, pTmpPath);
	dwSize = GetTempFileName(pTmpPath, TEXT(""), 0, pszDir);
	GlobalFree(pTmpPath);
	if (dwSize == 0) return FALSE;
	DeleteFile(pszDir);
	return (CreateDirectory(pszDir, 0) != 0);
}

void AddEditString(HWND hEdit, LPSTR lpszString)
{
	const DWORD_PTR len = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);
	SendMessage(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	SendMessageA(hEdit, EM_REPLACESEL, 0, (LPARAM)lpszString);
}

void AddEditString(HWND hEdit, LPWSTR lpszString)
{
	const DWORD_PTR len = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);
	SendMessage(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
	SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)lpszString);
}

void Run(LPCWSTR lpszCode, DWORD dwSize, LPCTSTR lpszTempPath, HWND hOutputEdit)
{
	SetWindowText(hOutputEdit, 0);
	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	TCHAR szBinFolderPath[MAX_PATH];
	lstrcpy(szBinFolderPath, lpszTempPath);
	SetCurrentDirectory(szBinFolderPath);
	TCHAR szCodeFilePath[MAX_PATH];
	lstrcpy(szCodeFilePath, lpszTempPath);
	PathAppend(szCodeFilePath, TEXT("code.py"));
	const HANDLE hFileCode = CreateFile(szCodeFilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFileCode != INVALID_HANDLE_VALUE)
	{
		const DWORD dwLength = WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, 0, 0, 0, 0);
		LPSTR lpszString = (LPSTR)GlobalAlloc(0, dwLength);
		WideCharToMultiByte(CP_UTF8, 0, lpszCode, -1, lpszString, dwLength, 0, 0);
		DWORD dwWritten;
		WriteFile(hFileCode, lpszString, dwLength, &dwWritten, 0);
		GlobalFree(lpszString);
		CloseHandle(hFileCode);
	}
	//リダイレクト先のファイルを開く
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE; //TRUEでハンドルを引き継ぐ
	TCHAR szOutputFilePath[MAX_PATH];
	lstrcpy(szOutputFilePath, lpszTempPath);
	PathAppend(szOutputFilePath, TEXT("stdout.txt"));
	const HANDLE hFileOutput = CreateFile(szOutputFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFileOutput != INVALID_HANDLE_VALUE)
	{
		//標準入出力の指定
		STARTUPINFO si = { 0 };
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = stdin;
		si.hStdOutput = hFileOutput;
		si.hStdError = hFileOutput;
		//コンパイラを起動する
		TCHAR szRunCommand[1024];
		wsprintf(szRunCommand, TEXT("pythonw.exe \"%s\""), szCodeFilePath);
		PROCESS_INFORMATION pi = { 0 };
		const DWORD dwRunStartTime = timeGetTime();
		if (!CreateProcess(0, szRunCommand, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi))
		{
			AddEditString(hOutputEdit, TEXT("実行失敗\r\n\r\n"));
		}
		//起動したプロセスの終了を待つ
		WaitForSingleObject(pi.hProcess, INFINITE);
		const DWORD dwRunTime = timeGetTime() - dwRunStartTime;
		DWORD dwExidCode = 0;
		GetExitCodeProcess(pi.hProcess, &dwExidCode);
		//ハンドルを閉じる
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		SetFilePointer(hFileOutput, 0, 0, FILE_BEGIN);
		const int nFileSize = GetFileSize(hFileOutput, 0);
		LPSTR lpszOutput = (LPSTR)GlobalAlloc(0, nFileSize + 1);
		DWORD dwRead;
		ReadFile(hFileOutput, lpszOutput, nFileSize, &dwRead, 0);
		lpszOutput[dwRead] = 0;
		AddEditString(hOutputEdit, lpszOutput);
		AddEditString(hOutputEdit, TEXT("\r\n"));
		TCHAR szTime[1024];
		wsprintf(szTime, TEXT("実行時間: %d msec\r\n"), dwRunTime);
		AddEditString(hOutputEdit, szTime);
		GlobalFree(lpszOutput);
		CloseHandle(hFileOutput);
	}
	DeleteFile(szCodeFilePath);
	DeleteFile(szOutputFilePath);
	SetCurrentDirectory(szCurrentDirectory);
}

LRESULT CALLBACK MultiLineEditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
	return CallWindowProc(lpfnOldClassProc, hWnd, msg, wParam, lParam);
}

struct DATA
{
	HWND hWnd;
	TCHAR szTempPath[MAX_PATH];
	HWND hProgress;
	BOOL bAbort;
};

DWORD WINAPI ThreadExpandModule(LPVOID p)
{
	DATA* pData = (DATA*)p;
	const HMODULE hInst = GetModuleHandle(0);
	const HRSRC hResource = FindResource(hInst, MAKEINTRESOURCE(IDR_ZIP1), TEXT("ZIP"));
	if (!hResource) return FALSE;
	const DWORD imageSize = SizeofResource(hInst, hResource);
	if (!imageSize) return FALSE;
	const void* pResourceData = LockResource(LoadResource(hInst, hResource));
	if (!pResourceData) return FALSE;
	TCHAR szCurrentDirectory[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	SetCurrentDirectory(pData->szTempPath);
	const HZIP hz = OpenZip((LPBYTE)pResourceData, imageSize, 0);
	SetUnzipBaseDir(hz, pData->szTempPath);
	ZIPENTRY ze;
	GetZipItem(hz, -1, &ze);
	const int numitems = ze.index;
	for (int zi = 0; zi < numitems && !pData->bAbort; ++zi)
	{
		SendMessage(pData->hProgress, PBM_SETPOS, (WPARAM)(100.0 * (double)zi / (double)numitems), 0);
		GetZipItem(hz, zi, &ze);
		if (ze.attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			CreateDirectory(ze.name, 0);
		}
		else if (ze.attr & FILE_ATTRIBUTE_ARCHIVE)
		{
			UnzipItem(hz, zi, ze.name);
			char *ibuf = new char[ze.unc_size];
			UnzipItem(hz, zi, ibuf, ze.unc_size);
			const HANDLE hFile = CreateFile(ze.name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
			DWORD d;
			WriteFile(hFile, ibuf, ze.unc_size, &d, 0);
			CloseHandle(hFile);
			delete[] ibuf;
		}
	}
	CloseZip(hz);
	SetCurrentDirectory(szCurrentDirectory);
	PostMessage(pData->hWnd, WM_EXITTHREAD, 0, (LPARAM)p);
	ExitThread(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hInputEdit, hOutputEdit;
	static TCHAR szTempDirectoryPath[MAX_PATH];
	static HFONT hFont;
	static DATA* pData;
	static HANDLE hThread;
	static DWORD dwParam;
	switch (msg)
	{
	case WM_CREATE:
		InitCommonControls();
		if (!CreateTempDirectory(szTempDirectoryPath)) return -1;
		pData = (DATA*)GlobalAlloc(0, sizeof DATA);
		pData->hWnd = hWnd;
		pData->bAbort = 0;
		pData->hProgress = CreateWindowEx(0, PROGRESS_CLASS, 0, WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		lstrcpy(pData->szTempPath, szTempDirectoryPath);
		hThread = CreateThread(0, 0, ThreadExpandModule, (LPVOID)pData, 0, &dwParam);
		hFont = CreateFont(26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Consolas"));
		hInputEdit = CreateWindow(TEXT("EDIT"), TEXT("print(\"hello\")"), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hInputEdit, EM_LIMITTEXT, 0, 0);
		{
			const int tab = 16;
			SendMessage(hInputEdit, EM_SETTABSTOPS, 1, (LPARAM)&tab);
		}
		lpfnOldClassProc = (WNDPROC)SetWindowLongPtr(hInputEdit, GWLP_WNDPROC, (LONG_PTR)MultiLineEditWndProc);
		hOutputEdit = CreateWindow(TEXT("EDIT"), TEXT("Python の実行環境を構築しています..."), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hOutputEdit, EM_LIMITTEXT, 0, 0);
		SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hOutputEdit, WM_SETFONT, (WPARAM)hFont, 0);
		SendMessage(hInputEdit, EM_SETSEL, 0, -1);
		break;
	case WM_EXITTHREAD:
		{
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
			hThread = 0;
			BOOL bExit = pData->bAbort;
			ShowWindow(pData->hProgress, SW_HIDE);
			SetWindowText(hOutputEdit, TEXT("Python の実行環境が整いました。"));
			if (bExit) PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		break;
	case WM_SETFOCUS:
		SetFocus(hInputEdit);
		break;
	case WM_SIZE:
		MoveWindow(pData->hProgress, 0, 0, LOWORD(lParam), 8, 1);
		MoveWindow(hInputEdit, 0, 8, LOWORD(lParam) / 2, HIWORD(lParam) - 8, 1);
		MoveWindow(hOutputEdit, LOWORD(lParam) / 2, 8, LOWORD(lParam) / 2, HIWORD(lParam) - 8, 1);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_BUILD:
			if (hThread)
			{
				SetWindowText(hOutputEdit, TEXT("ビルドの準備がまだできていません。"));
			}
			else
			{
				const int nSize = GetWindowTextLength(hInputEdit);
				LPWSTR lpszCode = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR)*(nSize + 1));
				GetWindowTextW(hInputEdit, lpszCode, nSize + 1);
				Run(lpszCode, nSize, szTempDirectoryPath, hOutputEdit);
				GlobalFree(lpszCode);
			}
			break;
		case ID_ALLSELECT:
			SendMessage(GetFocus(), EM_SETSEL, 0, -1);
			break;
		}
		break;
	case WM_CLOSE:
		if (hThread)
		{
			pData->bAbort = TRUE;
		}
		else
		{
			DestroyWindow(hWnd);
		}
		break;
	case WM_DESTROY:
		GlobalFree(pData);
		pData = 0;
		SetWindowLongPtr(hInputEdit, GWLP_WNDPROC, (LONG_PTR)lpfnOldClassProc);
		DeleteDirectory(szTempDirectoryPath);
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	const WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		DLGWINDOWEXTRA,
		hInstance,
		LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),
		LoadCursor(0,IDC_ARROW),
		0,
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	const HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Python (F5キーで実行)"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	ACCEL Accel[] = {
		{ FVIRTKEY, VK_F5, ID_BUILD },
		{ FVIRTKEY | FCONTROL, 'A', ID_ALLSELECT },
	};
	const HACCEL hAccel = CreateAcceleratorTable(Accel, sizeof(Accel) / sizeof(ACCEL));
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (!TranslateAccelerator(hWnd, hAccel, &msg) && !IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	DestroyAcceleratorTable(hAccel);
	return (int)msg.wParam;
}
