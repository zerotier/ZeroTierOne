/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#include "node/Constants.hpp"
#include "node/RuntimeEnvironment.hpp"
#include "node/InetAddress.hpp"
#include "node/Utils.hpp"
#include "node/Identity.hpp"
#include "node/Packet.hpp"
#include "node/Salsa20.hpp"
#include "node/MAC.hpp"
#include "node/Peer.hpp"
#include "node/NodeConfig.hpp"
#include "node/Dictionary.hpp"
#include "node/EthernetTap.hpp"
#include "node/SHA512.hpp"
#include "node/C25519.hpp"
#include "node/Poly1305.hpp"
#include "node/CertificateOfMembership.hpp"
#include "node/HttpClient.hpp"
#include "node/Defaults.hpp"
#include "node/Node.hpp"

#ifdef __WINDOWS__
#include <tchar.h>
#endif

using namespace ZeroTier;

#include "selftest-crypto-vectors.hpp"

static unsigned char fuzzbuf[1048576];

static volatile bool webDone = false;
static std::string webSha512ShouldBe;
static void testHttpHandler(void *arg,int code,const std::string &url,const std::string &body)
{
	unsigned char sha[64];
	if (code == 200) {
		SHA512::hash(sha,body.data(),(unsigned int)body.length());
		if (webSha512ShouldBe == Utils::hex(sha,64))
			std::cout << "got " << body.length() << " bytes, response code " << code << ", SHA-512 OK" << std::endl;
		else std::cout << "got " << body.length() << " bytes, response code " << code << ", SHA-512 FAILED!" << std::endl;
	} else std::cout << "ERROR " << code << ": " << body << std::endl;
	webDone = true;
}

