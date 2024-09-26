#include "WinDNSHelper.hpp"

#include <WbemIdl.h>
#include <comdef.h>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <vector>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

namespace ZeroTier {

BOOL RegDelnodeRecurse(HKEY hKeyRoot, LPTSTR lpSubKey)
{
    LPTSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;

    // First, see if we can delete the key without having
    // to recurse.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS)
        return TRUE;

    lResult = RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS) {
        if (lResult == ERROR_FILE_NOT_FOUND) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }

    // Check for an ending slash and add one if it is missing.

    lpEnd = lpSubKey + lstrlen(lpSubKey);

    if (*(lpEnd - 1) != TEXT('\\')) {
        *lpEnd = TEXT('\\');
        lpEnd++;
        *lpEnd = TEXT('\0');
    }

    // Enumerate the keys

    dwSize = MAX_PATH;
    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL, NULL, NULL, &ftWrite);

    if (lResult == ERROR_SUCCESS) {
        do {
            *lpEnd = TEXT('\0');
            StringCchCat(lpSubKey, MAX_PATH * 2, szName);

            if (! RegDelnodeRecurse(hKeyRoot, lpSubKey)) {
                break;
            }

            dwSize = MAX_PATH;

            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL, NULL, NULL, &ftWrite);

        } while (lResult == ERROR_SUCCESS);
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey(hKey);

    // Try again to delete the key.

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);

    if (lResult == ERROR_SUCCESS)
        return TRUE;

    return FALSE;
}

//*************************************************************
//
//  RegDelnode()
//
//  Purpose:    Deletes a registry key and all its subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL RegDelnode(HKEY hKeyRoot, LPCTSTR lpSubKey)
{
    TCHAR szDelKey[MAX_PATH * 2];

    StringCchCopy(szDelKey, MAX_PATH * 2, lpSubKey);
    return RegDelnodeRecurse(hKeyRoot, szDelKey);
}
std::vector<std::string> getSubKeys(const char* key)
{
    std::vector<std::string> subkeys;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        TCHAR achKey[MAX_KEY_LENGTH];          // buffer for subkey name
        DWORD cbName;                          // size of name string
        TCHAR achClass[MAX_PATH] = TEXT("");   // buffer for class name
        DWORD cchClassName = MAX_PATH;         // size of class string
        DWORD cSubKeys = 0;                    // number of subkeys
        DWORD cbMaxSubKey;                     // longest subkey size
        DWORD cchMaxClass;                     // longest class string
        DWORD cValues;                         // number of values for key
        DWORD cchMaxValue;                     // longest value name
        DWORD cbMaxValueData;                  // longest value data
        DWORD cbSecurityDescriptor;            // size of security descriptor
        FILETIME ftLastWriteTime;              // last write time

        DWORD i, retCode;

        TCHAR achValue[MAX_VALUE_NAME];
        DWORD cchValue = MAX_VALUE_NAME;

        retCode = RegQueryInfoKey(
            hKey,                    // key handle
            achClass,                // buffer for class name
            &cchClassName,           // size of class string
            NULL,                    // reserved
            &cSubKeys,               // number of subkeys
            &cbMaxSubKey,            // longest subkey size
            &cchMaxClass,            // longest class string
            &cValues,                // number of values for this key
            &cchMaxValue,            // longest value name
            &cbMaxValueData,         // longest value data
            &cbSecurityDescriptor,   // security descriptor
            &ftLastWriteTime);       // last write time

        for (i = 0; i < cSubKeys; ++i) {
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime);
            if (retCode == ERROR_SUCCESS) {
                subkeys.push_back(achKey);
            }
        }
    }
    RegCloseKey(hKey);
    return subkeys;
}

std::vector<std::string> getValueList(const char* key)
{
    std::vector<std::string> values;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        TCHAR achKey[MAX_KEY_LENGTH];          // buffer for subkey name
        DWORD cbName;                          // size of name string
        TCHAR achClass[MAX_PATH] = TEXT("");   // buffer for class name
        DWORD cchClassName = MAX_PATH;         // size of class string
        DWORD cSubKeys = 0;                    // number of subkeys
        DWORD cbMaxSubKey;                     // longest subkey size
        DWORD cchMaxClass;                     // longest class string
        DWORD cValues;                         // number of values for key
        DWORD cchMaxValue;                     // longest value name
        DWORD cbMaxValueData;                  // longest value data
        DWORD cbSecurityDescriptor;            // size of security descriptor
        FILETIME ftLastWriteTime;              // last write time

        DWORD i, retCode;

        TCHAR achValue[MAX_VALUE_NAME];
        DWORD cchValue = MAX_VALUE_NAME;

        retCode = RegQueryInfoKey(
            hKey,                    // key handle
            achClass,                // buffer for class name
            &cchClassName,           // size of class string
            NULL,                    // reserved
            &cSubKeys,               // number of subkeys
            &cbMaxSubKey,            // longest subkey size
            &cchMaxClass,            // longest class string
            &cValues,                // number of values for this key
            &cchMaxValue,            // longest value name
            &cbMaxValueData,         // longest value data
            &cbSecurityDescriptor,   // security descriptor
            &ftLastWriteTime);       // last write time

        for (i = 0, retCode = ERROR_SUCCESS; i < cValues; ++i) {
            cchValue = MAX_VALUE_NAME;
            achValue[0] = '\0';
            retCode = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL, NULL, NULL);
            if (retCode == ERROR_SUCCESS) {
                values.push_back(achValue);
            }
        }
    }
    RegCloseKey(hKey);
    return values;
}

