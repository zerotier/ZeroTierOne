/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "../osdep/WinDNSHelper.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ZeroTier;

static void check(bool condition, const char* message)
{
	if (! condition)
		throw std::runtime_error(message);
}

// Redirect HKLM only in this test process. Never create a live NRPT rule or
// require elevation. A unique HKCU tree contains every helper write.
class RegistrySandbox {
  public:
	RegistrySandbox() : _root(NULL), _redirected(false)
	{
		GUID guid;
		check(CoCreateGuid(&guid) == S_OK, "CoCreateGuid failed");
		wchar_t text[40];
		check(StringFromGUID2(guid, text, 40) != 0, "StringFromGUID2 failed");
		_path = L"Software\\ZeroTier-WinDNSHelperTest-" + std::wstring(text);
		DWORD disposition;
		check(RegCreateKeyExW(HKEY_CURRENT_USER, _path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &_root, &disposition) == ERROR_SUCCESS, "sandbox creation failed");
		if (disposition != REG_CREATED_NEW_KEY) {
			RegCloseKey(_root);
			throw std::runtime_error("sandbox already exists");
		}
		if (RegOverridePredefKey(HKEY_LOCAL_MACHINE, _root) != ERROR_SUCCESS) {
			RegCloseKey(_root);
			RegDeleteKeyW(HKEY_CURRENT_USER, _path.c_str());
			throw std::runtime_error("HKLM redirection failed");
		}
		_redirected = true;
	}

	~RegistrySandbox()
	{
		if (_redirected)
			RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL);
		RegCloseKey(_root);
		if (RegDeleteTreeW(HKEY_CURRENT_USER, _path.c_str()) != ERROR_SUCCESS) {
			std::fprintf(stderr, "Sandbox cleanup failed\n");
			std::abort();
		}
	}

	RegistrySandbox(const RegistrySandbox&) = delete;
	RegistrySandbox& operator=(const RegistrySandbox&) = delete;

  private:
	HKEY _root;
	bool _redirected;
	std::wstring _path;
};

static const char* policyPath = "SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\DnsPolicyConfig";

static void checkValue(HKEY key, const wchar_t* name, DWORD expectedType, const std::wstring& expected)
{
	DWORD type = 0;
	DWORD size = 0;
	check(RegQueryValueExW(key, name, NULL, &type, NULL, &size) == ERROR_SUCCESS, "value missing");
	check(type == expectedType, "wrong registry type");
	// RegQueryValueExW does not append missing terminators, unlike RegGetValue.
	if (size != expected.size() * sizeof(wchar_t))
		std::fprintf(stderr, "Raw registry size: %lu bytes; expected %zu bytes\n", size, expected.size() * sizeof(wchar_t));
	check(size == expected.size() * sizeof(wchar_t), "wrong raw UTF-16 byte count / missing terminators");
	std::vector<wchar_t> data(expected.size(), L'!');
	check(RegQueryValueExW(key, name, NULL, &type, reinterpret_cast<BYTE*>(data.data()), &size) == ERROR_SUCCESS, "raw query failed");
	check(std::wstring(data.data(), data.size()) == expected, "wrong raw UTF-16 data");
}

static std::wstring terminated(const std::wstring& value, unsigned int count = 1)
{
	return value + std::wstring(count, L'\0');
}

