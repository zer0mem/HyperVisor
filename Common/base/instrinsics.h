/**
 * @file instrinsics.h
 * @author created by: Peter Hlavaty
 */

#ifndef __INSTRINSICS_H__
#define __INSTRINSICS_H__

#include <intrin.h>

EXTERN_C ULONG_PTR __rol(ULONG_PTR val, BYTE rotation);
EXTERN_C ULONG_PTR __ror(ULONG_PTR val, BYTE rotation);

EXTERN_C void __cli();
EXTERN_C void __sti();

EXTERN_C ULONG_PTR __str();
EXTERN_C ULONG_PTR __sldt();
EXTERN_C ULONG_PTR __sgdt(__out void * gdtr);
EXTERN_C ULONG_PTR __vmx_call(__in ULONG_PTR);

EXTERN_C ULONG_PTR __readcs();
EXTERN_C ULONG_PTR __readds();
EXTERN_C ULONG_PTR __reades();
EXTERN_C ULONG_PTR __readss();
EXTERN_C ULONG_PTR __readfs();
EXTERN_C ULONG_PTR __readgs();

EXTERN_C ULONG_PTR __xchgds(__inout ULONG_PTR* ds);
EXTERN_C ULONG_PTR __writeds(__in ULONG_PTR ds);

#define rdmsr(reg)				__readmsr(reg)
#define wrmsr(reg,val64)		__writemsr(reg, val64)

#define ROL(val, rotation)		__rol(val, rotation)
#define ROR(val, rotation)		__ror(val, rotation)

#define cli()					__cli()
#define sti()					__sti()
#define sgdt(dtp)				__sgdt(dtp)
#define sidt(dtp)				__sidt(dtp)
#define str()					__str()
#define sldt()					__sldt()
#define readcr0()				__readcr0()
#define readcr2()				__readcr2()
#define readcr3()				__readcr3()
#define readcr4()				__readcr4()
#define readeflags()			__readeflags()
#define writecr0(cr0)			__writecr0(cr0)
#define writecr4(cr4)			__writecr4(cr4)
#define	rdtsc()					__rdtsc()

#define vmxon(vmcs)				__vmx_on(reinterpret_cast<ULONG_PTR*>(vmcs))
#define vmclear(vmcs)			__vmx_vmclear(reinterpret_cast<ULONG_PTR*>(vmcs))
#define vmptrld(vmcs)			__vmx_vmptrld(reinterpret_cast<ULONG_PTR*>(vmcs))
#define vmlaunch()				__vmx_vmlaunch()
#define vmresume()				__vmx_vmresume()
#define vmxoff()				__vmx_off()
#define vmcall(magic)			__vmx_call(magic)

#define readcs()				__readcs()
#define readds()				__readds()
#define reades()				__reades()
#define readss()				__readss()
#define readfs()				__readfs()
#define readgs()				__readgs()

#define xchgds(ds)				__xchgds(ds)
#define writeds(ds)				__writeds(ds)


enum EVmErrors
{
	VM_ERROR_OK = 0,
	VM_ERROR_ERR_INFO_OK,
	VM_ERROR_ERR_INFO_ERR,
};

#define VM_OK(status) (status == EVmErrors::VM_ERROR_OK)
#define VM_ERR_NOINFO(status) (status == EVmErrors::VM_ERROR_ERR_INFO_ERR)
#define VM_ERR_INFO(status) (status == EVmErrors::VM_ERROR_ERR_INFO_OK)

namespace Instrinsics
{	
	__forceinline
	ULONG_PTR VmRead(
		__in size_t field,
		__inout EVmErrors* err = NULL
		)
	{
		size_t val;
		EVmErrors _err = (EVmErrors)__vmx_vmread(field, &val);

		if (err)
			*err = _err;

		NT_ASSERT(VM_OK(_err));
		return static_cast<ULONG_PTR>(val);
	}

	__forceinline
	__checkReturn
	EVmErrors VmWrite(
		__in size_t field, 
		__inout ULONG_PTR val 
		)
	{
		EVmErrors err = (EVmErrors)__vmx_vmwrite(field, static_cast<size_t>(val));
		NT_ASSERT(VM_OK(err));
		return err;
	}
};

#endif //__INSTRINSICS_H__
