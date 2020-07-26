#include <StdAfx.h>
#include <MrMapi/MMPropTag.h>
#include <MrMapi/mmcli.h>
#include <core/interpret/guid.h>
#include <core/smartview/SmartView.h>
#include <core/utility/strings.h>
#include <core/addin/addin.h>
#include <core/addin/mfcmapi.h>
#include <core/utility/output.h>
#include <core/interpret/proptags.h>
#include <core/interpret/proptype.h>

// Searches a NAMEID_ARRAY_ENTRY array for a target dispid.
// Exact matches are those that match
// If no hits, then ulNoMatch should be returned for lpulFirstExact
void FindNameIDArrayMatches(
	_In_ LONG lTarget,
	_In_count_(ulMyArray) NAMEID_ARRAY_ENTRY* MyArray,
	_In_ ULONG ulMyArray,
	_Out_ ULONG* lpulNumExacts,
	_Out_ ULONG* lpulFirstExact) noexcept
{
	ULONG ulLowerBound = 0;
	auto ulUpperBound = ulMyArray - 1; // ulMyArray-1 is the last entry
	auto ulMidPoint = (ulUpperBound + ulLowerBound) / 2;
	ULONG ulFirstMatch = cache::ulNoMatch;
	ULONG ulLastMatch = cache::ulNoMatch;

	if (lpulNumExacts) *lpulNumExacts = 0;
	if (lpulFirstExact) *lpulFirstExact = cache::ulNoMatch;

	// find A match
	while (ulUpperBound - ulLowerBound > 1)
	{
		if (lTarget == MyArray[ulMidPoint].lValue)
		{
			ulFirstMatch = ulMidPoint;
			break;
		}

		if (lTarget < MyArray[ulMidPoint].lValue)
		{
			ulUpperBound = ulMidPoint;
		}
		else if (lTarget > MyArray[ulMidPoint].lValue)
		{
			ulLowerBound = ulMidPoint;
		}
		ulMidPoint = (ulUpperBound + ulLowerBound) / 2;
	}

	// When we get down to two points, we may have only checked one of them
	// Make sure we've checked the other
	if (lTarget == MyArray[ulUpperBound].lValue)
	{
		ulFirstMatch = ulUpperBound;
	}
	else if (lTarget == MyArray[ulLowerBound].lValue)
	{
		ulFirstMatch = ulLowerBound;
	}

	// Check that we got a match
	if (cache::ulNoMatch != ulFirstMatch)
	{
		ulLastMatch = ulFirstMatch; // Remember the last match we've found so far

		// Scan backwards to find the first match
		while (ulFirstMatch > 0 && lTarget == MyArray[ulFirstMatch - 1].lValue)
		{
			ulFirstMatch = ulFirstMatch - 1;
		}

		// Scan forwards to find the real last match
		// Last entry in the array is ulPropTagArray-1
		while (ulLastMatch + 1 < ulMyArray && lTarget == MyArray[ulLastMatch + 1].lValue)
		{
			ulLastMatch = ulLastMatch + 1;
		}

		ULONG ulNumMatches = 0;

		if (cache::ulNoMatch != ulFirstMatch)
		{
			ulNumMatches = ulLastMatch - ulFirstMatch + 1;
		}

		if (lpulNumExacts) *lpulNumExacts = ulNumMatches;
		if (lpulFirstExact) *lpulFirstExact = ulFirstMatch;
	}
}

// prints the type of a prop tag
// no pretty stuff or \n - calling function gets to do that
void PrintType(_In_ ULONG ulPropTag) noexcept
{
	auto bNeedInstance = false;

	if (ulPropTag & MV_INSTANCE)
	{
		ulPropTag &= ~MV_INSTANCE;
		bNeedInstance = true;
	}

	// Do a linear search through PropTypeArray - ulPropTypeArray will be small
	auto bFound = false;

	for (const auto& propType : PropTypeArray)
	{
		if (PROP_TYPE(ulPropTag) == propType.ulValue)
		{
			bFound = true;
			wprintf(L"%ws", propType.lpszName);
			break;
		}
	}

	if (!bFound) wprintf(L"0x%04lX = Unknown type", PROP_TYPE(ulPropTag));

	if (bNeedInstance) wprintf(L" | MV_INSTANCE");
}