std::pair<bool, std::string> WinDNSHelper::hasDNSConfig(uint64_t nwid)
{
    char networkStr[20] = { 0 };
    sprintf(networkStr, "%.16llx", nwid);

    const char* baseKey = "SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\DnsPolicyConfig";
    auto subkeys = getSubKeys(baseKey);
    for (auto it = subkeys.begin(); it != subkeys.end(); ++it) {
        char sub[MAX_KEY_LENGTH] = { 0 };
        sprintf(sub, "%s\\%s", baseKey, it->c_str());
        auto dnsRecords = getValueList(sub);
        for (auto it2 = dnsRecords.begin(); it2 != dnsRecords.end(); ++it2) {
            if ((*it2) == "Comment") {
                HKEY hKey;
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    char buf[16384] = { 0 };
                    DWORD size = sizeof(buf);
                    DWORD retCode = RegGetValueA(HKEY_LOCAL_MACHINE, sub, it2->c_str(), RRF_RT_REG_SZ, NULL, &buf, &size);
                    if (retCode == ERROR_SUCCESS) {
                        if (std::string(networkStr) == std::string(buf)) {
                            RegCloseKey(hKey);
                            return std::make_pair(true, std::string(sub));
                        }
                    }
                    else {
                    }
                }
                RegCloseKey(hKey);
            }
        }
    }

    return std::make_pair(false, std::string());
}

void WinDNSHelper::setDNS(uint64_t nwid, const char* domain, const std::vector<InetAddress>& servers)
{
    auto hasConfig = hasDNSConfig(nwid);

    std::stringstream ss;
    for (auto it = servers.begin(); it != servers.end(); ++it) {
        char ipaddr[256] = { 0 };
        ss << it->toIpString(ipaddr);
        if ((it + 1) != servers.end()) {
            ss << ";";
        }
    }
    std::string serverValue = ss.str();

    if (hasConfig.first) {
        // update existing config
        HKEY dnsKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, hasConfig.second.c_str(), 0, KEY_READ | KEY_WRITE, &dnsKey) == ERROR_SUCCESS) {
            auto retCode = RegSetKeyValueA(dnsKey, NULL, "GenericDNSServers", REG_SZ, serverValue.data(), (DWORD)serverValue.length());
            if (retCode != ERROR_SUCCESS) {
                fprintf(stderr, "Error writing dns servers: %d\n", retCode);
            }
        }
    }
    else {
        // add new config
        const char* baseKey = "SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\DnsPolicyConfig";
        GUID guid;
        CoCreateGuid(&guid);
        wchar_t guidTmp[128] = { 0 };
        char guidStr[128] = { 0 };
        StringFromGUID2(guid, guidTmp, 128);
        wcstombs(guidStr, guidTmp, 128);
        char fullKey[MAX_KEY_LENGTH] = { 0 };
        sprintf(fullKey, "%s\\%s", baseKey, guidStr);
        HKEY dnsKey;
        RegCreateKeyA(HKEY_LOCAL_MACHINE, fullKey, &dnsKey);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, fullKey, 0, KEY_READ | KEY_WRITE, &dnsKey) == ERROR_SUCCESS) {
            char nwString[32] = { 0 };
            sprintf(nwString, "%.16llx", nwid);
            RegSetKeyValueA(dnsKey, NULL, "Comment", REG_SZ, nwString, strlen(nwString));

            DWORD configOpts = 8;
            RegSetKeyValueA(dnsKey, NULL, "ConfigOptions", REG_DWORD, &configOpts, sizeof(DWORD));
            RegSetKeyValueA(dnsKey, NULL, "DisplayName", REG_SZ, "", 0);
            RegSetKeyValueA(dnsKey, NULL, "GenericDNSServers", REG_SZ, serverValue.data(), serverValue.length());
            RegSetKeyValueA(dnsKey, NULL, "IPSECCARestriction", REG_SZ, "", 0);
            std::string d = "." + std::string(domain);
            RegSetKeyValueA(dnsKey, NULL, "Name", REG_MULTI_SZ, d.data(), d.length());
            DWORD version = 2;
            RegSetKeyValueA(dnsKey, NULL, "Version", REG_DWORD, &version, sizeof(DWORD));
        }
    }
}

void WinDNSHelper::removeDNS(uint64_t nwid)
{
    auto hasConfig = hasDNSConfig(nwid);
    if (hasConfig.first) {
        RegDelnode(HKEY_LOCAL_MACHINE, hasConfig.second.c_str());
    }
}

}   // namespace ZeroTier