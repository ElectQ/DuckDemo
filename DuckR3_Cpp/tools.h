#pragma once
#include "head.h"
#include <Windows.h>

namespace Tools {
    /**
     * @brief �򻯴����̵߳ĺ�����
     * @param pFunctionAddress �̺߳����ĵ�ַ��
     * @param pParams ���ݸ��̺߳����Ĳ�����
     * @return �����´����̵߳ľ����
     */
    auto EasyCreateThread(void* pFunctionAddress, void* pParams)->HANDLE;
}