void PrintKnownTypes() noexcept
{
	// Do a linear search through PropTypeArray - ulPropTypeArray will be small
	wprintf(L"%-18s%-9s%ws\n", L"Type", L"Hex", L"Decimal");
	for (const auto& propType : PropTypeArray)
	{
		wprintf(L"%-15ws = 0x%04lX = %lu\n", propType.lpszName, propType.ulValue, propType.ulValue);
	}

	wprintf(L"\n");
	wprintf(L"Types may also have the flag 0x%04X = %ws\n", MV_INSTANCE, L"MV_INSTANCE");
}

// Print the tag found in the array at ulRow
void PrintTag(_In_ ULONG ulRow) noexcept
{
	wprintf(L"0x%08lX,%ws,", PropTagArray[ulRow].ulValue, PropTagArray[ulRow].lpszName);
	PrintType(PropTagArray[ulRow].ulValue);
	wprintf(L"\n");
}

// Given a property tag, output the matching names and partially matching names
// Matching names will be presented first, with partial matches following
// Example:
// C:\>MrMAPI 0x3001001F
// Property tag 0x3001001F:
//
// Exact matches:
// 0x3001001F,PR_DISPLAY_NAME_W,PT_UNICODE
// 0x3001001F,PidTagDisplayName,PT_UNICODE
//
// Partial matches:
// 0x3001001E,PR_DISPLAY_NAME,PT_STRING8
// 0x3001001E,PR_DISPLAY_NAME_A,PT_STRING8
// 0x3001001E,ptagDisplayName,PT_STRING8
void PrintTagFromNum(_In_ ULONG ulPropTag)
{
	ULONG ulPropID = NULL;

	if (ulPropTag & 0xffff0000) // dealing with a full prop tag
	{
		ulPropID = PROP_ID(ulPropTag);
	}
	else
	{
		ulPropID = ulPropTag;
	}

	wprintf(L"Property tag 0x%08lX:\n", ulPropTag);

	if (ulPropID & 0x8000)
	{
		wprintf(L"Since this property ID is greater than 0x8000 there is a good chance it is a named property.\n");
		wprintf(L"Only trust the following output if this property is known to be an address book property.\n");
	}

	std::vector<ULONG> ulExacts;
	std::vector<ULONG> ulPartials;
	proptags::FindTagArrayMatches(ulPropTag, true, PropTagArray, ulExacts, ulPartials);

	if (!ulExacts.empty())
	{
		wprintf(L"\nExact matches:\n");
		for (const auto& ulCur : ulExacts)
		{
			PrintTag(ulCur);
		}
	}

	if (!ulPartials.empty())
	{
		wprintf(L"\nPartial matches:\n");
		// let's print our partial matches
		for (const auto& ulCur : ulPartials)
		{
			if (ulPropTag == PropTagArray[ulCur].ulValue) continue; // skip our exact matches
			PrintTag(ulCur);
		}
	}
}

void PrintTagFromName(_In_z_ LPCWSTR lpszPropName, _In_ ULONG ulType)
{
	if (!lpszPropName) return;

	if (cache::ulNoMatch != ulType)
	{
		wprintf(L"Restricting output to ");
		PrintType(ulType);
		wprintf(L"\n");
	}

	auto bMatchFound = false;

	for (ULONG ulCur = 0; ulCur < PropTagArray.size(); ulCur++)
	{
		if (0 == lstrcmpiW(lpszPropName, PropTagArray[ulCur].lpszName))
		{
			if (cache::ulNoMatch == ulType)
			{
				PrintTag(ulCur);
			}

			// now that we have a match, let's see if we have other tags with the same number
			const auto ulExactMatch = ulCur; // The guy that matched lpszPropName

			std::vector<ULONG> ulExacts;
			std::vector<ULONG> ulPartials;
			proptags::FindTagArrayMatches(PropTagArray[ulExactMatch].ulValue, true, PropTagArray, ulExacts, ulPartials);

			// We're gonna skip at least one, so only print if we have more than one
			if (ulExacts.size() > 1)
			{
				if (cache::ulNoMatch == ulType)
				{
					wprintf(L"\nOther exact matches:\n");
				}

				for (const auto& ulMatch : ulExacts)
				{
					if (cache::ulNoMatch == ulType && ulExactMatch == ulMatch) continue; // skip this one
					if (cache::ulNoMatch != ulType && ulType != PROP_TYPE(PropTagArray[ulMatch].ulValue)) continue;
					PrintTag(ulMatch);
				}
			}

			if (!ulPartials.empty())
			{
				if (cache::ulNoMatch == ulType)
				{
					wprintf(L"\nOther partial matches:\n");
				}

				for (const auto& ulMatch : ulPartials)
				{
					if (PropTagArray[ulExactMatch].ulValue == PropTagArray[ulMatch].ulValue)
						continue; // skip our exact matches
					if (cache::ulNoMatch != ulType && ulType != PROP_TYPE(PropTagArray[ulMatch].ulValue)) continue;
					PrintTag(ulMatch);
				}
			}

			bMatchFound = true;
			break;
		}
	}

	if (!bMatchFound) wprintf(L"Property tag \"%ws\" not found\n", lpszPropName);
}

