/**
 * @file VmmAutoExit.hpp
 * @author created by: Peter Hlavaty
 */

#ifndef __VMMAUTOEXIT_H__
#define __VMMAUTOEXIT_H__

#include "../Common/base/HVCommon.h"

class CVMMAutoExit
{
#define INTERRUPTION_INFO_TYPE_SHIFT 8
#define INTERRUPTION_INFO_TYPE(a) (((a) >> INTERRUPTION_INFO_TYPE_SHIFT) & 7)
public:
	__forceinline
	CVMMAutoExit() : 
		m_ip(NULL),
		m_insLen(0),
		m_sp(NULL),
		m_flags(0),
		m_intInfo(0)
	{
		EVmErrors status;
		m_intInfo = Instrinsics::VmRead(VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO, &status);
		if (VM_OK(status))
		{
			m_ip = reinterpret_cast<void*>(Instrinsics::VmRead(VMX_VMCS64_GUEST_RIP, &status));
			if (VM_OK(status))
			{
				m_sp = reinterpret_cast<ULONG_PTR*>(Instrinsics::VmRead(VMX_VMCS64_GUEST_RSP, &status));
				if (VM_OK(status))
				{
					m_flags = Instrinsics::VmRead(VMX_VMCS_GUEST_RFLAGS, &status);
					if (VM_OK(status))
					{
						m_reason = Instrinsics::VmRead(VMX_VMCS32_RO_EXIT_REASON, &status);
						if (VM_OK(status))
							m_insLen = Instrinsics::VmRead(VMX_VMCS32_RO_EXIT_INSTR_LENGTH, &status);
					}
				}
			}
		}
	}

	__forceinline
	~CVMMAutoExit()
	{
		NT_ASSERT(VM_OK(Instrinsics::VmWrite(VMX_VMCS64_GUEST_RIP, m_ip)));
		NT_ASSERT(VM_OK(Instrinsics::VmWrite(VMX_VMCS_GUEST_RFLAGS, m_flags)));
		NT_ASSERT(VM_OK(Instrinsics::VmWrite(VMX_VMCS64_GUEST_RSP, m_sp)));
	}

	__forceinline
	void DisableTrap()
	{
		m_flags &= ~TRAP;
	}

	__forceinline
	bool IsTrapActive()
	{
		return !!(m_flags & TRAP);
	}

	__forceinline
	const void* GetIp()
	{
		return m_ip;
	}

	__forceinline
	void SetIp(
		__in const void* ip
		)
	{
		m_ip = ip;
	}

	__forceinline
	void SetIpFromCallback(
		__in const void* ip
		)
	{
		m_ip = static_cast<const BYTE*>(ip) - m_insLen;
	}

	__forceinline
	ULONG_PTR* GetSp()
	{
		return m_sp;
	}

	__forceinline
	void SetSp(
		__in ULONG_PTR* sp
		)
	{
		m_sp = sp;
	}

	__forceinline
	ULONG_PTR GetFlags()
	{
		return m_flags;
	}

	__forceinline
	size_t GetInsLen()
	{
		return m_insLen;
	}

	__forceinline
	BYTE GetInterruptionInfo()
	{
		return static_cast<BYTE>(m_intInfo);
	}

	__forceinline
	ULONG_PTR GetReason()
	{
		return m_reason;
	}

protected:
	const void* m_ip;
	size_t m_insLen;
	ULONG_PTR* m_sp;
	ULONG_PTR m_flags;
	ULONG_PTR m_intInfo;
	ULONG_PTR m_reason;
};

#endif //__VMMAUTOEXIT_H__
