#include <StdAfx.h>
#include <MrMapi/MMErr.h>
#include <MrMapi/mmcli.h>
#include <core/utility/strings.h>

namespace error
{
	extern ERROR_ARRAY_ENTRY g_ErrorArray[];
	extern ULONG g_ulErrorArray;
} // namespace error

void PrintErrFromNum(_In_ ULONG ulError)
{
	wprintf(L"0x%08lX = %ws\n", ulError, error::ErrorNameFromErrorCode(ulError).c_str());
}

void PrintErrFromName(_In_ const std::wstring& lpszError) noexcept
{
	const auto szErr = lpszError.c_str();

	for (ULONG i = 0; i < error::g_ulErrorArray; i++)
	{
		if (0 == lstrcmpiW(szErr, error::g_ErrorArray[i].lpszName))
		{
			wprintf(L"0x%08lX = %ws\n", error::g_ErrorArray[i].ulErrorName, szErr);
		}
	}
}

void PrintErrFromPartialName(_In_ const std::wstring& lpszError) noexcept
{
	if (!lpszError.empty())
		wprintf(L"Searching for \"%ws\"\n", lpszError.c_str());
	else
		wprintf(L"Searching for all errors\n");

	ULONG ulNumMatches = 0;

	for (ULONG ulCur = 0; ulCur < error::g_ulErrorArray; ulCur++)
	{
		if (lpszError.empty() || nullptr != StrStrIW(error::g_ErrorArray[ulCur].lpszName, lpszError.c_str()))
		{
			wprintf(L"0x%08lX = %ws\n", error::g_ErrorArray[ulCur].ulErrorName, error::g_ErrorArray[ulCur].lpszName);
			ulNumMatches++;
		}
	}

	wprintf(L"Found %lu matches.\n", ulNumMatches);
}

void DoErrorParse()
{
	auto lpszErr = cli::switchUnswitched[0];
	const auto ulErrNum = strings::wstringToUlong(lpszErr, cli::switchDecimal.isSet() ? 10 : 16);

	if (ulErrNum)
	{
		PrintErrFromNum(ulErrNum);
	}
	else
	{
		if (cli::switchSearch.isSet() || lpszErr.empty())
		{
			PrintErrFromPartialName(lpszErr);
		}
		else
		{
			PrintErrFromName(lpszErr);
		}
	}
}