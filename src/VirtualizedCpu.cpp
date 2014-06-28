/**
 * @file VirtualizedCpu.cpp
 * @author created by: Peter Hlavaty
 */

#include <ntifs.h>
#include "VirtualizedCpu.h"

CVirtualizedCpu::CVirtualizedCpu( 
	__in BYTE cpuCore, 
	__in_opt const VMTrap traps[MAX_HV_CALLBACK], 
	__in_opt ULONG_PTR exceptionMask,
	__in_opt const VMCallback callback,
	__in_opt const VOID* param 
	) : m_vmx(PROCID(cpuCore), exceptionMask), 
		m_cpuCore(cpuCore),
		m_hvStack(static_cast<ULONG_PTR*>(
			MmAllocateContiguousMemory(HYPERVISOR_STACK_PAGE, GetTopAdress())),
			MmFreeContiguousMemory)
{
	if (!m_hvStack.get())
		return;

	RtlZeroMemory(m_hvStack.get(), HYPERVISOR_STACK_PAGE);

	m_hvStack.get()[0] = kStackMark;
	m_hvStack.get()[1] = (ULONG_PTR)param;
	::new(m_hvStack.get() + 2) CHyperVisor(cpuCore, traps, callback);
}

CVirtualizedCpu::~CVirtualizedCpu()
{
	if (!m_hvStack.get())
		return;

	KeSetSystemAffinityThread(PROCID(m_cpuCore));
	(reinterpret_cast<CHyperVisor*>(m_hvStack.get() + 1))->~CHyperVisor();
}

__checkReturn 
bool
CVirtualizedCpu::VirtualizationON()
{
	return m_vmx.InstallHyperVisor(CHyperVisor::HvExitPoint(), reinterpret_cast<void*>(reinterpret_cast<size_t>(m_hvStack.get()) + HYPERVISOR_STACK_PAGE - 1));
}

__checkReturn 
bool
CVirtualizedCpu::VirtualizationOFF()
{
	if (m_vmx.CpuActivated())
	{
		//return m_hv->Stop();
	}
	return false;
}

__forceinline
__checkReturn 
ULONG_PTR*
CVirtualizedCpu::GetTopOfStack( 
	__in const ULONG_PTR* stack
	)
{
	while (kStackMark != stack[0])
		stack = CommonRoutines::align<ULONG_PTR>(stack - 1, PAGE_SIZE);

	return const_cast<ULONG_PTR*>(stack);
}

__checkReturn 
BYTE 
CVirtualizedCpu::GetCoreId( 
	__in const ULONG_PTR* stack 
	)
{
	EVmErrors status;
	ULONG_PTR ds = Instrinsics::VmRead(VMX_VMCS16_GUEST_FIELD_DS, &status);
	
	xchgds(&ds);

	ULONG_PTR* stack_top = CVirtualizedCpu::GetTopOfStack(stack);
	CHyperVisor* hypervisor = reinterpret_cast<CHyperVisor*>(stack_top + 2);
	BYTE cored_id = hypervisor->GetCoredId();
	
	writeds(ds);

	return cored_id;
}

EXTERN_C 
VMTrap 
HVExitTrampoline( 
	__inout ULONG_PTR reg[REG_COUNT]
	)
{
	EVmErrors status;
	ULONG_PTR ds = Instrinsics::VmRead(VMX_VMCS16_GUEST_FIELD_DS, &status);

	xchgds(&ds);

	ULONG_PTR* stack_top = CVirtualizedCpu::GetTopOfStack(reg);
	CHyperVisor* hypervisor = reinterpret_cast<CHyperVisor*>(stack_top + 2);
	VMTrap handler = hypervisor->HVEntryPoint(reg, (stack_top + 1));

	writeds(ds);

	return handler;
}
