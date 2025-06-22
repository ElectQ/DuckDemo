#include "tools.h"


namespace Tools {
	auto EasyCreateThread(void* pFunctionAddress, void* pParams) -> HANDLE {
		return CreateThread(
			NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(pFunctionAddress),
			static_cast<LPVOID>(pParams), NULL, NULL);
	}
};