static int testHttp()
{
	HttpClient http;

	webSha512ShouldBe = "221b348c8278ad2063c158fb15927c35dc6bb42880daf130d0574025f88ec350811c34fae38a014b576d3ef5c98af32bb540e68204810db87a51fa9b239ea567";
	std::cout << "[http] fetching http://download.zerotier.com/dev/1k ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/1k",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "342e1a058332aad2d7a5412c1d9cd4ad02b4038178ca0c3ed9d34e3cf0905c118b684e5d2a935a158195d453d7d69e9c6e201e252620fb53f29611794a5d4b0c";
	std::cout << "[http] fetching http://download.zerotier.com/dev/2k ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/2k",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "439562e1471dd6bdb558cb680f38dd7742e521497e280cb1456a31f74b9216b7d98145b3896c2f68008e6ac0c1662a4cb70562caeac294c5d01f378b22a21292";
	std::cout << "[http] fetching http://download.zerotier.com/dev/4k ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/4k",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "fbd3901a9956158b9d290efa1af4fff459d8c03187c98b0e630d10a19fab61940e668652257763973f6cde34f2aa81574f9a50b1979b675b45ddd18d69a4ceb8";
	std::cout << "[http] fetching http://download.zerotier.com/dev/8k ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/8k",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "098ae593f8c3a962f385f9f008ec2116ad22eea8bc569fc88a06a0193480fdfb27470345c427116d19179fb2a74df21d95fe5f1df575a9f2d10d99595708b765";
	std::cout << "[http] fetching http://download.zerotier.com/dev/4m ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/4m",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "";
	std::cout << "[http] fetching http://download.zerotier.com/dev/NOEXIST ... "; std::cout.flush();
	webDone = false;
	http.GET("http://download.zerotier.com/dev/NOEXIST",HttpClient::NO_HEADERS,30,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	webSha512ShouldBe = "";
	std::cout << "[http] fetching http://1.1.1.1/SHOULD_TIME_OUT ... "; std::cout.flush();
	webDone = false;
	http.GET("http://1.1.1.1/SHOULD_TIME_OUT",HttpClient::NO_HEADERS,4,&testHttpHandler,(void *)0);
	while (!webDone) Thread::sleep(500);

	return 0;
}

static int testCrypto()
{
	unsigned char buf1[16384];
	unsigned char buf2[sizeof(buf1)],buf3[sizeof(buf1)];

	for(int i=0;i<3;++i) {
		Utils::getSecureRandom(buf1,64);
		std::cout << "[crypto] getSecureRandom: " << Utils::hex(buf1,64) << std::endl;
	}

	std::cout << "[crypto] Testing Salsa20... "; std::cout.flush();
	for(unsigned int i=0;i<4;++i) {
		for(unsigned int k=0;k<sizeof(buf1);++k)
			buf1[k] = (unsigned char)rand();
		memset(buf2,0,sizeof(buf2));
		memset(buf3,0,sizeof(buf3));
		Salsa20 s20;
		s20.init("12345678123456781234567812345678",256,"12345678",20);
		s20.encrypt(buf1,buf2,sizeof(buf1));
		s20.init("12345678123456781234567812345678",256,"12345678",20);
		s20.decrypt(buf2,buf3,sizeof(buf2));
		if (memcmp(buf1,buf3,sizeof(buf1))) {
			std::cout << "FAIL (encrypt/decrypt test)" << std::endl;
			return -1;
		}
	}
	Salsa20 s20(s20TV0Key,256,s20TV0Iv,20);
	memset(buf1,0,sizeof(buf1));
	memset(buf2,0,sizeof(buf2));
	s20.encrypt(buf1,buf2,64);
	if (memcmp(buf2,s20TV0Ks,64)) {
		std::cout << "FAIL (test vector 0)" << std::endl;
		return -1;
	}
	s20.init(s2012TV0Key,256,s2012TV0Iv,12);
	memset(buf1,0,sizeof(buf1));
	memset(buf2,0,sizeof(buf2));
	s20.encrypt(buf1,buf2,64);
	if (memcmp(buf2,s2012TV0Ks,64)) {
		std::cout << "FAIL (test vector 1)" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Benchmarking Salsa20/12... "; std::cout.flush();
	{
		unsigned char *bb = (unsigned char *)::malloc(1234567);
		for(unsigned int i=0;i<1234567;++i)
			bb[i] = (unsigned char)i;
		Salsa20 s20(s20TV0Key,256,s20TV0Iv,12);
		double bytes = 0.0;
		uint64_t start = Utils::now();
		for(unsigned int i=0;i<200;++i) {
			s20.encrypt(bb,bb,1234567);
			bytes += 1234567.0;
		}
		uint64_t end = Utils::now();
		SHA512::hash(buf1,bb,1234567);
		std::cout << ((bytes / 1048576.0) / ((double)(end - start) / 1000.0)) << " MiB/second (" << Utils::hex(buf1,16) << ')' << std::endl;
		::free((void *)bb);
	}

	std::cout << "[crypto] Testing SHA-512... "; std::cout.flush();
	SHA512::hash(buf1,sha512TV0Input,(unsigned int)strlen(sha512TV0Input));
	if (memcmp(buf1,sha512TV0Digest,64)) {
		std::cout << "FAIL" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Testing Poly1305... "; std::cout.flush();
	Poly1305::compute(buf1,poly1305TV0Input,sizeof(poly1305TV0Input),poly1305TV0Key);
	if (memcmp(buf1,poly1305TV0Tag,16)) {
		std::cout << "FAIL (1)" << std::endl;
		return -1;
	}
	Poly1305::compute(buf1,poly1305TV1Input,sizeof(poly1305TV1Input),poly1305TV1Key);
	if (memcmp(buf1,poly1305TV1Tag,16)) {
		std::cout << "FAIL (2)" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Testing C25519 and Ed25519 against test vectors... "; std::cout.flush();
	for(int k=0;k<ZT_NUM_C25519_TEST_VECTORS;++k) {
		C25519::Pair p1,p2;
		memcpy(p1.pub.data,C25519_TEST_VECTORS[k].pub1,p1.pub.size());
		memcpy(p1.priv.data,C25519_TEST_VECTORS[k].priv1,p1.priv.size());
		memcpy(p2.pub.data,C25519_TEST_VECTORS[k].pub2,p2.pub.size());
		memcpy(p2.priv.data,C25519_TEST_VECTORS[k].priv2,p2.priv.size());
		C25519::agree(p1,p2.pub,buf1,64);
		C25519::agree(p2,p1.pub,buf2,64);
		if (memcmp(buf1,buf2,64)) {
			std::cout << "FAIL (1)" << std::endl;
			return -1;
		}
		if (memcmp(buf1,C25519_TEST_VECTORS[k].agreement,64)) {
			std::cout << "FAIL (2)" << std::endl;
			return -1;
		}
		C25519::Signature sig1 = C25519::sign(p1,buf1,64);
		if (memcmp(sig1.data,C25519_TEST_VECTORS[k].agreementSignedBy1,64)) {
			std::cout << "FAIL (3)" << std::endl;
			return -1;
		}
		C25519::Signature sig2 = C25519::sign(p2,buf1,64);
		if (memcmp(sig2.data,C25519_TEST_VECTORS[k].agreementSignedBy2,64)) {
			std::cout << "FAIL (4)" << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Testing C25519 ECC key agreement... "; std::cout.flush();
	for(unsigned int i=0;i<100;++i) {
		memset(buf1,64,sizeof(buf1));
		memset(buf2,64,sizeof(buf2));
		memset(buf3,64,sizeof(buf3));
		C25519::Pair p1 = C25519::generate();
		C25519::Pair p2 = C25519::generate();
		C25519::Pair p3 = C25519::generate();
		C25519::agree(p1,p2.pub,buf1,64);
		C25519::agree(p2,p1.pub,buf2,64);
		C25519::agree(p3,p1.pub,buf3,64);
		// p1<>p2 should equal p1<>p2
		if (memcmp(buf1,buf2,64)) {
			std::cout << "FAIL (1)" << std::endl;
			return -1;
		}
		// p2<>p1 should not equal p3<>p1
		if (!memcmp(buf2,buf3,64)) {
			std::cout << "FAIL (2)" << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Testing Ed25519 ECC signatures... "; std::cout.flush();
	C25519::Pair didntSign = C25519::generate();
	for(unsigned int i=0;i<10;++i) {
		C25519::Pair p1 = C25519::generate();
		for(unsigned int k=0;k<sizeof(buf1);++k)
			buf1[k] = (unsigned char)rand();
		C25519::Signature sig = C25519::sign(p1,buf1,sizeof(buf1));
		if (!C25519::verify(p1.pub,buf1,sizeof(buf1),sig)) {
			std::cout << "FAIL (1)" << std::endl;
			return -1;
		}
		++buf1[17];
		if (C25519::verify(p1.pub,buf1,sizeof(buf1),sig)) {
			std::cout << "FAIL (2)" << std::endl;
			return -1;
		}
		--buf1[17];
		if (!C25519::verify(p1.pub,buf1,sizeof(buf1),sig)) {
			std::cout << "FAIL (3)" << std::endl;
			return -1;
		}
		if (C25519::verify(didntSign.pub,buf1,sizeof(buf1),sig)) {
			std::cout << "FAIL (2)" << std::endl;
			return -1;
		}
		for(unsigned int k=0;k<64;++k) {
			C25519::Signature sig2(sig);
			sig2.data[rand() % sig2.size()] ^= (unsigned char)(1 << (rand() & 7));
			if (C25519::verify(p1.pub,buf1,sizeof(buf1),sig2)) {
				std::cout << "FAIL (5)" << std::endl;
				return -1;
			}
		}
	}
	std::cout << "PASS" << std::endl;

	return 0;
}

static int testIdentity()
{
	Identity id;
	Buffer<512> buf;

	std::cout << "[identity] Validate known-good identity... "; std::cout.flush();
	if (!id.fromString(KNOWN_GOOD_IDENTITY)) {
		std::cout << "FAIL (1)" << std::endl;
		return -1;
	}
	if (!id.locallyValidate()) {
		std::cout << "FAIL (2)" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[identity] Validate known-bad identity... "; std::cout.flush();
	if (!id.fromString(KNOWN_BAD_IDENTITY)) {
		std::cout << "FAIL (1)" << std::endl;
		return -1;
	}
	if (id.locallyValidate()) {
		std::cout << "FAIL (2)" << std::endl;
		return -1;
	}
	std::cout << "PASS (i.e. it failed)" << std::endl;

	for(unsigned int k=0;k<4;++k) {
		std::cout << "[identity] Generate identity... "; std::cout.flush();
		uint64_t genstart = Utils::now();
		id.generate();
		uint64_t genend = Utils::now();
		std::cout << "(took " << (genend - genstart) << "ms): " << id.toString(true) << std::endl;
		std::cout << "[identity] Locally validate identity: ";
		if (id.locallyValidate()) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	{
		Identity id2;
		buf.clear();
		id.serialize(buf,true);
		id2.deserialize(buf);
		std::cout << "[identity] Serialize and deserialize (w/private): ";
		if ((id == id2)&&(id2.locallyValidate())) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	{
		Identity id2;
		buf.clear();
		id.serialize(buf,false);
		id2.deserialize(buf);
		std::cout << "[identity] Serialize and deserialize (no private): ";
		if ((id == id2)&&(id2.locallyValidate())) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	{
		Identity id2;
		id2.fromString(id.toString(true).c_str());
		std::cout << "[identity] Serialize and deserialize (ASCII w/private): ";
		if ((id == id2)&&(id2.locallyValidate())) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	{
		Identity id2;
		id2.fromString(id.toString(false).c_str());
		std::cout << "[identity] Serialize and deserialize (ASCII no private): ";
		if ((id == id2)&&(id2.locallyValidate())) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	return 0;
}

static int testCertificate()
{
	Identity authority;
	std::cout << "[certificate] Generating identity to act as authority... "; std::cout.flush();
	authority.generate();
	std::cout << authority.address().toString() << std::endl;

	Identity idA,idB;
	std::cout << "[certificate] Generating identities A and B... "; std::cout.flush();
	idA.generate();
	idB.generate();
	std::cout << idA.address().toString() << ", " << idB.address().toString() << std::endl;

	std::cout << "[certificate] Generating certificates A and B...";
	CertificateOfMembership cA(10000,100,1,idA.address());
	CertificateOfMembership cB(10099,100,1,idB.address());
	std::cout << std::endl;

	std::cout << "[certificate] Signing certificates A and B with authority...";
	cA.sign(authority);
	cB.sign(authority);
	std::cout << std::endl;

	//std::cout << "[certificate] A: " << cA.toString() << std::endl;
	//std::cout << "[certificate] B: " << cB.toString() << std::endl;

	std::cout << "[certificate] A agrees with B and B with A... ";
	if (cA.agreesWith(cB))
		std::cout << "yes, ";
	else {
		std::cout << "FAIL" << std::endl;
		return -1;
	}
	if (cB.agreesWith(cA))
		std::cout << "yes." << std::endl;
	else {
		std::cout << "FAIL" << std::endl;
		return -1;
	}

	std::cout << "[certificate] Testing string serialization... ";
	CertificateOfMembership copyA(cA.toString());
	CertificateOfMembership copyB(cB.toString());
	if (copyA != cA) {
		std::cout << "FAIL" << std::endl;
		return -1;
	}
	if (copyB != cB) {
		std::cout << "FAIL" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[certificate] Generating two certificates that should not agree...";
	cA = CertificateOfMembership(10000,100,1,idA.address());
	cB = CertificateOfMembership(10101,100,1,idB.address());
	std::cout << std::endl;

	std::cout << "[certificate] A agrees with B and B with A... ";
	if (!cA.agreesWith(cB))
		std::cout << "no, ";
	else {
		std::cout << "FAIL" << std::endl;
		return -1;
	}
	if (!cB.agreesWith(cA))
		std::cout << "no." << std::endl;
	else {
		std::cout << "FAIL" << std::endl;
		return -1;
	}

	return 0;
}

static int testPacket()
{
	unsigned char salsaKey[32],hmacKey[32];
	Packet a,b;

	a.burn();
	b.burn();

	for(unsigned int i=0;i<32;++i) {
		salsaKey[i] = (unsigned char)rand();
		hmacKey[i] = (unsigned char)rand();
	}

	std::cout << "[packet] Testing Packet encoder/decoder... ";

	a.reset(Address(),Address(),Packet::VERB_HELLO);
	for(int i=0;i<32;++i)
		a.append("supercalifragilisticexpealidocious",(unsigned int)strlen("supercalifragilisticexpealidocious"));

	b = a;
	if (a != b) {
		std::cout << "FAIL (assign)" << std::endl;
		return -1;
	}

	a.compress();
	unsigned int complen = a.size();
	a.uncompress();

	std::cout << "(compressed: " << complen << ", decompressed: " << a.size() << ") ";
	if (a != b) {
		std::cout << "FAIL (compresssion)" << std::endl;
		return -1;
	}

	a.armor(salsaKey,true);
	if (!a.dearmor(salsaKey)) {
		std::cout << "FAIL (encrypt-decrypt/verify)" << std::endl;
		return -1;
	}

	std::cout << "PASS" << std::endl;
	return 0;
}

static int testOther()
{
	std::cout << "[other] Testing hex encode/decode... "; std::cout.flush();
	for(unsigned int k=0;k<1000;++k) {
		unsigned int flen = (rand() % 8194) + 1;
		for(unsigned int i=0;i<flen;++i)
			fuzzbuf[i] = (unsigned char)(rand() & 0xff);
		std::string dec = Utils::unhex(Utils::hex(fuzzbuf,flen).c_str());
		if ((dec.length() != flen)||(memcmp(dec.data(),fuzzbuf,dec.length()))) {
			std::cout << "FAILED!" << std::endl;
			std::cout << Utils::hex(fuzzbuf,flen) << std::endl;
			std::cout << Utils::hex(dec.data(),(unsigned int)dec.length()) << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[other] Testing Dictionary... "; std::cout.flush();
	for(int k=0;k<1000;++k) {
		Dictionary a,b;
		int nk = rand() % 32;
		for(int q=0;q<nk;++q) {
			std::string k,v;
			int kl = (rand() % 512);
			int vl = (rand() % 512);
			for(int i=0;i<kl;++i)
				k.push_back((char)rand());
			for(int i=0;i<vl;++i)
				v.push_back((char)rand());
			a[k] = v;
		}
		std::string aser = a.toString();
		b.fromString(aser);
		if (a != b) {
			std::cout << "FAIL!" << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	return 0;
}

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
	int r = 0;

	// Code to generate the C25519 test vectors -- did this once and then
	// put these up top so that we can ensure that every platform produces
	// the same result.
	/*
	for(int k=0;k<32;++k) {
		C25519::Pair p1 = C25519::generate();
		C25519::Pair p2 = C25519::generate();
		unsigned char agg[64];
		C25519::agree(p1,p2.pub,agg,64);
		C25519::Signature sig1 = C25519::sign(p1,agg,64);
		C25519::Signature sig2 = C25519::sign(p2,agg,64);
		printf("{{");
		for(int i=0;i<64;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)p1.pub.data[i]);
		printf("},{");
		for(int i=0;i<64;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)p1.priv.data[i]);
		printf("},{");
		for(int i=0;i<64;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)p2.pub.data[i]);
		printf("},{");
		for(int i=0;i<64;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)p2.priv.data[i]);
		printf("},{");
		for(int i=0;i<64;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)agg[i]);
		printf("},{");
		for(int i=0;i<96;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)sig1.data[i]);
		printf("},{");
		for(int i=0;i<96;++i)
			printf("%s0x%.2x",((i > 0) ? "," : ""),(unsigned int)sig2.data[i]);
		printf("}}\n");
	}
	exit(0);
	*/

	std::cout << "[info] sizeof(void *) == " << sizeof(void *) << std::endl;
	std::cout << "[info] default home: " << ZT_DEFAULTS.defaultHomePath << std::endl;
	std::cout << "[info] system authtoken.secret: " << Node::NodeControlClient::authTokenDefaultSystemPath() << std::endl;
	std::cout << "[info] user authtoken.secret: " << Node::NodeControlClient::authTokenDefaultUserPath() << std::endl;

	srand((unsigned int)time(0));

	r |= testCrypto();
	r |= testHttp();
	r |= testPacket();
	r |= testOther();
	r |= testIdentity();
	r |= testCertificate();

	if (r)
		std::cout << std::endl << "SOMETHING FAILED!" << std::endl;

	return r;
}