static HKEY onlyRule(HKEY base)
{
	char name[256];
	DWORD length = sizeof(name);
	check(RegEnumKeyExA(base, 0, name, &length, NULL, NULL, NULL, NULL) == ERROR_SUCCESS, "rule missing");
	HKEY rule;
	check(RegOpenKeyExA(base, name, 0, KEY_READ | KEY_WRITE, &rule) == ERROR_SUCCESS, "cannot open rule");
	length = sizeof(name);
	if (RegEnumKeyExA(base, 1, name, &length, NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS) {
		RegCloseKey(rule);
		throw std::runtime_error("update created a duplicate rule");
	}
	return rule;
}

int main(int argc, char** argv)
{
	try {
		RegistrySandbox sandbox;
		HKEY base;
		check(RegCreateKeyExA(HKEY_LOCAL_MACHINE, policyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &base, NULL) == ERROR_SUCCESS, "cannot create fake policy base");
		const uint64_t nwid = 0x0123456789abcdefULL;
		const std::vector<InetAddress> servers = { InetAddress("192.0.2.53/32"), InetAddress("2001:db8::53/128") };
		if (argc == 2 && std::string(argv[1]) == "--existing") {
			HKEY oldRule;
			check(RegCreateKeyExA(base, "existing-rule", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &oldRule, NULL) == ERROR_SUCCESS, "cannot seed old rule");
			const wchar_t comment[] = L"0123456789abcdef";
			const wchar_t name[] = L".KDG.local"; // Deliberately missing the list terminator.
			check(RegSetKeyValueW(oldRule, NULL, L"Comment", REG_SZ, comment, sizeof(comment)) == ERROR_SUCCESS, "cannot seed comment");
			check(RegSetKeyValueW(oldRule, NULL, L"Name", REG_MULTI_SZ, name, sizeof(name)) == ERROR_SUCCESS, "cannot seed old namespace");
			RegCloseKey(oldRule);
		}
		WinDNSHelper::setDNS(nwid, "KDG.local", servers);
		HKEY rule = onlyRule(base);
		checkValue(rule, L"Name", REG_MULTI_SZ, terminated(L".KDG.local", 2)); // Exactly 24 bytes.
		checkValue(rule, L"Comment", REG_SZ, terminated(L"0123456789abcdef"));
		if (argc == 1) {
			checkValue(rule, L"DisplayName", REG_SZ, terminated(L""));
			checkValue(rule, L"IPSECCARestriction", REG_SZ, terminated(L""));
		}
		char ipv6[256];
		const std::string serverText = std::string("192.0.2.53;") + servers[1].toIpString(ipv6);
		checkValue(rule, L"GenericDNSServers", REG_SZ, terminated(std::wstring(serverText.begin(), serverText.end())));

		// An older client's single-NUL value must be repaired on the existing-rule path.
		const wchar_t malformed[] = L".KDG.local";
		check(RegSetKeyValueW(rule, NULL, L"Name", REG_MULTI_SZ, malformed, sizeof(malformed)) == ERROR_SUCCESS, "cannot seed malformed namespace");
		WinDNSHelper::setDNS(nwid, "KDG.local", servers);
		checkValue(rule, L"Name", REG_MULTI_SZ, terminated(L".KDG.local", 2));

		// Internationalized DNS names in their ASCII-compatible (IDNA) form.
		WinDNSHelper::setDNS(nwid, "xn--bcher-kva.example", {});
		checkValue(rule, L"Name", REG_MULTI_SZ, terminated(L".xn--bcher-kva.example", 2));
		checkValue(rule, L"GenericDNSServers", REG_SZ, terminated(L""));
		const std::string longest(127, 'a'); // ZT_VirtualNetworkDNS::domain capacity minus NUL.
		WinDNSHelper::setDNS(nwid, longest.c_str(), servers);
		checkValue(rule, L"Name", REG_MULTI_SZ, terminated(L"." + std::wstring(127, L'a'), 2));

		// The A API uses the current ANSI code page, not UTF-16 byte counts.
		// Check a non-ASCII name if this code page can represent it losslessly.
		const wchar_t unicodeDomain[] = L"b\u00fccher.example";
		char ansiDomain[128];
		wchar_t roundtrip[128];
		if (WideCharToMultiByte(CP_ACP, 0, unicodeDomain, -1, ansiDomain, sizeof(ansiDomain), NULL, NULL) &&
			MultiByteToWideChar(CP_ACP, 0, ansiDomain, -1, roundtrip, 128) && std::wstring(roundtrip) == unicodeDomain) {
			WinDNSHelper::setDNS(nwid, ansiDomain, servers);
			checkValue(rule, L"Name", REG_MULTI_SZ, terminated(L"." + std::wstring(unicodeDomain), 2));
		}
		else {
			std::puts("SKIP: non-ASCII name cannot round-trip through this ANSI code page");
		}
		HKEY sameRule = onlyRule(base);
		RegCloseKey(sameRule);
		RegCloseKey(rule);
		RegCloseKey(base);
		std::printf("WinDNSHelper registry regression passed (ACP %u)\n", GetACP());
		return 0;
	}
	catch (const std::exception& error) {
		std::fprintf(stderr, "WinDNSHelper regression failed: %s\n", error.what());
		return 1;
	}
}
