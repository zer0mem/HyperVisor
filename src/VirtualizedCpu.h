#pragma once

#include "../Common/base/HVCommon.h"

#include <memory>

#include "VMX.h"
#include "HyperVisor.h"

class CVirtualizedCpu
{
public:
	CVirtualizedCpu(
		__in BYTE cpuCore, 
		__in_opt const VMTrap traps[MAX_HV_CALLBACK], 
		__in_opt ULONG_PTR exceptionMask = 0,
		__in_opt const VMCallback callback = NULL, 
		__in_opt const VOID* param = NULL
		);

	~CVirtualizedCpu();

	__checkReturn 
	bool 
	VirtualizationON();

	__checkReturn
	bool
	VirtualizationOFF();

	__checkReturn
	static 
	BYTE 
	GetCoreId(
		__in const ULONG_PTR* stack
		);

	__checkReturn 
	static 
	__forceinline 
	ULONG_PTR* 
	GetTopOfStack(
		__in const ULONG_PTR* stack
		);

protected:
	static 
	PHYSICAL_ADDRESS 
	GetTopAdress()
	{
		PHYSICAL_ADDRESS addr;
		addr.HighPart = -1;
		return addr;
	}

	BYTE m_cpuCore;
	std::unique_ptr<ULONG_PTR, decltype(&MmFreeContiguousMemory)> m_hvStack;

	CVmx m_vmx;
};
