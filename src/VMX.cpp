/**
 * @file VMX.cpp
 * @author created by: Peter Hlavaty
 */

#include "VMX.h"
#include <Common/cpu/msr.h>

extern "C" 
void
__fastcall
get_guest_exit(
	__out ULONG_PTR* guestRip, 
	__out ULONG_PTR* guestRsp
	);

#define VMWRITE_ERR_QUIT(field, val) if (!VM_OK((status = Instrinsics::VmWrite((field), (val))))) return status;
#define VMWRITE_ERR_QUITB(field, val) if (!VM_OK((status = Instrinsics::VmWrite((field), (val))))) return !!status;

void
CVmx::MmFreeNonCachedMemoryHVRes(
	__inout void* hvResource
	)
{
	MmFreeNonCachedMemory(hvResource, PAGE_SIZE);
}

//////////////////////////////////////////////////////////////////////
// PUBLIC INTERFACE
//////////////////////////////////////////////////////////////////////

CVmx::CVmx(
	__in KAFFINITY procId,
	__in size_t exceptionhandling
	) : m_cpuActivated(false),
		m_exceptionMask(exceptionhandling),
		m_pGVmcs(static_cast<char*>(MmAllocateNonCachedMemory(PAGE_SIZE)), MmFreeNonCachedMemoryHVRes),
		m_pHVmcs(static_cast<char*>(MmAllocateNonCachedMemory(PAGE_SIZE)), MmFreeNonCachedMemoryHVRes)
{
	RtlZeroMemory(&m_guestState, sizeof(m_guestState));
	m_preparedState = GetGuestState(procId);
}

__checkReturn 
bool
CVmx::InstallHyperVisor( 
	__in const void* hvEntryPoint,
	__in void* hvStack 
	)
{
	if (m_preparedState)
	{
		m_guestState.HRIP = hvEntryPoint;
		m_guestState.HRSP = CommonRoutines::align<ULONG_PTR>(hvStack, 0x100);

		m_cpuActivated = VmcsInit();
	}

	return m_cpuActivated;
}

//not yet active
EVmErrors 
CVmx::VmcsToRing0()
{
	EVmErrors status;
	//set guest CR
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_CR0, m_guestState.CR0);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_CR4, m_guestState.CR4);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_DR7, 0x400 | DR7_GD/* | DR7_ENABLED_MASK*/);

	//set descriptor tables
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_IDTR_BASE, m_guestState.Idtr.base);
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_IDTR_LIMIT, m_guestState.Idtr.limit);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_GDTR_BASE, m_guestState.Gdtr.base);
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_GDTR_LIMIT, m_guestState.Gdtr.limit);	

	//SELECTORS
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_CS, m_guestState.Cs);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_DS, m_guestState.Ds);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_ES, m_guestState.Es);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_SS, m_guestState.Ss);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_FS, m_guestState.Fs);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_GS, m_guestState.Gs);

	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_RSP, reinterpret_cast<ULONG_PTR>(m_guestState.SESP));

	return EVmErrors::VM_ERROR_OK;
}

//////////////////////////////////////////////////////////////////////
// STATIC HELPERS (PUBLIC)
//////////////////////////////////////////////////////////////////////

__checkReturn 
bool 
CVmx::IsVirtualizationEnabled()
{
	return (0 != (rdmsr(IA32_FEATURE_CONTROL_CODE) & FEATURE_CONTROL_VMXON_ENABLED));
}

__checkReturn 
bool 
CVmx::IsVirtualizationLocked()
{
	return (0 != (rdmsr(IA32_FEATURE_CONTROL_CODE) & FEATURE_CONTROL_LOCKED));
}

void CVmx::EnableVirtualization()
{
	DbgPrint("Virtualization is trying to enable via wrmsr setting FEATURE_CONTROL_VMXON_ENABLED");
	wrmsr(IA32_FEATURE_CONTROL_CODE, rdmsr(IA32_FEATURE_CONTROL_CODE) | FEATURE_CONTROL_VMXON_ENABLED);
}

//////////////////////////////////////////////////////////////////////
// INNER PROCESSING (PROTECTED)
//////////////////////////////////////////////////////////////////////

