#include "stdafx.h"

_NT_BEGIN

#include "undname.h"
#include "module.h"
#include "../inc/rtlframe.h"

typedef RTL_FRAME<DATA_BLOB> AFRAME;

static void* __cdecl fAlloc(ULONG cb)
{
	if (AFRAME* prf = AFRAME::get())
	{
		if (cb > prf->cbData)
		{
			return 0;
		}
		prf->cbData -= cb;
		PVOID pv = prf->pbData;
		prf->pbData += cb;
		return pv;
	}

	return 0;
}

static void __cdecl fFree(void* )
{
}


PCSTR __cdecl GetParameter(long /*i*/)
{
	return "";
}

PSTR _unDName(PSTR undName, PCSTR rawName, DWORD cb, DWORD flags)
{
	PSTR psz = 0;
	if (PUCHAR pbData = new UCHAR[32*PAGE_SIZE])
	{
		AFRAME af;
		af.pbData = pbData, af.cbData = 32*PAGE_SIZE;
		psz = __unDNameEx(undName, rawName, cb, fAlloc, fFree, GetParameter, flags);
		delete [] pbData;
	}

	return psz;
}

PCSTR unDNameEx(PSTR undName, PCSTR rawName, DWORD cb, DWORD flags)
{
	if (*rawName != '?')
	{
		return rawName;
	}
	PSTR sz = _unDName(undName, rawName, cb, flags);
	return sz ? sz : rawName;
}

void WINAPI DumpStack(_In_ ULONG FramesToSkip, _In_ PCSTR txt, ULONG (__cdecl * print) ( PCSTR Format, ...))
{
	PVOID pv[32], *ppv;
	
	ULONG n = RtlWalkFrameChain(pv, _countof(pv), FramesToSkip << RTL_STACK_WALKING_MODE_FRAMES_TO_SKIP_SHIFT);
	
	if (n > FramesToSkip)
	{
		if (txt) print(">>> ************* %s\r\n", txt);

		n -= FramesToSkip, ppv = pv;

		do 
		{
			PVOID p = *ppv++;
			ULONG d;
			PCWSTR name;

			if (PCSTR psz = CModule::GetNameFromVa(p, &d, &name))
			{
				char undName[0x400];
				print(">> %p %S!%s + %x\r\n", p, name, unDNameEx(undName, psz, _countof(undName), UNDNAME_DEFAULT), d);
			}
			else
			{
				print(">> %p\n", p);
			}
		} while (--n);

		if (txt) print("<<<< ************* %s\r\n", txt);
	}
}

_NT_END