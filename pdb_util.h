#pragma once

#ifndef _CV_INFO_PDB_DEFINED_
#define _CV_INFO_PDB_DEFINED_

struct CV_INFO_PDB 
{
	DWORD CvSignature;
	GUID Signature;
	DWORD Age;
	char PdbFileName[];
};

#endif

struct __declspec(novtable) SymStore 
{
	NTSTATUS GetSymbols(HMODULE hmod, PCWSTR NtSymbolPath);
	
	NTSTATUS GetSymbols(PCWSTR PdbPath);

	virtual void Symbol(ULONG rva, PCSTR name) = 0;
	
	virtual BOOL EnumSymbolsEnd() = 0;
	
	virtual NTSTATUS OnOpenPdb(NTSTATUS status, CV_INFO_PDB* /*lpcvh*/)
	{
		return status;
	}

private:
	NTSTATUS GetSymbols(HANDLE hFile, PGUID signature, DWORD age);

};

