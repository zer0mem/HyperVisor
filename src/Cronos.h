#pragma once

#include <Common/auto/MemoryObj.hpp>
#include "../../HyperVisor/src/VirtualizedCpu.h"

struct HV_CALLBACK
{
	VMTrap Callback;
	HV_CALLBACK* Next;
	
	HV_CALLBACK()
	{
		Callback = nullptr;
		Next = nullptr;
	}

	HV_CALLBACK(
		__in const VMTrap callback, 
		__in HV_CALLBACK* next
		)
	{
		Callback = callback;
		Next = next;
	}
};

class CCRonos
{
public:
	CCRonos();
	~CCRonos();

	void 
	Install();

	void 
	StopVirtualization();

	static 
	bool 
	RegisterCallback(
		__in HV_CALLBACK* callbacks, 
		__in const VMTrap callback 
		);	

protected:
	bool EnableVirtualization();

	static 
	void 
	HVCallback(
		__inout ULONG_PTR reg[REG_COUNT], 
		__in_opt const HV_CALLBACK** callbacks
		);

	static 
	void 
	HVCpuid(
		__inout ULONG_PTR reg[REG_COUNT]
		);

	virtual 
	void 
	PerCoreAction(
		__in BYTE coreId
		);

	virtual 
	__checkReturn 
	bool 
	SetVirtualizationCallbacks();

protected:
	HV_CALLBACK m_callbacks;
	VMTrap m_traps[MAX_HV_CALLBACK];

	ULONG_PTR m_exceptionsMask;

	KAFFINITY m_cpu;
	CMemObj<CVirtualizedCpu> m_vCpu;
};

#define kCpuidMark1	MAKEFOURCC(' ', 'o', 'p', 'e')
#define kCpuidMark2	MAKEFOURCC('n', ' ', 'g', 'a')
#define kCpuidMark3	MAKEFOURCC('t', 'e', '!', '\0')
