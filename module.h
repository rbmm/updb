#pragma once

#include "pdb_util.h"

class CModule : LIST_ENTRY
{
	UNICODE_STRING _Name {};
	PVOID _ImageBase;
	ULONG _size;
	BOOL _b = FALSE;
	ULONG _nSymbols;
	RVAOFS _Symbols[];
	//CHAR Names[];
	void Init(PCWSTR fmt, ...);

	void Init(PCUNICODE_STRING Name, PVOID ImageBase, ULONG size)
	{
		_size = size, _ImageBase = ImageBase;
		RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, Name, &_Name);
		//DbgPrint("++CModule<%p>(%wZ) %p\n", this, Name, ImageBase);
	}

	static LIST_ENTRY s_head;
	inline static SRWLOCK _SRWLock;

	~CModule()
	{
		//DbgPrint("--CModule<%p>(%wZ) %p\n", this, &_Name, _ImageBase);
		_b ? delete [] _Name.Buffer : RtlFreeUnicodeString(&_Name);
	}

	PVOID GetVaFromName(PCSTR Name);
	PCSTR GetNameFromRva(ULONG rva, PULONG pdisp, PCWSTR* ppname);
	static NTSTATUS Create(PCUNICODE_STRING Name, PVOID ImageBase, ULONG size, CModule** ppmod);
public:

	void* operator new(size_t s, ULONG nSymbols, size_t cbNames)
	{
		if (PVOID pv = LocalAlloc(0, s + nSymbols * sizeof(RVAOFS) + cbNames))
		{
			reinterpret_cast<CModule*>(pv)->_nSymbols = nSymbols;
			return pv;
		}
		return 0;
	}

	void operator delete(void* pv)
	{
		LocalFree(pv);
	}

	static PVOID GetVaFromName(HMODULE hmod, PCSTR Name);
	static PCSTR GetNameFromVa(PVOID pv, PULONG pdisp, PCWSTR* ppname);

	static void Cleanup();
};

void WINAPI DumpStack(_In_ ULONG FramesToSkip, _In_ PCSTR txt = 0, ULONG (__cdecl * print) ( PCSTR Format, ...) = DbgPrint);
