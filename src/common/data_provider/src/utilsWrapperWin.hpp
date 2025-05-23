/*
 * Wazuh SysInfo
 * Copyright (C) 2025, Wazuh Inc.
 * March 31, 2025.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */
#pragma once

#ifdef _WIN32
#ifdef WIN_EXPORT
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif
#endif

#include <set>
#include <stdio.h>
#include <string>
#include <wbemcli.h>
#include <wbemidl.h>
#include <wuapi.h>

class EXPORTED IComHelper
{
public:
    virtual ~IComHelper() = default;

    // Abstracted methods for WMI functions
    virtual HRESULT CreateWmiLocator(IWbemLocator*& pLoc) = 0;
    virtual HRESULT ConnectToWmiServer(IWbemLocator* pLoc, IWbemServices*& pSvc) = 0;
    virtual HRESULT SetProxyBlanket(IWbemServices* pSvc) = 0;
    virtual HRESULT ExecuteWmiQuery(IWbemServices* pSvc, IEnumWbemClassObject*& pEnumerator) = 0;

    // Abstracted methods for Windows Update API functions
    virtual HRESULT CreateUpdateSearcher(IUpdateSearcher*& pUpdateSearcher) = 0;
    virtual HRESULT GetTotalHistoryCount(IUpdateSearcher* pUpdateSearcher, LONG& count) = 0;
    virtual HRESULT
    QueryHistory(IUpdateSearcher* pUpdateSearcher, IUpdateHistoryEntryCollection*& pHistory, LONG& count) = 0;
    virtual HRESULT GetCount(IUpdateHistoryEntryCollection* pHistory, LONG& count) = 0;
    virtual HRESULT GetItem(IUpdateHistoryEntryCollection* pHistory, LONG index, IUpdateHistoryEntry** pEntry) = 0;
    virtual HRESULT GetTitle(IUpdateHistoryEntry* pEntry, BSTR& title) = 0;
};

class EXPORTED ComHelper : public IComHelper
{
public:
    // Implement WMI functions
    HRESULT CreateWmiLocator(IWbemLocator*& pLoc) override;
    HRESULT ConnectToWmiServer(IWbemLocator* pLoc, IWbemServices*& pSvc) override;
    HRESULT SetProxyBlanket(IWbemServices* pSvc) override;
    HRESULT ExecuteWmiQuery(IWbemServices* pSvc, IEnumWbemClassObject*& pEnumerator) override;

    // Implement Windows Update API functions
    HRESULT CreateUpdateSearcher(IUpdateSearcher*& pUpdateSearcher) override;
    HRESULT GetTotalHistoryCount(IUpdateSearcher* pUpdateSearcher, LONG& count) override;
    HRESULT
    QueryHistory(IUpdateSearcher* pUpdateSearcher, IUpdateHistoryEntryCollection*& pHistory, LONG& count) override;
    HRESULT GetCount(IUpdateHistoryEntryCollection* pHistory, LONG& count) override;
    HRESULT GetItem(IUpdateHistoryEntryCollection* pHistory, LONG index, IUpdateHistoryEntry** pEntry) override;
    HRESULT GetTitle(IUpdateHistoryEntry* pEntry, BSTR& title) override;
};

// Queries Windows Management Instrumentation (WMI) to retrieve installed hotfixes
//  and stores them in the provided set.
EXPORTED void QueryWMIHotFixes(std::set<std::string>& hotfixSet, IComHelper& comHelper);

// Queries Windows Update Agent (WUA) for installed update history,
// extracts hotfixes, and adds them to the provided set.
EXPORTED void QueryWUHotFixes(std::set<std::string>& hotfixSet, IComHelper& comHelper);