// Search for properties matching lpszPropName on a substring
// If ulType isn't ulNoMatch, restrict on the property type as well
void PrintTagFromPartialName(_In_opt_z_ LPCWSTR lpszPropName, _In_ ULONG ulType) noexcept
{
	if (lpszPropName)
		wprintf(L"Searching for \"%ws\"\n", lpszPropName);
	else
		wprintf(L"Searching for all properties\n");

	if (cache::ulNoMatch != ulType)
	{
		wprintf(L"Restricting output to ");
		PrintType(ulType);
		wprintf(L"\n");
	}

	ULONG ulNumMatches = 0;

	for (ULONG ulCur = 0; ulCur < PropTagArray.size(); ulCur++)
	{
		if (!lpszPropName || nullptr != StrStrIW(PropTagArray[ulCur].lpszName, lpszPropName))
		{
			if (cache::ulNoMatch != ulType && ulType != PROP_TYPE(PropTagArray[ulCur].ulValue)) continue;
			PrintTag(ulCur);
			ulNumMatches++;
		}
	}

	wprintf(L"Found %lu matches.\n", ulNumMatches);
}

void PrintGUID(_In_ LPCGUID lpGUID) noexcept
{
	// PSUNKNOWN isn't a real guid - just a placeholder - don't print it
	if (!lpGUID || IsEqualGUID(*lpGUID, guid::PSUNKNOWN))
	{
		wprintf(L",");
		return;
	}

	printf(
		"{%.8lX-%.4X-%.4X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X}",
		lpGUID->Data1,
		lpGUID->Data2,
		lpGUID->Data3,
		lpGUID->Data4[0],
		lpGUID->Data4[1],
		lpGUID->Data4[2],
		lpGUID->Data4[3],
		lpGUID->Data4[4],
		lpGUID->Data4[5],
		lpGUID->Data4[6],
		lpGUID->Data4[7]);

	wprintf(L",");
	for (const auto& guid : PropGuidArray)
	{
		if (IsEqualGUID(*lpGUID, *guid.lpGuid))
		{
			wprintf(L"%ws", guid.lpszName);
			break;
		}
	}
}

void PrintGUIDs() noexcept
{
	for (const auto& guid : PropGuidArray)
	{
		printf(
			"{%.8lX-%.4X-%.4X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X}",
			guid.lpGuid->Data1,
			guid.lpGuid->Data2,
			guid.lpGuid->Data3,
			guid.lpGuid->Data4[0],
			guid.lpGuid->Data4[1],
			guid.lpGuid->Data4[2],
			guid.lpGuid->Data4[3],
			guid.lpGuid->Data4[4],
			guid.lpGuid->Data4[5],
			guid.lpGuid->Data4[6],
			guid.lpGuid->Data4[7]);
		wprintf(L",%ws\n", guid.lpszName);
	}
}

void PrintDispID(_In_ ULONG ulRow) noexcept
{
	wprintf(L"0x%04lX,%ws,", NameIDArray[ulRow].lValue, NameIDArray[ulRow].lpszName);
	PrintGUID(NameIDArray[ulRow].lpGuid);
	wprintf(L",");
	if (PT_UNSPECIFIED != NameIDArray[ulRow].ulType)
	{
		PrintType(PROP_TAG(NameIDArray[ulRow].ulType, 0));
	}

	wprintf(L",");
	if (NameIDArray[ulRow].lpszArea)
	{
		wprintf(L"%ws", NameIDArray[ulRow].lpszArea);
	}

	wprintf(L"\n");
}