__checkReturn 
bool 
CVmx::VmcsInit()
{
	ULONG_PTR guest_rsp;
	ULONG_PTR guest_rip;
	get_guest_exit(&guest_rsp, &guest_rip);

	if (m_cpuActivated)
		return true;

	KeSetSystemAffinityThread(m_guestState.ProcId);

	if (!IsVirtualizationEnabled())
	{
		if (IsVirtualizationLocked())
		{
			DbgPrint("\nVirtualization not supported from BIOS! [%x]", rdmsr(IA32_FEATURE_CONTROL_CODE));
			return false;
		}

		EnableVirtualization();
	}
	DbgPrint("\n=Virtualization is enabled! exc mask : %p //%p %p \n", m_exceptionMask, guest_rsp, guest_rip);

	{
		cli();
		writecr0(m_guestState.CR0);
		writecr4(m_guestState.CR4);
		sti();
	}

	*(reinterpret_cast<ULONG_PTR*>(m_guestState.GVmcs.pvmcs)) = rdmsr(IA32_VMX_BASIC_MSR_CODE);
	*(reinterpret_cast<ULONG_PTR*>(m_guestState.HVmcs.pvmcs)) = rdmsr(IA32_VMX_BASIC_MSR_CODE);

	vmxon(&(m_guestState.HVmcs.vmcs));

	vmclear(&(m_guestState.GVmcs.vmcs));

	vmptrld(&(m_guestState.GVmcs.vmcs));

	//GLOBALS
	EVmErrors status;
	VMWRITE_ERR_QUITB(VMX_VMCS_CTRL_ENTRY_MSR_LOAD_COUNT, 0);
	VMWRITE_ERR_QUITB(VMX_VMCS_CTRL_EXIT_MSR_LOAD_COUNT, 0);
	VMWRITE_ERR_QUITB(VMX_VMCS_CTRL_EXIT_MSR_STORE_COUNT, 0);

	//set controls
	if (!VM_OK(SetCRx()) ||
		!VM_OK(SetControls()) ||
		!VM_OK(SetDT()) ||
		!VM_OK(SetSysCall()))
	{
		return false;
	}

	//GUEST
	VMWRITE_ERR_QUITB(VMX_VMCS_GUEST_LINK_PTR_FULL, -1);
	VMWRITE_ERR_QUITB(VMX_VMCS_GUEST_LINK_PTR_HIGH, -1);

	VMWRITE_ERR_QUITB(VMX_VMCS_GUEST_DEBUGCTL_FULL, rdmsr(IA32_DEBUGCTL) & SEG_D_LIMIT);
	VMWRITE_ERR_QUITB(VMX_VMCS_GUEST_DEBUGCTL_HIGH, rdmsr(IA32_DEBUGCTL) >> 32);

	//SELECTORS
	if (!VM_OK(SetSegSelector(m_guestState.Cs, VMX_VMCS16_GUEST_FIELD_CS)) ||
		!VM_OK(SetSegSelector(m_guestState.Ds, VMX_VMCS16_GUEST_FIELD_DS)) ||
		!VM_OK(SetSegSelector(m_guestState.Es, VMX_VMCS16_GUEST_FIELD_ES)) || 
		!VM_OK(SetSegSelector(m_guestState.Ss, VMX_VMCS16_GUEST_FIELD_SS)) ||
		!VM_OK(SetSegSelector(m_guestState.Fs, VMX_VMCS16_GUEST_FIELD_FS)) ||
		!VM_OK(SetSegSelector(m_guestState.Gs, VMX_VMCS16_GUEST_FIELD_GS)))
	{
		return false;
	}

	DbgPrint("\n > VMX_VMCS16_GUEST_FIELD_GS : %p\n", m_guestState.Gs);

	if (!VM_OK(SetSegSelector(m_guestState.Ldtr, VMX_VMCS16_GUEST_FIELD_LDTR)) ||
		!VM_OK(SetSegSelector(m_guestState.Tr, VMX_VMCS16_GUEST_FIELD_TR)))
	{
		return false;
	}
	
	VMWRITE_ERR_QUITB(VMX_VMCS64_GUEST_FS_BASE, rdmsr(IA32_FS_BASE));
	VMWRITE_ERR_QUITB(VMX_VMCS64_GUEST_GS_BASE, rdmsr(IA32_GS_BASE));

	//HOST
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_CS, m_guestState.Cs);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_DS, SEG_DATA);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_ES, SEG_DATA);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_SS, m_guestState.Ss);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_FS, m_guestState.Fs & 0xf8);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_GS, m_guestState.Gs & 0xf8);
	VMWRITE_ERR_QUITB(VMX_VMCS16_HOST_FIELD_TR, m_guestState.Tr);

	RtlZeroMemory(reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(m_guestState.GVmcs.pvmcs) + 4), sizeof(void*) - 4);

	VMWRITE_ERR_QUITB(VMX_VMCS64_GUEST_RSP, guest_rsp);
	VMWRITE_ERR_QUITB(VMX_VMCS64_GUEST_RIP, guest_rip);
	VMWRITE_ERR_QUITB(VMX_VMCS_GUEST_RFLAGS, m_guestState.RFLAGS);

	VMWRITE_ERR_QUITB(VMX_VMCS_HOST_RSP, reinterpret_cast<ULONG_PTR>(m_guestState.HRSP));
	VMWRITE_ERR_QUITB(VMX_VMCS_HOST_RIP, reinterpret_cast<ULONG_PTR>(m_guestState.HRIP));
	
	if (m_exceptionMask)
	{
		//activate exception handling
		ULONG_PTR int_state = Instrinsics::VmRead(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &status);
		if (VM_OK(status))
		{
			ULONG_PTR intercepts = Instrinsics::VmRead(VMX_VMCS_CTRL_EXCEPTION_BITMAP, &status);
			if (VM_OK(status))
			{
				if((int_state & 3))
				{
					int_state &= ~(3);
					VMWRITE_ERR_QUITB(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, int_state);
				}
				intercepts |= m_exceptionMask;
				VMWRITE_ERR_QUITB(VMX_VMCS_CTRL_EXCEPTION_BITMAP, intercepts);
			}
		}
	}
	
	//handle pagefault via VMX_EXIT_EPT_VIOLATION
	/*
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_EPTP_FULL, m_guestState.CR3 | VMX_EPT_MEMTYPE_WB | (VMX_EPT_PAGE_WALK_LENGTH_DEFAULT << VMX_EPT_PAGE_WALK_LENGTH_SHIFT));
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_EPTP_HIGH, m_guestState.CR3 >> 32);
	*/

	DbgPrint("\ncr0 %p", m_guestState.CR0);	
	DbgPrint("\ncr3 %p", m_guestState.CR3);
	DbgPrint("\ncr4 %p", m_guestState.CR4);

	//set descriptor tables
	DbgPrint("\nidtr base %p", m_guestState.Idtr.base);
	DbgPrint("\nidtr limit %p", m_guestState.Idtr.limit);
	DbgPrint("\ngdtr base %p", m_guestState.Gdtr.base);
	DbgPrint("\ngdtr limit %p", m_guestState.Gdtr.limit);	

	//SELECTORS
	DbgPrint("\ncs  %p", m_guestState.Cs);

	DbgPrint("\nds  %p", m_guestState.Ds);
	DbgPrint("\nes  %p", m_guestState.Es);
	DbgPrint("\nss  %p", m_guestState.Ss);	
	DbgPrint("\nfs  %p", m_guestState.Fs);
	DbgPrint("\ngs  %p", m_guestState.Gs);	

	DbgPrint("\nldtr %p", m_guestState.Ldtr);
	DbgPrint("\ntr  %p", m_guestState.Tr);

	m_cpuActivated = true;

	vmlaunch();

	DbgPrint("\nHYPERVISOR IS NOT TURNED ON, something failed!\n");
	DbgBreakPoint();
	return false;
}

