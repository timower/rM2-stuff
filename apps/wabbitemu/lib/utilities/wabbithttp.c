#include "stdafx.h"

#include "wabbithttp.h"

size_t ReadFileHttp(TCHAR *url, LPVOID buffer, size_t buffer_size, INTERNET_STATUS_CALLBACK callback) {
	if (!buffer) {
		return NULL;
	}
	ZeroMemory(buffer, buffer_size);
	HINTERNET hInternet = InternetOpen(_T("Wabbitemu"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) {
		return NULL;
	}
	HINTERNET hOpenUrl = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, NULL);
	if (hOpenUrl == NULL) {
		return NULL;
	}
	DWORD bytesRead;
	BOOL succeeded = InternetReadFile(hOpenUrl, buffer, buffer_size, &bytesRead);
	return bytesRead;
}
