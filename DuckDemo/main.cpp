#include "head.h"




void DriverUnload(PDRIVER_OBJECT driver) {

	DebugPrint("Good BYE!\n");
	Callback::Uninstall();
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver,
	PUNICODE_STRING reg_path) {
	driver->DriverUnload = DriverUnload;
	if (Callback::Init(driver) == FALSE) {
		return STATUS_UNSUCCESSFUL;
	}
	if (Minifilter::Init(driver) == FALSE) {
		return STATUS_UNSUCCESSFUL;
	}
	if (KernelMsg::Init() == FALSE) {
		return STATUS_UNSUCCESSFUL;
	}
	DebugPrint("\nhello world! \n");
	return STATUS_SUCCESS;
}




