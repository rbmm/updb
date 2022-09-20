#include "stdafx.h"

_NT_BEGIN

#include "module.h"
#include "pdb_util.h"

LIST_ENTRY CModule::s_head = { &s_head, &s_head };

PVOID CModule::GetVaFromName(HMODULE hmod, PCSTR Name)
{
	CModule* p = 0;
	PLIST_ENTRY entry = &s_head;

	AcquireSRWLockShared(&_SRWLock);

	while ((entry = entry->Flink) != &s_head)
	{
		p = static_cast<CModule*>(entry);

		if (hmod == p->_ImageBase)
		{
			ReleaseSRWLockShared(&_SRWLock);

			return p->GetVaFromName(Name);
		}
	}

	ReleaseSRWLockShared(&_SRWLock);

	_LDR_DATA_TABLE_ENTRY* ldte;
	if (0 <= LdrFindEntryForAddress(hmod, &ldte))
	{
		if (0 <= CModule::Create(&ldte->BaseDllName, ldte->DllBase, ldte->SizeOfImage, &p))
		{
			AcquireSRWLockExclusive(&_SRWLock);
			InsertHeadList(&s_head, p);
			ReleaseSRWLockExclusive(&_SRWLock);
			return p->GetVaFromName(Name);
		}
	}

	return 0;
}

PCSTR CModule::GetNameFromVa(PVOID pv, PULONG pdisp, PCWSTR* ppname)
{
	CModule* p = 0;
	PLIST_ENTRY entry = &s_head;
	
	AcquireSRWLockShared(&_SRWLock);

	while ((entry = entry->Flink) != &s_head)
	{
		p = static_cast<CModule*>(entry);

		ULONG_PTR rva = (ULONG_PTR)pv - (ULONG_PTR)p->_ImageBase;
		
		if (rva < p->_size)
		{
			ReleaseSRWLockShared(&_SRWLock);

			return p->GetNameFromRva((ULONG)rva, pdisp, ppname);
		}
	}

	ReleaseSRWLockShared(&_SRWLock);

	_LDR_DATA_TABLE_ENTRY* ldte;
	if (0 <= LdrFindEntryForAddress(pv, &ldte))
	{
		if (0 <= CModule::Create(&ldte->BaseDllName, ldte->DllBase, ldte->SizeOfImage, &p))
		{
			AcquireSRWLockExclusive(&_SRWLock);
			InsertHeadList(&s_head, p);
			ReleaseSRWLockExclusive(&_SRWLock);
			return p->GetNameFromRva((ULONG)((ULONG_PTR)pv - (ULONG_PTR)ldte->DllBase), pdisp, ppname);
		}
	}

	return 0;
}

PVOID CModule::GetVaFromName(PCSTR Name)
{
	if (ULONG n = _nSymbols)
	{
		PULARGE_INTEGER offsets = _offsets;

		do 
		{
			if (!strcmp(Name, RtlOffsetToPointer(this, offsets->HighPart)))
			{
				return (PBYTE)_ImageBase + offsets->LowPart;
			}
		} while (offsets++, --n);
	}

	return 0;
}

PCSTR CModule::GetNameFromRva(ULONG rva, PULONG pdisp, PCWSTR* ppname)
{
	*ppname = _Name.Buffer;
	ULONG a = 0, b = _nSymbols, o, ofs;

	if (!b)
	{
		*pdisp = rva;
		return "MZ";
	}

	PULARGE_INTEGER offsets = _offsets;

	do 
	{
		if (rva == (ofs = offsets[o = (a + b) >> 1].LowPart))
		{
			*pdisp = 0;
			return RtlOffsetToPointer(this, offsets[o].HighPart);
		}

		rva < ofs ? b = o : a = o + 1;

	} while (a < b);

	if (rva < ofs)
	{
		if (!o)
		{
			return 0;
		}
		ofs = offsets[--o].LowPart;
	}

	*pdisp = (ULONG)rva - ofs;

	return RtlOffsetToPointer(this, offsets[o].HighPart);
}

