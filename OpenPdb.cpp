#include "STDAFX.H"

_NT_BEGIN

NTSTATUS OpenPdb(PHANDLE phFile, PCWSTR FilePath)
{
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName};
	RtlInitUnicodeString(&ObjectName, FilePath);
	IO_STATUS_BLOCK iosb;
	return NtOpenFile(phFile, FILE_GENERIC_READ, &oa, &iosb, FILE_SHARE_VALID_FLAGS, FILE_SYNCHRONOUS_IO_NONALERT);
}

NTSTATUS OpenPdb(PHANDLE phFile, PCSTR PdbFileName, PCWSTR NtSymbolPath, const GUID* Signature, ULONG Age)
{
	PWSTR FilePath = 0;
	ULONG BytesInMultiByteString, cb = 0, rcb, BytesInUnicodeString, SymInUnicodeString;
	NTSTATUS status;

	if (strchr(PdbFileName, '\\'))
	{
		PWSTR buf = 0;
		while (cb = MultiByteToWideChar(CP_UTF8, 0, PdbFileName, MAXULONG, buf, cb))
		{
			if (FilePath)
			{
				return OpenPdb(phFile, FilePath);
			}

			STATIC_WSTRING(GLOBAL, "\\GLOBAL??\\");
			FilePath = (PWSTR)alloca(cb * sizeof(WCHAR) + sizeof(GLOBAL) - sizeof(WCHAR));
			memcpy(FilePath, GLOBAL, sizeof(GLOBAL) - sizeof(WCHAR));
			buf = FilePath + _countof(GLOBAL) - 1;
		}

		return STATUS_UNSUCCESSFUL;
	}

	BytesInMultiByteString = (ULONG)strlen(PdbFileName) + 1;

	if (0 > (status = RtlMultiByteToUnicodeSize(&BytesInUnicodeString, PdbFileName, BytesInMultiByteString)))
	{
		return status;
	}

	SymInUnicodeString = BytesInUnicodeString >> 1;

	ULONG n = (ULONG)wcslen(NtSymbolPath) + 34 + SymInUnicodeString, m = Age;
	do 
	{
		n++;
	} while (m >>= 4);

	if ((rcb = (n + SymInUnicodeString) << 1) > cb)
	{
		FilePath = (PWSTR)alloca(rcb - cb);
	}

	if (0 > (status = RtlMultiByteToUnicodeN(FilePath + n, BytesInUnicodeString, &BytesInUnicodeString, PdbFileName, BytesInMultiByteString)))
	{
		return status;
	}

	swprintf(FilePath, L"%s\\%s\\%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%x", 
		NtSymbolPath, FilePath + n,
		Signature->Data1, Signature->Data2, Signature->Data3,
		Signature->Data4[0], Signature->Data4[1], Signature->Data4[2], Signature->Data4[3],
		Signature->Data4[4], Signature->Data4[5], Signature->Data4[6], Signature->Data4[7],
		Age);

	FilePath[n - 1] = '\\';

	return OpenPdb(phFile, FilePath);
}

struct PdbFileHeader;

#include "pdb_util.h"

NTSTATUS ParsePDB(PdbFileHeader* header, SIZE_T ViewSize, PGUID signature, DWORD age, SymStore* pss);

NTSTATUS SymStore::GetSymbols(PCWSTR PdbPath)
{
	HANDLE hFile;
	NTSTATUS status = OpenPdb(&hFile, PdbPath);

	return 0 > status ? status : GetSymbols(hFile, 0, 0);
}

NTSTATUS SymStore::GetSymbols(HANDLE hFile, const GUID* signature, DWORD age)
{
	HANDLE hSection;

	NTSTATUS status = NtCreateSection(&hSection, SECTION_MAP_READ, 0, 0, PAGE_READONLY, SEC_COMMIT, hFile);

	NtClose(hFile);

	if (0 > status) return status;

	PVOID BaseAddress = 0;
	SIZE_T ViewSize = 0;

	status = ZwMapViewOfSection(hSection, NtCurrentProcess(), &BaseAddress, 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_READONLY);

	NtClose(hSection);

	if (0 > status) return status;

	status = ParsePDB((PdbFileHeader*)BaseAddress, ViewSize, signature, age, this);

	ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);

	return status;
}

#define LDR_IS_DATAFILE(DllHandle) (((ULONG_PTR)(DllHandle)) & (ULONG_PTR)1)

NTSTATUS SymStore::GetSymbols(HMODULE hmod, PCWSTR NtSymbolPath)
{
	DWORD cb;
	BOOLEAN bMappedAsImage = !LDR_IS_DATAFILE(hmod);
	PVOID Base = PAGE_ALIGN(hmod);
	PIMAGE_DEBUG_DIRECTORY pidd = (PIMAGE_DEBUG_DIRECTORY)RtlImageDirectoryEntryToData(Base, bMappedAsImage, IMAGE_DIRECTORY_ENTRY_DEBUG, &cb);

	if (!pidd || !cb || (cb % sizeof(IMAGE_DEBUG_DIRECTORY))) return STATUS_NOT_FOUND;

	do 
	{
		if (pidd->Type == IMAGE_DEBUG_TYPE_CODEVIEW && pidd->SizeOfData > sizeof(CV_INFO_PDB))
		{
			if (DWORD PointerToRawData = bMappedAsImage ? pidd->AddressOfRawData : pidd->PointerToRawData)
			{
				CV_INFO_PDB* lpcvh = (CV_INFO_PDB*)RtlOffsetToPointer(Base, PointerToRawData);

				if (lpcvh->CvSignature == 'SDSR')
				{
					HANDLE hFile;
					NTSTATUS status = OpenPdb(&hFile, lpcvh->PdbFileName, 
						NtSymbolPath, &lpcvh->Signature, lpcvh->Age);

					return OnOpenPdb(0 > status ? status : GetSymbols(hFile, &lpcvh->Signature, lpcvh->Age), lpcvh);
				}
			}
		}

	} while (pidd++, cb -= sizeof(IMAGE_DEBUG_DIRECTORY));

	return STATUS_NOT_FOUND;
}

_NT_END