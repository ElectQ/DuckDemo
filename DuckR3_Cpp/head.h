#pragma once
#define NOMINMAX // 防止Windows宏定义min和max

#include <iostream>
#include <Windows.h>
#include <fltuser.h>
#include <vector>
#include <share.h>
#include <mutex>

#pragma comment(lib, "fltLib.lib")
#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")

// 定义逻辑最小尺寸
#define LOGICAL_MIN_WIDTH 1000
#define LOGICAL_MIN_HEIGHT 720
// 默认 DPI (100% 缩放)
#define USER_DEFAULT_SCREEN_DPI 96
typedef long long int64;
typedef unsigned long long uint64;

#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_life_span_handler.h"
#include "include/wrapper/cef_helpers.h"     // For CEF_REQUIRE_UI_THREAD
#include "include/wrapper/cef_message_router.h"
#include "include/cef_v8.h"  // 用于V8交互，MessageRouter会用到
#include "main_app.h"


extern "C" {
	HRESULT
		WINAPI
		FilterConnectCommunicationPort(
			_In_ LPCWSTR lpPortName,
			_In_ DWORD dwOptions,
			_In_reads_bytes_opt_(wSizeOfContext) LPCVOID lpContext,
			_In_ WORD wSizeOfContext,
			_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
			_Outptr_ HANDLE* hPort
		);
	HRESULT
		WINAPI
		FilterSendMessage(
			_In_ HANDLE hPort,
			_In_reads_bytes_(dwInBufferSize) LPVOID lpInBuffer,
			_In_ DWORD dwInBufferSize,
			_Out_writes_bytes_to_opt_(dwOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
			_In_ DWORD dwOutBufferSize,
			_Out_ LPDWORD lpBytesReturned
		);;
};

#include "tools.h"
#include "kernel_msg.h"