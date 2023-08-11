#include "MacDNSHelper.hpp"

#include <stdio.h>

#include <SystemConfiguration/SystemConfiguration.h>

namespace ZeroTier {

static void printKeys (const void* key, const void* value, void* context) {
  CFShow(key);
  CFShow(value);
}

void MacDNSHelper::setDNS(uint64_t nwid, const char *domain, const std::vector<InetAddress> &servers)
{
    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);

    CFStringRef *s = new CFStringRef[4];
    for (unsigned int i = 0; i < servers.size(); ++i) {
        char buf[64];
        ZeroTier::InetAddress a = servers[i];
        const char *ipStr = a.toIpString(buf);
        s[i] = CFStringCreateWithCString(NULL, ipStr, kCFStringEncodingUTF8);
    }

    CFArrayRef serverArray = CFArrayCreate(NULL, (const void**)s, servers.size(), &kCFTypeArrayCallBacks);

    CFStringRef keys[3];
    keys[0] = CFSTR("SupplementalMatchDomains");
    keys[1] = CFSTR("ServerAddresses");
    keys[2] = CFSTR("SearchDomains");

    CFStringRef cfdomain = CFStringCreateWithCString(NULL, domain, kCFStringEncodingUTF8);
    CFStringRef cfdomain2 = CFStringCreateWithCString(NULL, domain, kCFStringEncodingUTF8);
    CFArrayRef domainArray = CFArrayCreate(NULL, (const void**)&cfdomain, 1, &kCFTypeArrayCallBacks);
    CFArrayRef domainArray2 = CFArrayCreate(NULL, (const void**)&cfdomain2, 1, &kCFTypeArrayCallBacks);

    CFTypeRef values[3];
    values[0] = domainArray;
    values[1] = serverArray;
    values[2] = domainArray2;

    CFDictionaryRef dict = CFDictionaryCreate(NULL,
        (const void**)keys, (const void**)values, 3, &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    char buf[256] = {0};
    sprintf(buf, "State:/Network/Service/%.16llx/DNS", nwid);
    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    CFArrayRef list = SCDynamicStoreCopyKeyList(ds, key);
    CFIndex i = 0, j = CFArrayGetCount(list);
    bool dnsServersChanged = true;
    CFPropertyListRef oldDNSServers = NULL;
    if (j > 0) {
        oldDNSServers = SCDynamicStoreCopyValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i));
        dnsServersChanged = !CFEqual(oldDNSServers,dict);
    }
    if (dnsServersChanged) {
        bool ret = TRUE;
        if (j <= 0) {
            ret &= SCDynamicStoreAddValue(ds, key, dict);
        } else {
            ret &= SCDynamicStoreSetValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i), dict);
        }
        if (!ret) {
            fprintf(stderr, "Error writing DNS configuration\n");
        }
    }
    if (oldDNSServers != NULL) {
        CFRelease(oldDNSServers);
    }
    CFRelease(list);
    CFRelease(key);
    CFRelease(dict);
    CFRelease(domainArray);
    CFRelease(domainArray2);
    CFRelease(cfdomain);
    CFRelease(cfdomain2);
    CFRelease(serverArray);
    for (int i = 0; i < servers.size(); ++i) {
        CFRelease(s[i]);
    }
    delete[] s;
    CFRelease(ds);
}

void MacDNSHelper::removeDNS(uint64_t nwid)
{
    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);

    char buf[256] = {0};
    sprintf(buf, "State:/Network/Service/%.16llx/DNS", nwid);
    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    SCDynamicStoreRemoveValue(ds, key);
    CFRelease(key);
    CFRelease(ds);
}

// Make macOS believe we do in fact have ipv6 connectivity and that it should resolve dns names
// over ipv6 if we ask for them.
// Originally I planned to put all the v6 ip addresses from the network into the config.
// But only the link local address is necessary and sufficient. Added other v6 addresses
// doesn't do anything.
//
// As of Monterey we need IPv4 set up too.

