#pragma once
#include "head.h"
enum class _ClientMsgType { kCreateProcess, kReadFile, kWriteFile };

struct _MsgClientPacket {
	uint32_t magicNum; 
	_ClientMsgType type;
};
struct _MsgCreateProcess {
	_MsgClientPacket packetHeader;
	uint32_t pid;
	uint32_t ppid;
	size_t pathSize;
	size_t commandSize;
	size_t commandOffset;


	wchar_t path[1];
	wchar_t commandLine[1];
};
struct _Msg_CreateProcess_R3Reply {
	bool isNeedBlock;
};
struct _MsgFileRw {
	_MsgClientPacket packetHeader;
	uint32_t pid;
	wchar_t path[1];
};
struct _Ring3MsgQueue {
	LIST_ENTRY ListEntry;
	void* buffer;
	size_t bufferSize;
};

namespace KernelMsg {
	extern bool isDriverUninstall;
	auto Init() -> bool;
	auto Uninstall() -> void;
	auto SendCreateProcessEvent(_MsgCreateProcess* msg, size_t msgSize) -> bool;
	auto PushMsgToQueue(void* msgBuffer, size_t msgSize) -> void;
};