__checkReturn 
bool 
CVmx::GetGuestState( 
	__in KAFFINITY procId 
	)
{
	m_guestState.ProcId = procId;
	KeSetSystemAffinityThread(procId);

	m_guestState.CR0 = (readcr0() & rdmsr(IA32_VMX_CR0_FIXED1)) | rdmsr(IA32_VMX_CR0_FIXED0) | CR0_PE | CR0_NE | CR0_PG;
	m_guestState.CR4 = (readcr4() & rdmsr(IA32_VMX_CR4_FIXED1)) | rdmsr(IA32_VMX_CR4_FIXED0) | CR4_VMXE | CR4_DE;

	m_guestState.CR3 = readcr3();
	m_guestState.RFLAGS = readeflags();

	m_guestState.Cs = readcs();
	m_guestState.Ds = readds();
	m_guestState.Es = reades();
	m_guestState.Ss = readss();
	m_guestState.Fs = readfs();
	m_guestState.Gs = readgs();

	m_guestState.PIN = reinterpret_cast<BYTE*>(rdmsr(IA32_VMX_PINBASED_CTLS) & SEG_D_LIMIT);
	m_guestState.PROC = reinterpret_cast<BYTE*>((rdmsr(IA32_VMX_PROCBASED_CTLS) & SEG_D_LIMIT) | CPU_BASED_RDTSC_EXITING /*| CPU_BASED_MOV_DR_EXITING*/);
	m_guestState.EXIT = reinterpret_cast<BYTE*>((rdmsr(IA32_VMX_EXIT_CTLS) & SEG_D_LIMIT | (1 << 15)) | VMX_VMCS32_EXIT_IA32E_MODE | VMX_VMCS32_EXIT_ACK_ITR_ON_EXIT);
	m_guestState.ENTRY = reinterpret_cast<BYTE*>((rdmsr(IA32_VMX_ENTRY_CTLS)& SEG_D_LIMIT) | VMX_VMCS32_ENTRY_IA32E_MODE);
	m_guestState.SEIP = reinterpret_cast<BYTE*>(rdmsr(IA64_SYSENTER_EIP) & SEG_D_LIMIT);
	m_guestState.SESP = reinterpret_cast<BYTE*>(rdmsr(IA32_SYSENTER_ESP) & SEG_D_LIMIT);

	m_guestState.GVmcs.pvmcs = MmAllocateNonCachedMemory(PAGE_SIZE);
	if (NULL == m_pGVmcs.get())
		return false;

	RtlZeroMemory(m_guestState.GVmcs.pvmcs, PAGE_SIZE);
	m_guestState.GVmcs.vmcs = MmGetPhysicalAddress(m_guestState.GVmcs.pvmcs);

	m_guestState.HVmcs.pvmcs = MmAllocateNonCachedMemory(PAGE_SIZE);
	if (NULL == m_pHVmcs.get())
		return false;

	RtlZeroMemory(m_guestState.HVmcs.pvmcs, PAGE_SIZE);
	m_guestState.HVmcs.vmcs = MmGetPhysicalAddress(m_guestState.HVmcs.pvmcs);


	sgdt(&(m_guestState.Gdtr));
	sidt(&(m_guestState.Idtr));

	m_guestState.Tr = str();
	m_guestState.Ldtr = sldt();

	return true;
}

