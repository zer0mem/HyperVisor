#pragma once

#include "../Common/base/HVCommon.h"

class CVmx
{
	static
	void
	MmFreeNonCachedMemoryHVRes(
		__inout void* hvResource
		);
public:
	CVmx(
		__in KAFFINITY procId,
		__in size_t exceptionhandling = 0
		);

	__checkReturn 
	bool 
	InstallHyperVisor(
		__in const void* hvEntryPoint, 
		__in void* hvStack
		);
	
	__checkReturn
	EVmErrors 
	VmcsToRing0();
	
	//force inline and public
	__checkReturn 
	bool
	CpuActivated() const
	{
		return m_cpuActivated;
	}
	
//static
	static
	void 
	EnableVirtualization();

	static 
	__checkReturn 
	bool
	IsVirtualizationLocked();

	static
	__checkReturn
	bool 
	IsVirtualizationEnabled();

protected:
	__checkReturn 
	bool 
	VmcsInit();

	__checkReturn 
	bool
	GetGuestState(
		__in KAFFINITY procId
		);

	void 
	GetSegmentDescriptor(
		__out SEGMENT_SELECTOR* segSel, 
		__in ULONG_PTR selector
		);

	EVmErrors
	SetSegSelector(
		__in ULONG_PTR segSelector,
		__in ULONG_PTR segField
		);

	__checkReturn
	EVmErrors 
	SetCRx();

	__checkReturn
	EVmErrors 
	SetControls();

	__checkReturn
	EVmErrors 
	SetDT();

	__checkReturn
	EVmErrors 
	SetSysCall();

protected:
	bool m_cpuActivated;
	size_t m_exceptionMask;

private:
	bool m_preparedState;
	GUEST_STATE	m_guestState;

	std::unique_ptr<char, decltype(&MmFreeNonCachedMemoryHVRes)> m_pGVmcs;
	std::unique_ptr<char, decltype(&MmFreeNonCachedMemoryHVRes)> m_pHVmcs;
};
