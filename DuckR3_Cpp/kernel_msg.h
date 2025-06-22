#pragma once


#include "head.h"

namespace KerneMsg {
	extern std::map<int, std::tuple<void*, bool*>> eventIdMap;
	extern int currentIndex;  // 之前的变量也一起修复
	
	extern std::mutex theLock;

	auto HandleCefPopUpMsg(int eventId, bool isNeedBlock)->void;
	auto OnProcessCreate(std::wstring Path, std::wstring CommandLine)->bool;
	auto KernMsgRoute(void* ctx) -> void;
	auto WaitKernelMsg() -> void;
	auto Init() -> bool;
}