void PrintDispIDFromNum(_In_ ULONG ulDispID) noexcept
{
	ULONG ulNumExacts = NULL;
	ULONG ulFirstExactMatch = cache::ulNoMatch;

	wprintf(L"Dispid tag 0x%04lX:\n", ulDispID);

	FindNameIDArrayMatches(
		ulDispID, NameIDArray.data(), static_cast<ULONG>(NameIDArray.size()), &ulNumExacts, &ulFirstExactMatch);

	if (ulNumExacts > 0 && cache::ulNoMatch != ulFirstExactMatch)
	{
		wprintf(L"\nExact matches:\n");
		for (auto ulCur = ulFirstExactMatch; ulCur < ulFirstExactMatch + ulNumExacts; ulCur++)
		{
			PrintDispID(ulCur);
		}
	}
}

void PrintDispIDFromName(_In_opt_z_ LPCWSTR lpszDispIDName) noexcept
{
	if (!lpszDispIDName) return;

	auto bMatchFound = false;

	for (ULONG ulCur = 0; ulCur < NameIDArray.size(); ulCur++)
	{
		if (0 == lstrcmpiW(lpszDispIDName, NameIDArray[ulCur].lpszName))
		{
			PrintDispID(ulCur);

			// now that we have a match, let's see if we have other dispids with the same number
			const auto ulExactMatch = ulCur; // The guy that matched lpszPropName
			ULONG ulNumExacts = NULL;
			ULONG ulFirstExactMatch = cache::ulNoMatch;

			FindNameIDArrayMatches(
				NameIDArray[ulExactMatch].lValue,
				NameIDArray.data(),
				static_cast<ULONG>(NameIDArray.size()),
				&ulNumExacts,
				&ulFirstExactMatch);

			// We're gonna skip at least one, so only print if we have more than one
			if (ulNumExacts > 1)
			{
				wprintf(L"\nOther exact matches:\n");
				for (ulCur = ulFirstExactMatch; ulCur < ulFirstExactMatch + ulNumExacts; ulCur++)
				{
					if (ulExactMatch == ulCur) continue; // skip this one
					PrintDispID(ulCur);
				}
			}

			bMatchFound = true;
			break;
		}
	}

	if (!bMatchFound) wprintf(L"Property tag \"%ws\" not found\n", lpszDispIDName);
}

// Search for properties matching lpszPropName on a substring
void PrintDispIDFromPartialName(_In_opt_z_ LPCWSTR lpszDispIDName, _In_ ULONG ulType) noexcept
{
	if (lpszDispIDName)
		wprintf(L"Searching for \"%ws\"\n", lpszDispIDName);
	else
		wprintf(L"Searching for all properties\n");

	ULONG ulNumMatches = 0;

	for (ULONG ulCur = 0; ulCur < NameIDArray.size(); ulCur++)
	{
		if (!lpszDispIDName || nullptr != StrStrIW(NameIDArray[ulCur].lpszName, lpszDispIDName))
		{
			if (cache::ulNoMatch != ulType && ulType != NameIDArray[ulCur].ulType) continue;
			PrintDispID(ulCur);
			ulNumMatches++;
		}
	}

	wprintf(L"Found %lu matches.\n", ulNumMatches);
}

