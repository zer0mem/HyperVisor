#pragma once

#include <ntifs.h>
#include <minwindef.h>

#include <memory>

#include "../../Common/CPU/vmcs.h"
#include "instrinsics.h"

//#define ALIGN(addr, granularity)	std::align(granularity, 1, addr, 1)//(ULONG_PTR)((ULONG_PTR)(addr) & (~((granularity) - 1)))//

namespace CommonRoutines
{
	__checkReturn
	template<typename Type_t>
	Type_t*
	align(
		__in const void* ptr,
		__in size_t alignment,
		__in_opt size_t sizeReq = 1,
		__in_opt size_t sizeAvailable = static_cast<size_t>(~0)
		)
	{
		return static_cast<Type_t*>(std::align(alignment, sizeReq, const_cast<void*&>(ptr), sizeAvailable));
	};
};


#pragma pack(push, 1)

struct GATE_DESCRIPTOR
{
	WORD Offset;
	DWORD Access;
	WORD Selector;
	DWORD ExtendedOffset;
	DWORD Reserved;
};

struct SEGMENT_DESCRIPTOR
{
	ULONG_PTR LimitLow : 16;
	ULONG_PTR BaseLow : 16;
	ULONG_PTR BaseMid : 8;
	ULONG_PTR AttributesLow : 8;
	ULONG_PTR LimitHigh : 4;
	ULONG_PTR AttributesHigh : 4;
	ULONG_PTR BaseHigh : 8;
};

//SEGMENT_DESCRIPTOR.Attributes (-Gap)...
typedef union
{
	USHORT UCHARs;
	struct FIELDS
	{
		USHORT type:4;              /* 0;  Bit 40-43 */
		USHORT s:1;                 	/* 4;  Bit 44 */
		USHORT dpl:2;               	/* 5;  Bit 45-46 */
		USHORT p:1;                 	/* 7;  Bit 47 */
		// gap!   (this will be explained later)     
		USHORT avl:1;               	/* 8;  Bit 52 */
		USHORT l:1;                 	/* 9;  Bit 53 */
		USHORT db:1;                	/* 10; Bit 54 */
		USHORT g:1;                 	/* 11; Bit 55 */
		USHORT Gap:4;
	};
} SEGMENT_ATTRIBUTES;

struct SEGMENT_SELECTOR
{
	ULONG_PTR selector;
	DWORD limit;
	ULONG_PTR base;
	ULONG_PTR rights;
	USHORT attributes;
};

struct GDT
{
	WORD limit;
	ULONG_PTR base;
};

struct VMCS
{
	void* pvmcs;
	PHYSICAL_ADDRESS vmcs;
};

struct GUEST_STATE
{
	KAFFINITY ProcId;
	VMCS GVmcs;
	VMCS HVmcs;
	PBYTE PIN;
	PBYTE PROC;
	PBYTE EXIT;
	PBYTE ENTRY;
	PBYTE SEIP;
	PBYTE SESP;
	GDT Gdtr;
	GDT Idtr;
	ULONG_PTR Ldtr;
	ULONG_PTR Tr;
	ULONG_PTR Gs;
	ULONG_PTR Fs;
	ULONG_PTR Ss;
	ULONG_PTR Es;
	ULONG_PTR Ds;
	ULONG_PTR Cs;
	ULONG_PTR CR4;
	ULONG_PTR CR3;
	ULONG_PTR CR0;
	ULONG_PTR* HRSP;
	ULONG_PTR RFLAGS;
	const VOID*	HRIP;
};

#pragma pack(pop)

#define LBR 1
#define BTF 2
#define TRAP 0x100

#define HYPERVISOR_STACK_PAGE	(2 * PAGE_SIZE)

#define CALL(proc)				((VOID (*)()) & proc)

#define SEG_DATA				0x10
#define SEG_CODE				0x18
#define SEG_Q_LIMIT				0xFFFFFFFFFFFFFFFF
#define SEG_D_LIMIT				SEG_Q_LIMIT

#define NORMAL					0x10
#define	IS_GRANULARITY_4KB		0xB

#define BTS(b)					(1 << b)
#define CR4_VMXE				static_cast<ULONG>(BTS(13))
#define CR4_DE					static_cast<ULONG>(BTS(3))
#define CR0_PG					static_cast<ULONG>(BTS(31))
#define CR0_NE					static_cast<ULONG>(BTS(5))
#define CR0_PE					static_cast<ULONG>(BTS(0))

#define FEATURE_CONTROL_LOCKED			BTS(0)
#define FEATURE_CONTROL_VMXON_ENABLED	BTS(2)

#define	MAX_HV_CALLBACK			VMX_EXIT_XSETBV+2

enum
{
	G_GS = 0,
	G_FS,
	G_ES,
	G_DS,
	G_RIP,
	G_CS,
	G_RFLAGS,
	G_RSP,
	G_SS
};

#define MAX_PROCID (sizeof(ULONG) << 3) //*8 .. byte => 8bite

#define	PROCID(cpu) (KAFFINITY)((ULONG_PTR)KeQueryActiveProcessors() & (ULONG_PTR)(1 << (USHORT)cpu))

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                  \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define kCpuidMark	MAKEFOURCC('P', 'I', 'L', 'L')
#define kStackMark	MAKEFOURCC('C', 'O', 'L', 'D')

//------------------------------------------------------------------
// ****************** DEFINE PUSHAQ order of regs ******************
//------------------------------------------------------------------

enum RegSetx86
{
	RDI = 0,
	RSI,
	RBP,
	RSP,
	RBX,
	RDX,
	RCX,
	RAX,
	REG_X86_COUNT
};

enum RegSetx64
{
	R15 = REG_X86_COUNT,
	R14,
	R13,
	R12,
	R11,
	R10,
	R9,
	R8,
	REG_X64_COUNT
};

#define REG_COUNT REG_X64_COUNT

enum RegFastCallX64Volatile
{
	VOLATILE_REG_RCX = 0,
	VOLATILE_REG_RDX,
	VOLATILE_REG_R8,
	VOLATILE_REG_R9,
	VOLATILE_REG_COUNT
};
