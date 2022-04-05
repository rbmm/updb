#pragma once

class CModule : LIST_ENTRY
{
	UNICODE_STRING _Name {};
	PVOID _ImageBase;
	ULONG _size;
	ULONG _nSymbols;
	BOOL _b = FALSE;
	ULARGE_INTEGER _offsets[/*_nSymbols { rva, ofs }*/];
	//CHAR Names[];
	void Init(PCWSTR fmt, ...);

	void Init(PCUNICODE_STRING Name, PVOID ImageBase, ULONG size)
	{
		_size = size, _ImageBase = ImageBase;
		RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, Name, &_Name);
		DbgPrint("++CModule<%p>(%wZ) %p\n", this, Name, ImageBase);
	}

	static LIST_ENTRY s_head;

	CModule()
	{
		InsertHeadList(&s_head, this);
	}

	~CModule()
	{
		DbgPrint("--CModule<%p>(%wZ) %p\n", this, &_Name, _ImageBase);
		_b ? delete [] _Name.Buffer : RtlFreeUnicodeString(&_Name);
	}

	PCSTR GetNameFromRva(ULONG rva, PULONG pdisp, PCWSTR* ppname);
public:

	void* operator new(size_t s, ULONG nSymbols, ULONG cbNames)
	{
		if (PVOID pv = LocalAlloc(0, s + nSymbols * sizeof(ULARGE_INTEGER) + cbNames))
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

	static NTSTATUS Create(PCUNICODE_STRING Name, PVOID ImageBase, ULONG size, CModule** ppmod);
	static PCSTR GetNameFromVa(PVOID pv, PULONG pdisp, PCWSTR* ppname);

	static void Cleanup();
};

void LoadNtModule(ULONG n, const ULONG ph[]);
void DumpStack(PCSTR txt);
ULONG HashString(PCSTR lpsz, ULONG hash = 0);