NTSTATUS CModule::Create(PCUNICODE_STRING Name, PVOID ImageBase, ULONG size, CModule** ppmod)
{
	struct Z : SymStore 
	{
		CModule* _pModule = 0;
		PSTR _psz = 0;
		PULARGE_INTEGER _pSymbol = 0;
		CV_INFO_PDB* _lpcvh = 0;
		ULONG _nSymbols = 0;
		ULONG _cbNames = 0;

		virtual NTSTATUS OnOpenPdb(NTSTATUS status, CV_INFO_PDB* lpcvh)
		{
			_lpcvh = lpcvh;
			return status;
		}

		static int __cdecl cmp(const void* pa, const void* pb)
		{
			ULONG a = ((ULARGE_INTEGER*)pa)->LowPart, b = ((ULARGE_INTEGER*)pb)->LowPart;
			if (a < b) return -1;
			if (a > b) return +1;
			return 0;
		}

		virtual BOOL EnumSymbolsEnd()
		{
			CModule * pDll = _pModule;
			ULONG nSymbols = _nSymbols;

			if (!pDll)
			{
				if (nSymbols)
				{
					if (pDll = new(nSymbols, _cbNames) CModule)
					{
						_pModule = pDll;
						_pSymbol = pDll->_offsets;
						_psz = (PSTR)&pDll->_offsets[nSymbols];

						return FALSE;
					}
				}

				return TRUE;
			}

			qsort(pDll->_offsets, nSymbols, sizeof(ULARGE_INTEGER), cmp);

			return TRUE;
		}

		virtual void Symbol(ULONG rva, PCSTR name)
		{
			if ((name[0] == '?' && name[1] == '?') ||
				(name[0] == 'W' && name[1] == 'P' && name[2] == 'P' && name[3] == '_') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'i' && name[3] == 'm' && name[4] == 'p' && name[5] == '_') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'h' && name[3] == 'm' && name[4] == 'o' && name[5] == 'd') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'I' && name[3] == 'M' && name[4] == 'P' && name[5] == 'O') ||
				(name[0] == '_' && name[1] == '_' && name[2] == 'D' && name[3] == 'E' && name[4] == 'L' && name[5] == 'A')
				)
			{
				return ;
			}

			ULONG len = (ULONG)strlen(name) + 1;

			CModule * pDll = _pModule;

			if (!pDll)
			{
				_nSymbols++;
				_cbNames += len;
				return ;
			}

			_pSymbol->HighPart = RtlPointerToOffset(pDll, memcpy(_psz, name, len));
			_pSymbol++->LowPart = rva;
			_psz += len;
		}
	} ss;

	NTSTATUS status = ss.GetSymbols((HMODULE)ImageBase, L"\\systemroot\\symbols");

	CModule* pModule;

	if (0 > status)
	{
		if (pModule = new(0, 0) CModule)
		{
			if (CV_INFO_PDB* lpcvh = ss._lpcvh)
			{
				PCSTR PdbFileName = lpcvh->PdbFileName;
				if (PCSTR psz = strrchr(PdbFileName, '\\'))
				{
					PdbFileName = psz + 1;
				}

				GUID* Signature = &lpcvh->Signature;

				pModule->Init(L"%S*%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x", 
					PdbFileName, Signature->Data1, Signature->Data2, Signature->Data3,
					Signature->Data4[0], Signature->Data4[1], Signature->Data4[2], Signature->Data4[3],
					Signature->Data4[4], Signature->Data4[5], Signature->Data4[6], Signature->Data4[7],
					lpcvh->Age);

				pModule->_size = size, pModule->_ImageBase = ImageBase;
				goto __1;
				
			}
			goto __0;
		}
	}
	else
	{
		if (pModule = ss._pModule)
		{
__0:
			pModule->Init(Name, ImageBase, size);
__1:
			*ppmod = pModule;
			return STATUS_SUCCESS;
		}

		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

void CModule::Init(PCWSTR fmt, ...)
{
	PWSTR buf = 0;
	LONG cch = 0;
	va_list v;
	va_start(v, fmt);
	while (0 < (cch = _vsnwprintf(buf, cch, fmt, v)))
	{
		if (buf)
		{
			_b = TRUE;
			_Name.Buffer = buf;
			_Name.MaximumLength = (USHORT)(cch >>= 1);
			_Name.Length = (USHORT)(cch - 1);
			return ;
		}

		buf = new WCHAR[++cch];
	}

	delete [] buf;
}

void CModule::Cleanup()
{
	PLIST_ENTRY entry = s_head.Flink;

	while (entry != &s_head)
	{
		CModule* p = static_cast<CModule*>(entry);
		
		entry = entry->Flink;

		delete p;
	}
}

_NT_END