bool MacDNSHelper::addIps4(uint64_t nwid, const MAC mac, const char *dev, const std::vector<InetAddress>& addrs)
{
    const char* ipStr = {0};
    char buf2[256] = {0};

    bool hasV4 = false;
    for (unsigned int i = 0; i < addrs.size(); ++i) {
        if (addrs[i].isV4()) {
            hasV4 = true;

            ipStr = addrs[i].toIpString(buf2);

            break;
        }
    }

    if (!hasV4) {
        MacDNSHelper::removeIps4(nwid);
        return true;
    }


    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);
    char buf[256] = { 0 };
    sprintf(buf, "State:/Network/Service/%.16llx/IPv4", nwid);


    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);

    CFStringRef cfaddr = CFStringCreateWithCString(NULL, ipStr, kCFStringEncodingUTF8);
    CFArrayRef addrArray = CFArrayCreate(NULL, (const void**)&cfaddr, 1, &kCFTypeArrayCallBacks);

    CFStringRef cfdev = CFStringCreateWithCString(NULL, dev, kCFStringEncodingUTF8);

    CFStringRef cfserver = CFStringCreateWithCString(NULL, "127.0.0.1", kCFStringEncodingUTF8);
    // using the ip from the zerotier network breaks routing on the mac
    CFStringRef cfrouter = CFStringCreateWithCString(NULL, "127.0.0.1", kCFStringEncodingUTF8);

    const int SIZE = 4;
    CFStringRef keys[SIZE];
    keys[0] = CFSTR("Addresses");
    keys[1] = CFSTR("InterfaceName");
    keys[2] = CFSTR("ServerAddress");
    keys[3] = CFSTR("Router");

    CFTypeRef values[SIZE];
    values[0] = addrArray;
    values[1] = cfdev;
    values[2] = cfserver;
    values[3] = cfrouter;


    CFDictionaryRef dict = CFDictionaryCreate(NULL,
        (const void**)keys, (const void**)values, SIZE, &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    // CFDictionaryApplyFunction(dict, printKeys, NULL);

    CFArrayRef list = SCDynamicStoreCopyKeyList(ds, key);
    CFIndex i = 0, j = CFArrayGetCount(list);
    bool addrsChanged = true;
    CFPropertyListRef oldAddrs = NULL;

    bool ret = TRUE;
    if (j > 0) {
        oldAddrs = SCDynamicStoreCopyValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i));
        addrsChanged = !CFEqual(oldAddrs,dict);
    }
    if (addrsChanged) {
        if (j <= 0) {
            ret &= SCDynamicStoreAddValue(ds, key, dict);
        } else {
            ret &= SCDynamicStoreSetValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i), dict);
        }
        if (!ret) {
            fprintf(stderr, "Error writing IPv6 configuration\n");
        }
    }
    if (oldAddrs != NULL) {
        CFRelease(oldAddrs);
    }

    CFRelease(cfaddr);

    CFRelease(addrArray);
    CFRelease(cfdev);
    CFRelease(cfserver);
    CFRelease(cfrouter);

    CFRelease(ds);
    CFRelease(key);

    // for (unsigned int i = 0; i < SIZE; ++i) {
    //     values[i] = NULL;
    // }

    CFRelease(list);
    CFRelease(dict);

    return ret;

}
bool MacDNSHelper::addIps6(uint64_t nwid, const MAC mac, const char *dev, const std::vector<InetAddress>& addrs)
{
    bool hasV6 = false;
    for (unsigned int i = 0; i < addrs.size(); ++i) {
        if (addrs[i].isV6()) {
            hasV6 = true;
            break;
        }
    }

    if (!hasV6) {
        MacDNSHelper::removeIps6(nwid);
        return true;
    }


    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);
    char buf[256] = { 0 };
    sprintf(buf, "State:/Network/Service/%.16llx/IPv6", nwid);

    InetAddress ll = InetAddress::makeIpv6LinkLocal(mac);
    char buf2[256] = {0};
    const char* llStr = ll.toIpString(buf2);


    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);

    CFStringRef cfaddr = CFStringCreateWithCString(NULL, llStr, kCFStringEncodingUTF8);
    CFStringRef cfprefixes = CFStringCreateWithCString(NULL, "64", kCFStringEncodingUTF8);
    CFStringRef cfdestaddrs = CFStringCreateWithCString(NULL, "::ffff:ffff:ffff:ffff:0:0", kCFStringEncodingUTF8);
    CFStringRef cfflags = CFStringCreateWithCString(NULL, "0", kCFStringEncodingUTF8);

    CFArrayRef addrArray = CFArrayCreate(NULL, (const void**)&cfaddr, 1, &kCFTypeArrayCallBacks);
    CFArrayRef prefixArray = CFArrayCreate(NULL, (const void**)&cfprefixes, 1, &kCFTypeArrayCallBacks);
    CFArrayRef destArray = CFArrayCreate(NULL, (const void**)&cfdestaddrs, 1, &kCFTypeArrayCallBacks);
    CFArrayRef flagsArray = CFArrayCreate(NULL, (const void**)&cfflags, 1, &kCFTypeArrayCallBacks);
    CFStringRef cfdev = CFStringCreateWithCString(NULL, dev, kCFStringEncodingUTF8);

    const int SIZE = 5;
    CFStringRef keys[SIZE];
    keys[0] = CFSTR("Addresses");
    keys[1] = CFSTR("DestAddresses");
    keys[2] = CFSTR("Flags");
    keys[3] = CFSTR("InterfaceName");
    keys[4] = CFSTR("PrefixLength");

    CFTypeRef values[SIZE];
    values[0] = addrArray;
    values[1] = destArray;
    values[2] = flagsArray;
    // values[3] = devArray;
    values[3] = cfdev;
    values[4] = prefixArray;


    CFDictionaryRef dict = CFDictionaryCreate(NULL,
        (const void**)keys, (const void**)values, SIZE, &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    // CFDictionaryApplyFunction(dict, printKeys, NULL);

    CFArrayRef list = SCDynamicStoreCopyKeyList(ds, key);
    CFIndex i = 0, j = CFArrayGetCount(list);
    bool addrsChanged = true;
    CFPropertyListRef oldAddrs = NULL;

    bool ret = TRUE;
    if (j > 0) {
        oldAddrs = SCDynamicStoreCopyValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i));
        addrsChanged = !CFEqual(oldAddrs,dict);
    }
    if (addrsChanged) {
        if (j <= 0) {
            ret &= SCDynamicStoreAddValue(ds, key, dict);
        } else {
            ret &= SCDynamicStoreSetValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i), dict);
        }
        if (!ret) {
            fprintf(stderr, "Error writing IPv6 configuration\n");
        }
    }
    if (oldAddrs != NULL) {
        CFRelease(oldAddrs);
    }

    CFRelease(cfaddr);
    CFRelease(cfprefixes);
    CFRelease(cfdestaddrs);
    CFRelease(cfflags);

    CFRelease(addrArray);
    CFRelease(prefixArray);
    CFRelease(destArray);
    CFRelease(flagsArray);
    CFRelease(cfdev);

    CFRelease(ds);
    CFRelease(key);

    // for (unsigned int i = 0; i < SIZE; ++i) {
    //     values[i] = NULL;
    // }

    CFRelease(list);
    CFRelease(dict);

    return ret;
}
bool MacDNSHelper::removeIps6(uint64_t nwid)
{
    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);

    char buf[256] = {0};
    sprintf(buf, "State:/Network/Service/%.16llx/IPv6", nwid);
    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    bool res = SCDynamicStoreRemoveValue(ds, key);
    CFRelease(key);
    CFRelease(ds);

    return res;
}


bool MacDNSHelper::removeIps4(uint64_t nwid)
{
    SCDynamicStoreRef ds = SCDynamicStoreCreate(NULL, CFSTR("zerotier"), NULL, NULL);

    char buf[256] = {0};
    sprintf(buf, "State:/Network/Service/%.16llx/IPv4", nwid);
    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    bool res = SCDynamicStoreRemoveValue(ds, key);
    CFRelease(key);
    CFRelease(ds);

    return res;
}


}
