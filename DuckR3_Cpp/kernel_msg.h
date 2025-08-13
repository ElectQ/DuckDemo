#pragma once


#include "head.h"

namespace KerneMsg {
	extern std::map<int, std::tuple<void*, bool*>> eventIdMap;
	extern int currentIndex;  // ֮ǰ�ı���Ҳһ���޸�
	
	extern std::mutex theLock;

	auto HandleCefPopUpMsg(int eventId, bool isNeedBlock)->void;
	auto OnProcessCreate(std::wstring Path, std::wstring CommandLine)->bool;
	auto KernMsgRoute(void* ctx) -> void;
	auto WaitKernelMsg() -> void;
	auto Init() -> bool;
	
	// 测试方法
	auto TestMultiplePopups(int count = 3) -> void;
}

