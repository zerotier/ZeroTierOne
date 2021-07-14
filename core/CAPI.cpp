/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "CallContext.hpp"
#include "Certificate.hpp"
#include "Constants.hpp"
#include "ECC384.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Locator.hpp"
#include "Node.hpp"
#include "VL1.hpp"
#include "VL2.hpp"

extern "C" {

/********************************************************************************************************************/

// These macros make the idiom of passing buffers to outside code via the API work properly even
// if the first address of Buf does not overlap with its data field, since the C++ standard does
// not absolutely guarantee this.
#define ZT_PTRTOBUF(p) ((ZeroTier::Buf *)(((uintptr_t)(p)) - ((uintptr_t) & (((ZeroTier::Buf *)0)->unsafeData[0]))))
#define ZT_BUFTOPTR(b) ((void *)(&((b)->unsafeData[0])))

ZT_MAYBE_UNUSED void *ZT_getBuffer()
{
    // When external code requests a Buf, grab one from the pool (or freshly allocated)
    // and return it with its reference count left at zero. It's the responsibility of
    // external code to bring it back via freeBuffer() or one of the processX() calls.
    // When this occurs it's either sent back to the pool with Buf's delete operator or
    // wrapped in a SharedPtr<> to be passed into the core.
    try {
        return ZT_BUFTOPTR(new ZeroTier::Buf());
    }
    catch (...) {
        return nullptr;   // can only happen on out of memory condition
    }
}

ZT_MAYBE_UNUSED void ZT_freeBuffer(void *b)
{
    if (b)
        delete ZT_PTRTOBUF(b);
}

struct p_queryResultBase {
    void (*freeFunction)(const void *);
};

ZT_MAYBE_UNUSED void ZT_freeQueryResult(const void *qr)
{
    if ((qr) && (reinterpret_cast<const p_queryResultBase *>(qr)->freeFunction))
        reinterpret_cast<const p_queryResultBase *>(qr)->freeFunction(qr);
}

ZT_MAYBE_UNUSED void ZT_version(int *major, int *minor, int *revision, int *build)
{
    if (major)
        *major = ZEROTIER_VERSION_MAJOR;
    if (minor)
        *minor = ZEROTIER_VERSION_MINOR;
    if (revision)
        *revision = ZEROTIER_VERSION_REVISION;
    if (build)
        *build = ZEROTIER_VERSION_BUILD;
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_new(ZT_Node **node, int64_t clock, int64_t ticks, void *tptr, void *uptr, const struct ZT_Node_Callbacks *callbacks)
{
    *node = nullptr;
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        *node = reinterpret_cast<ZT_Node *>(new ZeroTier::Node(uptr, callbacks, cc));
        return ZT_RESULT_OK;
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (std::runtime_error &exc) {
        return ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED void ZT_Node_delete(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        reinterpret_cast<ZeroTier::Node *>(node)->shutdown(cc);
        delete (reinterpret_cast<ZeroTier::Node *>(node));
    }
    catch (...) {
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_processWirePacket(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, int64_t localSocket, const ZT_InetAddress *remoteAddress, const void *packetData, unsigned int packetLength, int isZtBuffer, volatile int64_t *)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? ZT_PTRTOBUF(packetData) : new ZeroTier::Buf(packetData, packetLength & ZT_BUF_MEM_MASK));
        reinterpret_cast<ZeroTier::Node *>(node)->context().vl1->onRemotePacket(cc, localSocket, *ZeroTier::asInetAddress(remoteAddress), buf, packetLength);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        // "OK" since invalid packets are simply dropped, but the system is still up.
        // We should never make it here, but if we did that would be the interpretation.
    }
    return ZT_RESULT_OK;
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, uint64_t nwid, uint64_t sourceMac, uint64_t destMac, unsigned int etherType, unsigned int vlanId, const void *frameData, unsigned int frameLength, int isZtBuffer, volatile int64_t *)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        const ZeroTier::Context &ctx = reinterpret_cast<ZeroTier::Node *>(node)->context();
        ZeroTier::SharedPtr<ZeroTier::Network> network(ctx.networks->get(nwid));
        if (likely(network)) {
            ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? ZT_PTRTOBUF(frameData) : new ZeroTier::Buf(frameData, frameLength & ZT_BUF_MEM_MASK));
            ctx.vl2->onLocalEthernet(cc, network, ZeroTier::MAC(sourceMac), ZeroTier::MAC(destMac), etherType, vlanId, buf, frameLength);
            return ZT_RESULT_OK;
        }
        else {
            return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
        }
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_processBackgroundTasks(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, volatile int64_t *nextBackgroundTaskDeadline)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->processBackgroundTasks(cc, nextBackgroundTaskDeadline);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_join(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, void *uptr, uint64_t nwid, const ZT_Fingerprint *controllerFingerprint)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->join(nwid, controllerFingerprint, uptr, cc);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_leave(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, void **uptr, uint64_t nwid)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->leave(nwid, uptr, cc);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_multicastSubscribe(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->multicastSubscribe(cc, nwid, multicastGroup, multicastAdi);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_multicastUnsubscribe(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->multicastUnsubscribe(cc, nwid, multicastGroup, multicastAdi);
    }
    catch (std::bad_alloc &exc) {
        return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED uint64_t ZT_Node_address(ZT_Node *node) { return reinterpret_cast<ZeroTier::Node *>(node)->context().identity.address().toInt(); }

ZT_MAYBE_UNUSED const ZT_Identity *ZT_Node_identity(ZT_Node *node) { return (const ZT_Identity *)(&(reinterpret_cast<ZeroTier::Node *>(node)->identity())); }

ZT_MAYBE_UNUSED void ZT_Node_status(ZT_Node *node, int64_t, int64_t, void *, ZT_NodeStatus *status)
{
    try {
        reinterpret_cast<ZeroTier::Node *>(node)->status(status);
    }
    catch (...) {
    }
}

ZT_MAYBE_UNUSED ZT_PeerList *ZT_Node_peers(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->peers(cc);
    }
    catch (...) {
        return (ZT_PeerList *)0;
    }
}

ZT_MAYBE_UNUSED ZT_VirtualNetworkConfig *ZT_Node_networkConfig(ZT_Node *node, int64_t, int64_t, void *, uint64_t nwid)
{
    try {
        return reinterpret_cast<ZeroTier::Node *>(node)->networkConfig(nwid);
    }
    catch (...) {
        return (ZT_VirtualNetworkConfig *)0;
    }
}

ZT_MAYBE_UNUSED ZT_VirtualNetworkList *ZT_Node_networks(ZT_Node *node)
{
    try {
        return reinterpret_cast<ZeroTier::Node *>(node)->networks();
    }
    catch (...) {
        return (ZT_VirtualNetworkList *)0;
    }
}

ZT_MAYBE_UNUSED void ZT_Node_setNetworkUserPtr(ZT_Node *node, uint64_t nwid, void *ptr)
{
    try {
        reinterpret_cast<ZeroTier::Node *>(node)->setNetworkUserPtr(nwid, ptr);
    }
    catch (...) {
    }
}

ZT_MAYBE_UNUSED void ZT_Node_setInterfaceAddresses(ZT_Node *node, int64_t, int64_t, void *, const ZT_InterfaceAddress *addrs, unsigned int addrCount)
{
    try {
        reinterpret_cast<ZeroTier::Node *>(node)->setInterfaceAddresses(addrs, addrCount);
    }
    catch (...) {
    }
}

ZT_MAYBE_UNUSED enum ZT_CertificateError ZT_Node_addCertificate(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, unsigned int localTrust, const ZT_Certificate *cert, const void *certData, unsigned int certSize)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->addCertificate(cc, localTrust, cert, certData, certSize);
    }
    catch (...) {
        return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
    }
}