EVmErrors
CVmx::SetCRx()
{
	EVmErrors status;
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_CR0_READ_SHADOW, CR0_PG);//PG
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_CR4_READ_SHADOW, 0);
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_CR3_TARGET_COUNT, 0);

	//CR GUEST
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_CR0, m_guestState.CR0);	
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_CR3, m_guestState.CR3);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_CR4, m_guestState.CR4);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_DR7, 0x400 | DR7_GD);

	//CR HOST
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_CR0, m_guestState.CR0);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_CR3, m_guestState.CR3);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_CR4, m_guestState.CR4);
	return status;
}

EVmErrors 
CVmx::SetControls()
{
	EVmErrors status;
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, reinterpret_cast<ULONG_PTR>(m_guestState.PIN));
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, reinterpret_cast<ULONG_PTR>(m_guestState.PROC));
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_EXIT_CONTROLS, reinterpret_cast<ULONG_PTR>(m_guestState.EXIT));
	VMWRITE_ERR_QUIT(VMX_VMCS_CTRL_ENTRY_CONTROLS, reinterpret_cast<ULONG_PTR>(m_guestState.ENTRY));
	return status;
}

EVmErrors 
CVmx::SetDT()
{
	EVmErrors status;
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_IDTR_BASE, m_guestState.Idtr.base);
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_IDTR_LIMIT, m_guestState.Idtr.limit);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_GDTR_BASE, m_guestState.Gdtr.base);
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_GDTR_LIMIT, m_guestState.Gdtr.limit);	

	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_FS_BASE, rdmsr(IA32_FS_BASE) & SEG_Q_LIMIT);
	SEGMENT_SELECTOR seg_sel;
	GetSegmentDescriptor(&seg_sel, m_guestState.Tr);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_TR_BASE, seg_sel.base);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_GDTR_BASE, m_guestState.Gdtr.base);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_IDTR_BASE, m_guestState.Idtr.base);
	return status;
}

