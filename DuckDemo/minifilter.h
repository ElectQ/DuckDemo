#pragma once
#include "head.h"
#define MINIFILTER_TAG 'TIFM'

namespace Minifilter {
	extern PFLT_PORT ClientHandle;   //这些都需要被外部链接，所以
	extern KSPIN_LOCK ClientSpinLock;   
	extern PFLT_FILTER filterHandle;  //Minifiler句柄

	enum class _FileAccessType {
		kNomal,
		kRawAccess,
		kPipe,
	};


	struct _Minifilter_Stream_Context {
		_FileAccessType accessType;

		PUNICODE_STRING rawPath;
		PUNICODE_STRING fixPath;
		PUNICODE_STRING dosVolumeName;
		PUNICODE_STRING volumeName;
		HANDLE pid;
		HANDLE requestorPid;
		bool isPreStream;
		bool hasReadFile;
		bool hasWriteFile;
	};

	auto PreCreateOperation(PFLT_CALLBACK_DATA Data,PCFLT_RELATED_OBJECTS FltObjects,PVOID* CompletionContext)->FLT_PREOP_CALLBACK_STATUS;
	auto MinifilterCtxCleanUp(PFLT_CONTEXT Context, FLT_CONTEXT_TYPE ContextType)-> void;
	auto MiniFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags)->NTSTATUS;
	auto Init(PDRIVER_OBJECT Driver) -> bool;
	auto InitializeMiniFilterRegedit(PUNICODE_STRING RegistryPath, PCWSTR Altitude)->NTSTATUS;
	auto PostCreateOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS;
	auto PostReadOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS;
	auto PostWriteOperation(PFLT_CALLBACK_DATA Data,PCFLT_RELATED_OBJECTS FltObjects,PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS;
	auto PostCleanUpOperation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)->FLT_POSTOP_CALLBACK_STATUS;
	const static UNICODE_STRING accessNamedPipe =RTL_CONSTANT_STRING(L"\\Device\\NamedPipe");





	static const auto Ring3CommPort = L"\\DuckGuardCommPort";  //通讯表示

	static const FLT_OPERATION_REGISTRATION Callbacks[] = {
		{IRP_MJ_CREATE, 0, (PFLT_PRE_OPERATION_CALLBACK)PreCreateOperation, (PFLT_POST_OPERATION_CALLBACK)PostCreateOperation},
		{IRP_MJ_READ, 0, NULL, (PFLT_POST_OPERATION_CALLBACK)PostReadOperation},
		{IRP_MJ_WRITE, 0, NULL, (PFLT_POST_OPERATION_CALLBACK)PostWriteOperation},
		{IRP_MJ_CLEANUP, 0, NULL,(PFLT_POST_OPERATION_CALLBACK)PostCleanUpOperation},
		{IRP_MJ_OPERATION_END} 
	};
	const FLT_CONTEXT_REGISTRATION contextRegistration[] = {
		{FLT_STREAMHANDLE_CONTEXT, 0, MinifilterCtxCleanUp,
		 sizeof(_Minifilter_Stream_Context), MINIFILTER_TAG},
		{FLT_CONTEXT_END},
	};

	const FLT_REGISTRATION filterRegistration = {
		sizeof(FLT_REGISTRATION),
		FLT_REGISTRATION_VERSION_0203,
		FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS,  //拓展监听对象
		contextRegistration, //上下文
		Callbacks,  //回调函数
		MiniFilterUnload,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};
	static const auto RegistryPath =
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DuckDemo";
	static const auto Altitude = L"370020";
};