ZT_MAYBE_UNUSED enum ZT_ResultCode ZT_Node_deleteCertificate(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, const void *serialNo)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->deleteCertificate(cc, serialNo);
    }
    catch (...) {
        return ZT_RESULT_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED ZT_CertificateList *ZT_Node_listCertificates(ZT_Node *node, int64_t, int64_t, void *)
{
    try {
        return reinterpret_cast<ZeroTier::Node *>(node)->listCertificates();
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED int ZT_Node_sendUserMessage(ZT_Node *node, int64_t clock, int64_t ticks, void *tptr, uint64_t dest, uint64_t typeId, const void *data, unsigned int len)
{
    try {
        ZeroTier::CallContext cc(clock, ticks, tptr);
        return reinterpret_cast<ZeroTier::Node *>(node)->sendUserMessage(cc, dest, typeId, data, len);
    }
    catch (...) {
        return 0;
    }
}

ZT_MAYBE_UNUSED void ZT_Node_setController(ZT_Node *node, void *networkControllerInstance)
{
    try {
        reinterpret_cast<ZeroTier::Node *>(node)->setController(networkControllerInstance);
    }
    catch (...) {
    }
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED ZT_Locator *ZT_Locator_create(int64_t rev, const ZT_Endpoint *endpoints, const ZT_EndpointAttributes *, unsigned int endpointCount, const ZT_Identity *signer)
{
    try {
        if ((!endpoints) || (endpointCount == 0) || (!signer))
            return nullptr;
        ZeroTier::Locator *loc = new ZeroTier::Locator();
        for (unsigned int i = 0; i < endpointCount; ++i)
            loc->add(reinterpret_cast<const ZeroTier::Endpoint *>(endpoints)[i], ZeroTier::Locator::EndpointAttributes::DEFAULT);
        if (!loc->sign(rev, *reinterpret_cast<const ZeroTier::Identity *>(signer))) {
            delete loc;
            return nullptr;
        }
        return reinterpret_cast<ZT_Locator *>(loc);
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED ZT_Locator *ZT_Locator_fromString(const char *str)
{
    try {
        if (!str)
            return nullptr;
        ZeroTier::Locator *loc = new ZeroTier::Locator();
        if (!loc->fromString(str)) {
            delete loc;
            return nullptr;
        }
        return reinterpret_cast<ZT_Locator *>(loc);
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED ZT_Locator *ZT_Locator_unmarshal(const void *data, unsigned int len)
{
    try {
        if ((!data) || (len == 0))
            return nullptr;
        ZeroTier::Locator *loc = new ZeroTier::Locator();
        if (loc->unmarshal(reinterpret_cast<const uint8_t *>(data), (int)len) <= 0) {
            delete loc;
            return nullptr;
        }
        return reinterpret_cast<ZT_Locator *>(loc);
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED int ZT_Locator_marshal(const ZT_Locator *loc, void *buf, unsigned int bufSize)
{
    if ((!loc) || (bufSize < ZT_LOCATOR_MARSHAL_SIZE_MAX))
        return -1;
    return reinterpret_cast<const ZeroTier::Locator *>(loc)->marshal(reinterpret_cast<uint8_t *>(buf), false);
}

ZT_MAYBE_UNUSED char *ZT_Locator_toString(const ZT_Locator *loc, char *buf, int capacity)
{
    if ((!loc) || (capacity < ZT_LOCATOR_STRING_SIZE_MAX))
        return nullptr;
    return reinterpret_cast<const ZeroTier::Locator *>(loc)->toString(buf);
}

ZT_MAYBE_UNUSED int64_t ZT_Locator_revision(const ZT_Locator *loc)
{
    if (!loc)
        return 0;
    return reinterpret_cast<const ZeroTier::Locator *>(loc)->revision();
}

ZT_MAYBE_UNUSED uint64_t ZT_Locator_signer(const ZT_Locator *loc)
{
    if (!loc)
        return 0;
    return reinterpret_cast<const ZeroTier::Locator *>(loc)->signer().toInt();
}

ZT_MAYBE_UNUSED int ZT_Locator_equals(const ZT_Locator *a, const ZT_Locator *b)
{
    if (a) {
        if (b) {
            if (*reinterpret_cast<const ZeroTier::Locator *>(a) == *reinterpret_cast<const ZeroTier::Locator *>(b))
                return 1;
        }
    }
    else if (!b) {
        return 1;
    }
    return 0;
}

ZT_MAYBE_UNUSED unsigned int ZT_Locator_endpointCount(const ZT_Locator *loc) { return (loc) ? (unsigned int)(reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints().size()) : 0; }

ZT_MAYBE_UNUSED const ZT_Endpoint *ZT_Locator_endpoint(const ZT_Locator *loc, const unsigned int ep)
{
    if (!loc)
        return nullptr;
    if (ep >= (unsigned int)(reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints().size()))
        return nullptr;
    return reinterpret_cast<const ZT_Endpoint *>(&(reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints()[ep]));
}

ZT_MAYBE_UNUSED int ZT_Locator_verify(const ZT_Locator *loc, const ZT_Identity *signer)
{
    if ((!loc) || (!signer))
        return 0;
    return reinterpret_cast<const ZeroTier::Locator *>(loc)->verify(*reinterpret_cast<const ZeroTier::Identity *>(signer)) ? 1 : 0;
}

ZT_MAYBE_UNUSED void ZT_Locator_delete(const ZT_Locator *loc)
{
    if (loc)
        delete reinterpret_cast<const ZeroTier::Locator *>(loc);
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED ZT_Identity *ZT_Identity_new(enum ZT_IdentityType type)
{
    if ((type != ZT_IDENTITY_TYPE_C25519) && (type != ZT_IDENTITY_TYPE_P384))
        return nullptr;
    try {
        ZeroTier::Identity *const id = new ZeroTier::Identity();
        id->generate((ZeroTier::Identity::Type)type);
        return reinterpret_cast<ZT_Identity *>(id);
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED ZT_Identity *ZT_Identity_clone(const ZT_Identity *id)
{
    if (id) {
        try {
            return reinterpret_cast<ZT_Identity *>(new ZeroTier::Identity(*reinterpret_cast<const ZeroTier::Identity *>(id)));
        }
        catch (...) {
            return nullptr;
        }
    }
    return nullptr;
}

ZT_MAYBE_UNUSED ZT_Identity *ZT_Identity_fromString(const char *idStr)
{
    if (!idStr)
        return nullptr;
    try {
        ZeroTier::Identity *const id = new ZeroTier::Identity();
        if (!id->fromString(idStr)) {
            delete id;
            return nullptr;
        }
        return reinterpret_cast<ZT_Identity *>(id);
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED int ZT_Identity_validate(const ZT_Identity *id)
{
    if (!id)
        return 0;
    return reinterpret_cast<const ZeroTier::Identity *>(id)->locallyValidate() ? 1 : 0;
}

ZT_MAYBE_UNUSED unsigned int ZT_Identity_sign(const ZT_Identity *id, const void *data, unsigned int len, void *signature, unsigned int signatureBufferLength)
{
    if (!id)
        return 0;
    if (signatureBufferLength < ZT_SIGNATURE_BUFFER_SIZE)
        return 0;
    return reinterpret_cast<const ZeroTier::Identity *>(id)->sign(data, len, signature, signatureBufferLength);
}

ZT_MAYBE_UNUSED int ZT_Identity_verify(const ZT_Identity *id, const void *data, unsigned int len, const void *signature, unsigned int sigLen)
{
    if ((!id) || (!signature) || (!sigLen))
        return 0;
    return reinterpret_cast<const ZeroTier::Identity *>(id)->verify(data, len, signature, sigLen) ? 1 : 0;
}

ZT_MAYBE_UNUSED enum ZT_IdentityType ZT_Identity_type(const ZT_Identity *id)
{
    if (!id)
        return (ZT_IdentityType)0;
    return (enum ZT_IdentityType) reinterpret_cast<const ZeroTier::Identity *>(id)->type();
}

ZT_MAYBE_UNUSED char *ZT_Identity_toString(const ZT_Identity *id, char *buf, int capacity, int includePrivate)
{
    if ((!id) || (!buf) || (capacity < ZT_IDENTITY_STRING_BUFFER_LENGTH))
        return nullptr;
    reinterpret_cast<const ZeroTier::Identity *>(id)->toString(includePrivate != 0, buf);
    return buf;
}

ZT_MAYBE_UNUSED int ZT_Identity_hasPrivate(const ZT_Identity *id)
{
    if (!id)
        return 0;
    return reinterpret_cast<const ZeroTier::Identity *>(id)->hasPrivate() ? 1 : 0;
}

ZT_MAYBE_UNUSED uint64_t ZT_Identity_address(const ZT_Identity *id)
{
    if (!id)
        return 0;
    return reinterpret_cast<const ZeroTier::Identity *>(id)->address();
}

ZT_MAYBE_UNUSED const ZT_Fingerprint *ZT_Identity_fingerprint(const ZT_Identity *id)
{
    if (!id)
        return nullptr;
    return &(reinterpret_cast<const ZeroTier::Identity *>(id)->fingerprint());
}

ZT_MAYBE_UNUSED int ZT_Identity_compare(const ZT_Identity *a, const ZT_Identity *b)
{
    if (a) {
        if (b) {
            if (*reinterpret_cast<const ZeroTier::Identity *>(a) < *reinterpret_cast<const ZeroTier::Identity *>(b)) {
                return -1;
            }
            else if (*reinterpret_cast<const ZeroTier::Identity *>(b) < *reinterpret_cast<const ZeroTier::Identity *>(a)) {
                return 1;
            }
            else {
                return 0;
            }
        }
        else {
            return 1;
        }
    }
    else if (b) {
        return -1;
    }
    else {
        return 0;
    }
}

ZT_MAYBE_UNUSED void ZT_Identity_delete(const ZT_Identity *id)
{
    if (id)
        delete reinterpret_cast<const ZeroTier::Identity *>(id);
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED int ZT_Certificate_newKeyPair(const enum ZT_CertificatePublicKeyAlgorithm type, uint8_t publicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE], int *const publicKeySize, uint8_t privateKey[ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE], int *const privateKeySize)
{
    try {
        return ZeroTier::Certificate::newKeyPair(type, publicKey, publicKeySize, privateKey, privateKeySize) ? ZT_RESULT_OK : ZT_RESULT_ERROR_BAD_PARAMETER;
    }
    catch (...) {
        return ZT_RESULT_FATAL_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED int ZT_Certificate_newCSR(const ZT_Certificate_Subject *subject, const void *const certificatePrivateKey, const int certificatePrivateKeySize, const void *const uniqueIdPrivateKey, const int uniqueIdPrivateKeySize, void *const csr, int *const csrSize)
{
    try {
        if ((!subject) || (!certificatePrivateKey) || (certificatePrivateKeySize <= 0))
            return ZT_RESULT_ERROR_BAD_PARAMETER;
        const ZeroTier::Vector<uint8_t> csrV(ZeroTier::Certificate::createCSR(*subject, certificatePrivateKey, (unsigned int)certificatePrivateKeySize, uniqueIdPrivateKey, (unsigned int)uniqueIdPrivateKeySize));
        if (csrV.empty() || ((int)csrV.size() > *csrSize))
            return ZT_RESULT_ERROR_BAD_PARAMETER;
        ZeroTier::Utils::copy(csr, csrV.data(), (unsigned int)csrV.size());
        *csrSize = (int)csrV.size();
        return ZT_RESULT_OK;
    }
    catch (...) {
        return ZT_RESULT_FATAL_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED ZT_Certificate *ZT_Certificate_sign(const ZT_Certificate *cert, const uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE], const void *issuerPrivateKey, int issuerPrivateKeySize)
{
    try {
        ZeroTier::Certificate *const c = new ZeroTier::Certificate(*cert);
        if (c->sign(issuer, issuerPrivateKey, issuerPrivateKeySize)) {
            return c;
        }
        else {
            delete c;
        }
    }
    catch (...) {
    }
    return nullptr;
}

ZT_MAYBE_UNUSED enum ZT_CertificateError ZT_Certificate_decode(const ZT_Certificate **decodedCert, const void *cert, int certSize, int verify)
{
    try {
        if ((!decodedCert) || (!cert) || (certSize <= 0))
            return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
        *decodedCert                   = nullptr;
        ZeroTier::Certificate *const c = new ZeroTier::Certificate();
        if (!c->decode(cert, certSize)) {
            delete c;
            return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
        }
        if (verify) {
            const ZT_CertificateError err = c->verify(-1, true);
            if (err != ZT_CERTIFICATE_ERROR_NONE) {
                delete c;
                return err;
            }
        }
        *decodedCert = c;
        return ZT_CERTIFICATE_ERROR_NONE;
    }
    catch (...) {
        return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
    }
}

ZT_MAYBE_UNUSED int ZT_Certificate_encode(const ZT_Certificate *cert, void *encoded, int *encodedSize)
{
    try {
        if ((!cert) || (!encoded) || (!encodedSize))
            return ZT_RESULT_ERROR_BAD_PARAMETER;
        ZeroTier::Certificate c(*cert);
        ZeroTier::Vector<uint8_t> enc(c.encode());
        if ((int)enc.size() > *encodedSize)
            return ZT_RESULT_ERROR_BAD_PARAMETER;
        ZeroTier::Utils::copy(encoded, enc.data(), (unsigned int)enc.size());
        *encodedSize = (int)enc.size();
        return ZT_RESULT_OK;
    }
    catch (...) {
        return ZT_RESULT_FATAL_ERROR_INTERNAL;
    }
}

ZT_MAYBE_UNUSED enum ZT_CertificateError ZT_Certificate_verify(const ZT_Certificate *cert, int64_t clock)
{
    try {
        if (!cert)
            return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
        return ZeroTier::Certificate(*cert).verify(clock, true);
    }
    catch (...) {
        return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
    }
}

ZT_MAYBE_UNUSED const ZT_Certificate *ZT_Certificate_clone(const ZT_Certificate *cert)
{
    try {
        if (!cert)
            return nullptr;
        return (const ZT_Certificate *)(new ZeroTier::Certificate(*cert));
    }
    catch (...) {
        return nullptr;
    }
}

ZT_MAYBE_UNUSED void ZT_Certificate_delete(const ZT_Certificate *cert)
{
    try {
        if (cert)
            delete (const ZeroTier::Certificate *)(cert);
    }
    catch (...) {
    }
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED char *ZT_Endpoint_toString(const ZT_Endpoint *ep, char *buf, int capacity)
{
    if ((!ep) || (!buf) || (capacity < ZT_ENDPOINT_STRING_SIZE_MAX))
        return nullptr;
    return reinterpret_cast<const ZeroTier::Endpoint *>(ep)->toString(buf);
}

ZT_MAYBE_UNUSED int ZT_Endpoint_fromString(ZT_Endpoint *ep, const char *str)
{
    if ((!ep) || (!str))
        return ZT_RESULT_ERROR_BAD_PARAMETER;
    return reinterpret_cast<ZeroTier::Endpoint *>(ep)->fromString(str) ? ZT_RESULT_OK : ZT_RESULT_ERROR_BAD_PARAMETER;
}

ZT_MAYBE_UNUSED int ZT_Endpoint_fromBytes(ZT_Endpoint *ep, const void *bytes, unsigned int len)
{
    if ((!ep) || (!bytes) || (!len))
        return ZT_RESULT_ERROR_BAD_PARAMETER;
    return (reinterpret_cast<ZeroTier::Endpoint *>(ep)->unmarshal(reinterpret_cast<const uint8_t *>(bytes), (int)len) > 0) ? 0 : ZT_RESULT_ERROR_BAD_PARAMETER;
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED char *ZT_Fingerprint_toString(const ZT_Fingerprint *fp, char *buf, int capacity)
{
    if (capacity < ZT_FINGERPRINT_STRING_SIZE_MAX)
        return nullptr;
    return reinterpret_cast<const ZeroTier::Fingerprint *>(fp)->toString(buf);
}

ZT_MAYBE_UNUSED int ZT_Fingerprint_fromString(ZT_Fingerprint *fp, const char *s)
{
    if ((!fp) || (!s))
        return 0;
    ZeroTier::Fingerprint f;
    if (f.fromString(s)) {
        *fp = f;
        return 1;
    }
    return 0;
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED void ZT_InetAddress_clear(ZT_InetAddress *ia)
{
    if (likely(ia != nullptr))
        ZeroTier::Utils::zero<sizeof(ZT_InetAddress)>(ia);
}

ZT_MAYBE_UNUSED char *ZT_InetAddress_toString(const ZT_InetAddress *ia, char *buf, unsigned int cap)
{
    if (likely((cap > 0) && (buf != nullptr))) {
        if (likely((ia != nullptr) && (cap >= ZT_INETADDRESS_STRING_SIZE_MAX))) {
            reinterpret_cast<const ZeroTier::InetAddress *>(ia)->toString(buf);
        }
        else {
            buf[0] = 0;
        }
    }
    return buf;
}

ZT_MAYBE_UNUSED int ZT_InetAddress_fromString(ZT_InetAddress *ia, const char *str)
{
    if (likely((ia != nullptr) && (str != nullptr))) {
        return (int)reinterpret_cast<ZeroTier::InetAddress *>(ia)->fromString(str);
    }
    return 0;
}

ZT_MAYBE_UNUSED void ZT_InetAddress_set(ZT_InetAddress *ia, const void *saddr)
{
    if (likely(ia != nullptr))
        (*reinterpret_cast<ZeroTier::InetAddress *>(ia)) = reinterpret_cast<const struct sockaddr *>(saddr);
}

ZT_MAYBE_UNUSED void ZT_InetAddress_setIpBytes(ZT_InetAddress *ia, const void *ipBytes, unsigned int ipLen, unsigned int port)
{
    if (likely(ia != nullptr))
        reinterpret_cast<ZeroTier::InetAddress *>(ia)->set(ipBytes, ipLen, port);
}

ZT_MAYBE_UNUSED void ZT_InetAddress_setPort(ZT_InetAddress *ia, unsigned int port)
{
    if (likely(ia != nullptr))
        reinterpret_cast<ZeroTier::InetAddress *>(ia)->setPort(port);
}

ZT_MAYBE_UNUSED unsigned int ZT_InetAddress_port(const ZT_InetAddress *ia)
{
    if (likely(ia != nullptr))
        return reinterpret_cast<const ZeroTier::InetAddress *>(ia)->port();
    return 0;
}

ZT_MAYBE_UNUSED int ZT_InetAddress_isNil(const ZT_InetAddress *ia)
{
    if (!ia)
        return 0;
    return (int)((bool)(*reinterpret_cast<const ZeroTier::InetAddress *>(ia)));
}

ZT_MAYBE_UNUSED int ZT_InetAddress_isV4(const ZT_InetAddress *ia)
{
    if (!ia)
        return 0;
    return (int)(reinterpret_cast<const ZeroTier::InetAddress *>(ia))->isV4();
}

ZT_MAYBE_UNUSED int ZT_InetAddress_isV6(const ZT_InetAddress *ia)
{
    if (!ia)
        return 0;
    return (int)(reinterpret_cast<const ZeroTier::InetAddress *>(ia))->isV6();
}

ZT_MAYBE_UNUSED unsigned int ZT_InetAddress_ipBytes(const ZT_InetAddress *ia, void *buf)
{
    if (ia) {
        switch (reinterpret_cast<const ZeroTier::InetAddress *>(ia)->as.sa.sa_family) {
            case AF_INET: ZeroTier::Utils::copy<4>(buf, &(reinterpret_cast<const ZeroTier::InetAddress *>(ia)->as.sa_in.sin_addr.s_addr)); return 4;
            case AF_INET6: ZeroTier::Utils::copy<16>(buf, reinterpret_cast<const ZeroTier::InetAddress *>(ia)->as.sa_in6.sin6_addr.s6_addr); return 16;
        }
    }
    return 0;
}

ZT_MAYBE_UNUSED enum ZT_InetAddress_IpScope ZT_InetAddress_ipScope(const ZT_InetAddress *ia)
{
    if (likely(ia != nullptr))
        return reinterpret_cast<const ZeroTier::InetAddress *>(ia)->ipScope();
    return ZT_IP_SCOPE_NONE;
}

ZT_MAYBE_UNUSED int ZT_InetAddress_compare(const ZT_InetAddress *a, const ZT_InetAddress *b)
{
    if (a) {
        if (b) {
            if (*reinterpret_cast<const ZeroTier::InetAddress *>(a) < *reinterpret_cast<const ZeroTier::InetAddress *>(b)) {
                return -1;
            }
            else if (*reinterpret_cast<const ZeroTier::InetAddress *>(b) < *reinterpret_cast<const ZeroTier::InetAddress *>(a)) {
                return 1;
            }
            else {
                return 0;
            }
        }
        else {
            return 1;
        }
    }
    else if (b) {
        return -1;
    }
    else {
        return 0;
    }
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED int ZT_Dictionary_parse(const void *const dict, const unsigned int len, void *const arg, void (*f)(void *, const char *, unsigned int, const void *, unsigned int))
{
    ZeroTier::Dictionary d;
    if (d.decode(dict, len)) {
        for (ZeroTier::Dictionary::const_iterator i(d.begin()); i != d.end(); ++i) {
            f(arg, i->first.c_str(), (unsigned int)i->first.length(), i->second.data(), (unsigned int)i->second.size());
        }
        return 1;
    }
    return 0;
}

/********************************************************************************************************************/

ZT_MAYBE_UNUSED uint64_t ZT_random() { return ZeroTier::Utils::random(); }

}   // extern "C"
