#pragma once
#include "head.h"


namespace Callback {
	auto Uninstall()->void ;
	auto Init(PDRIVER_OBJECT Driver)->bool;
}