EVmErrors
CVmx::SetSysCall()
{
	EVmErrors status;

	struct CS_STAR
	{
		union
		{
			ULONG_PTR Value;
			struct
			{
				ULONG_PTR Reserved:0x20;
				ULONG_PTR SyscallCs:0x10;
				ULONG_PTR SysretCs:0x10;
			};
		};
	};

	CS_STAR cs = { rdmsr(IA32_STAR) };
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_SYSENTER_CS,  cs.SyscallCs & SEG_D_LIMIT);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_SYSENTER_ESP, reinterpret_cast<ULONG_PTR>(m_guestState.SESP));
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_SYSENTER_EIP, reinterpret_cast<ULONG_PTR>(m_guestState.SEIP));

	VMWRITE_ERR_QUIT(VMX_VMCS32_HOST_SYSENTER_CS, cs.SyscallCs & SEG_D_LIMIT);
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_SYSENTER_EIP, reinterpret_cast<ULONG_PTR>(m_guestState.SEIP));
	VMWRITE_ERR_QUIT(VMX_VMCS_HOST_SYSENTER_ESP, reinterpret_cast<ULONG_PTR>(m_guestState.SESP));
	return status;
}

void 
CVmx::GetSegmentDescriptor( 
	__out SEGMENT_SELECTOR* segSel, 
	__in ULONG_PTR selector 
	)
{
	RtlZeroMemory(segSel, sizeof(SEGMENT_SELECTOR));	
	SEGMENT_DESCRIPTOR* seg = (SEGMENT_DESCRIPTOR *)((PUCHAR)m_guestState.Gdtr.base + (selector >> 3) * 8);	

	segSel->selector = selector;
	segSel->limit =	(ULONG)(seg->LimitLow | (seg->LimitHigh << 16));
	segSel->base = seg->BaseLow | (seg->BaseMid << 16) | (seg->BaseHigh << 24);
	segSel->attributes = (USHORT)(seg->AttributesLow | (seg->AttributesHigh << 8));

	//is TSS or HV_CALLBACK ?
	if (!(seg->AttributesLow & NORMAL))
		segSel->base = (segSel->base & SEG_D_LIMIT) | ((*(PULONG64) ((PUCHAR)seg + 8)) << 32);

	if (segSel->attributes >> IS_GRANULARITY_4KB == 1)
		segSel->limit = (segSel->limit << 12) | 0xFFFF;

	segSel->rights = (segSel->selector ? (((PUCHAR) &segSel->attributes)[0] + (((PUCHAR) &segSel->attributes)[1] << 12)) : 0x10000);
}

EVmErrors 
CVmx::SetSegSelector( 
	__in ULONG_PTR segSelector, 
	__in ULONG_PTR segField 
	)
{
	EVmErrors status;
	size_t index = (segField - VMX_VMCS16_GUEST_FIELD_ES);

	SEGMENT_SELECTOR seg_sel;
	GetSegmentDescriptor(&seg_sel, segSelector);

	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_ES_LIMIT + index, seg_sel.limit);	
	VMWRITE_ERR_QUIT(VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS + index, seg_sel.rights);
	VMWRITE_ERR_QUIT(VMX_VMCS16_GUEST_FIELD_ES + index, segSelector);
	VMWRITE_ERR_QUIT(VMX_VMCS64_GUEST_ES_BASE + index, seg_sel.base);
	return status;
}
