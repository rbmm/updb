#include "stdafx.h"

_NT_BEGIN

#include "undname.h"
#include "module.h"
#include "../inc/rtlframe.h"

typedef RTL_FRAME<DATA_BLOB> AFRAME;

static void* __cdecl fAlloc(ULONG cb)
{
	if (DATA_BLOB* prf = AFRAME::get())
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
	return const_cast<PSTR>("");
}

PSTR _unDName(PSTR buffer, PCSTR mangled, DWORD cb, DWORD flags)
{
	PSTR psz = 0;
	if (PUCHAR pbData = new UCHAR[32*PAGE_SIZE])
	{
		AFRAME af;
		af.pbData = pbData, af.cbData = 32*PAGE_SIZE;
		psz = __unDNameEx(buffer, mangled, cb, fAlloc, fFree, GetParameter, flags);
		delete [] pbData;
	}

	return psz;
}

#define CASE_XY(x, y) case x: c = y; break

PSTR UndecorateString(_In_ PSTR pszSym)
{
	BOOL bUnicode;
	PSTR pc = pszSym, name = pszSym;

	switch (*pc++)
	{
	case '0':
		bUnicode = FALSE;
		break;

	case '1':
		bUnicode = TRUE;
		break;

	default:
		//__debugbreak();
		return 0;
	}

	if (*pc - '0' >= 10 && !(pc = strchr(pc, '@')))
	{
		//__debugbreak();
		return 0;
	}

	if (pc = strchr(pc + 1, '@'))
	{
		if (bUnicode)
		{
			*pszSym++ = 'L';
		}
		*pszSym++ = '\"';
	}
	else
	{
		//__debugbreak();
		return 0;
	}

	int i = 0;
	char c;

	while ('@' != (c = *++pc))
	{
		// special char ?
		union {
			USHORT u = 0;
			char pp[2];
		};

		if ('?' == c)
		{
			switch (*++pc)
			{
			case '$':
				pp[1] = *++pc, pp[0] = *++pc;

				switch (u)
				{
					CASE_XY('AA', 0);
					CASE_XY('AH', '.');//\a
					CASE_XY('AI', '.');//\b
					CASE_XY('AM', '.');//\f
					CASE_XY('AL', '.');//\v
					CASE_XY('AN', '.');//\r
					CASE_XY('CC', '\"');
					CASE_XY('HL', '{');
					CASE_XY('HN', '}');
					CASE_XY('FL', '[');
					CASE_XY('FN', ']');
					CASE_XY('CI', '(');
					CASE_XY('CJ', ')');
					CASE_XY('DM', '<');
					CASE_XY('DO', '>');
					CASE_XY('GA', '`');
					CASE_XY('CB', '!');
					CASE_XY('EA', '@');
					CASE_XY('CD', '#');
					CASE_XY('CF', '%');
					CASE_XY('FO', '^');
					CASE_XY('CG', '&');
					CASE_XY('CK', '*');
					CASE_XY('CL', '+');
					CASE_XY('HO', '~');
					CASE_XY('DN', '=');
					CASE_XY('HM', '|');
					CASE_XY('DL', ';');
					CASE_XY('DP', '?');
				default:
					return 0;
				}
				break;
				CASE_XY('0', ',');
				CASE_XY('1', '/');
				CASE_XY('2', '\\');
				CASE_XY('3', ':');
				CASE_XY('4', '.');
				CASE_XY('5', ' ');
				CASE_XY('6', '.');//\n
				CASE_XY('7', '.');//\t
				CASE_XY('8', '\'');
				CASE_XY('9', '-');
			case '@':
				//__debugbreak();
			default:
				return 0;
			}
		}

		if (bUnicode)
		{
			if (++i & 1)
			{
				if (c)
				{
					//__debugbreak();
					return 0;
				}
				continue;
			}
		}

		*pszSym++ = c;
	}

	*pszSym++ = '\"', *pszSym = 0;

	if (*++pc)
	{
		if (PSTR pa = strchr(pc, '@'))
		{
			*pa++ = 0;

			if (*pa)
			{
				//__debugbreak();
				return 0;
			}
		}
		else
		{
			//__debugbreak();
			return 0;
		}
	}

	return name;
}

PSTR UndecorateString(PCSTR pszFunc)
{
	size_t len = strlen(pszFunc) + 1;
	return UndecorateString((PSTR)memcpy(alloca(len + 1), pszFunc, len));
}

PCSTR unDNameEx(_Out_ PSTR buffer, _In_ PCSTR mangled, _In_ DWORD cb, _In_ DWORD flags)
{
	if (*mangled != '?')
	{
		return mangled;
	}
	PSTR sz = _unDName(buffer, mangled, cb, flags);
	return sz ? sz : mangled;
}

void WINAPI DumpStack(_In_ ULONG FramesToSkip, _In_ PCSTR txt, ULONG (__cdecl * print) ( PCSTR Format, ...))
{
	PVOID pv[64], *ppv;
	
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

			if (PCSTR psz = CModule::s_GetNameFromVa(p, &d, &name))
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