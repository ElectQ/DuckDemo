#include "helper.h"

namespace Helper {

	auto checkEspListHasKernelGuid(PFLT_FILTER pFilter, PECP_LIST pEcpList,
		LPCGUID pGuid) -> bool {
		PVOID pEcpContext = nullptr;
		if (NT_SUCCESS(FltFindExtraCreateParameter(
			pFilter, pEcpList, pGuid, &pEcpContext, nullptr)) == false ||
			FltIsEcpFromUserMode(pFilter, pEcpContext)) {
			return false;
		}
		return true;
	}
}