void PrintFlag(_In_ ULONG ulPropNum, _In_opt_z_ LPCWSTR lpszPropName, _In_ bool bIsDispid, _In_ ULONG ulFlagValue)
{
	std::wstring szFlags;
	size_t ulCur = 0;
	if (bIsDispid)
	{
		if (ulPropNum)
		{
			for (const auto& nameID : NameIDArray)
			{
				if (ulPropNum == static_cast<ULONG>(nameID.lValue))
				{
					break;
				}

				ulCur++;
			}
		}
		else if (lpszPropName)
		{
			for (const auto& nameID : NameIDArray)
			{
				if (nameID.lpszName && 0 == lstrcmpiW(lpszPropName, nameID.lpszName))
				{
					break;
				}

				ulCur++;
			}
		}

		if (ulCur != NameIDArray.size())
		{
			wprintf(L"Found named property %ws (0x%04lX) ", NameIDArray[ulCur].lpszName, NameIDArray[ulCur].lValue);
			PrintGUID(NameIDArray[ulCur].lpGuid);
			wprintf(L".\n");
			szFlags = smartview::InterpretNumberAsStringNamedProp(
				ulFlagValue, NameIDArray[ulCur].lValue, NameIDArray[ulCur].lpGuid);
		}
	}
	else
	{
		if (ulPropNum)
		{
			for (ulCur = 0; ulCur < PropTagArray.size(); ulCur++)
			{
				if (ulPropNum == PropTagArray[ulCur].ulValue)
				{
					break;
				}
			}
		}
		else if (lpszPropName)
		{
			for (ulCur = 0; ulCur < PropTagArray.size(); ulCur++)
			{
				if (0 == lstrcmpiW(lpszPropName, PropTagArray[ulCur].lpszName))
				{
					break;
				}
			}
		}

		if (ulCur != PropTagArray.size())
		{
			wprintf(L"Found property %ws (0x%08lX).\n", PropTagArray[ulCur].lpszName, PropTagArray[ulCur].ulValue);
			szFlags = smartview::InterpretNumberAsStringProp(ulFlagValue, PropTagArray[ulCur].ulValue);
		}
	}

	if (!szFlags.empty())
	{
		wprintf(L"0x%08lX = %ws\n", ulFlagValue, szFlags.c_str());
	}
	else
	{
		wprintf(L"No flag parsing found.\n");
	}
}

void DoPropTags()
{
	auto propName = cli::switchUnswitched[0];
	const auto lpszPropName = propName.empty() ? nullptr : propName.c_str();
	const auto ulPropNum = strings::wstringToUlong(propName, cli::switchDecimal.isSet() ? 10 : 16);
	if (lpszPropName) output::DebugPrint(output::dbgLevel::Generic, L"lpszPropName = %ws\n", lpszPropName);
	output::DebugPrint(output::dbgLevel::Generic, L"ulPropNum = 0x%08X\n", ulPropNum);
	const auto ulTypeNum =
		cli::switchType.empty() ? cache::ulNoMatch : proptype::PropTypeNameToPropType(cli::switchType[0]);

	// Handle dispid cases
	if (cli::switchDispid.isSet())
	{
		if (cli::switchFlag.hasULONG(0, 16))
		{
			PrintFlag(ulPropNum, lpszPropName, true, cli::switchFlag.atULONG(0, 16));
		}
		else if (cli::switchSearch.isSet())
		{
			PrintDispIDFromPartialName(lpszPropName, ulTypeNum);
		}
		else if (ulPropNum)
		{
			PrintDispIDFromNum(ulPropNum);
		}
		else
		{
			PrintDispIDFromName(lpszPropName);
		}

		return;
	}

	// Handle prop tag cases
	if (cli::switchFlag.hasULONG(0, 16))
	{
		PrintFlag(ulPropNum, lpszPropName, false, cli::switchFlag.atULONG(0, 16));
	}
	else if (cli::switchSearch.isSet())
	{
		PrintTagFromPartialName(lpszPropName, ulTypeNum);
	}
	else if (lpszPropName && !ulPropNum)
	{
		PrintTagFromName(lpszPropName, ulTypeNum);
	}
	// If we weren't asked about a property, maybe we were asked about types
	else if (cli::switchType.isSet())
	{
		if (cache::ulNoMatch != ulTypeNum)
		{
			PrintType(PROP_TAG(ulTypeNum, 0));
			wprintf(L" = 0x%04lX = %lu", ulTypeNum, ulTypeNum);
			wprintf(L"\n");
		}
		else
		{
			PrintKnownTypes();
		}
	}
	else if (ulPropNum)
	{
		PrintTagFromNum(ulPropNum);
	}
}

void DoGUIDs() noexcept { PrintGUIDs(); }

void DoFlagSearch() noexcept
{
	const auto lpszFlagName = cli::switchFlag[0];
	for (const auto& flag : FlagArray)
	{
		if (strings::compareInsensitive(flag.lpszName, lpszFlagName))
		{
			wprintf(L"%ws = 0x%08lX\n", flag.lpszName, flag.lFlagValue);
			break;
		}
	}
}