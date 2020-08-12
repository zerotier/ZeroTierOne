#include "MacDNSHelper.hpp"

#include <stdio.h>

#include <SystemConfiguration/SystemConfiguration.h>

namespace ZeroTier {

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

    CFStringRef keys[2];
    keys[0] = CFSTR("SupplementalMatchDomains");
    keys[1] = CFSTR("ServerAddresses");

    CFStringRef cfdomain = CFStringCreateWithCString(NULL, domain, kCFStringEncodingUTF8);
    CFArrayRef domainArray = CFArrayCreate(NULL, (const void**)&cfdomain, 1, &kCFTypeArrayCallBacks);

    CFTypeRef values[2];
    values[0] = domainArray;
    values[1] = serverArray;

    CFDictionaryRef dict = CFDictionaryCreate(NULL,
        (const void**)keys, (const void**)values, 2, &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    char buf[256] = {0};
    sprintf(buf, "State:/Network/Service/%.16llx/DNS", nwid);
    CFStringRef key = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    CFArrayRef list = SCDynamicStoreCopyKeyList(ds, key);

    CFIndex i = 0, j = CFArrayGetCount(list);
    bool ret = TRUE;
    if (j <= 0) {
        ret &= SCDynamicStoreAddValue(ds, key, dict);
    } else {
        ret &= SCDynamicStoreSetValue(ds, (CFStringRef)CFArrayGetValueAtIndex(list, i), dict);
    }
    if (!ret) {
        fprintf(stderr, "Error writing DNS configuration\n");
    }

    CFRelease(list);
    CFRelease(key);
    CFRelease(dict);
    CFRelease(domainArray);
    CFRelease(cfdomain);
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

}