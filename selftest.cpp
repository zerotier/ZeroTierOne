/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
#include "node/EllipticCurveKey.hpp"
#include "node/EllipticCurveKeyPair.hpp"
#include "node/Utils.hpp"
#include "node/Identity.hpp"
#include "node/Packet.hpp"
#include "node/Salsa20.hpp"
#include "node/HMAC.hpp"
#include "node/MAC.hpp"
#include "node/Peer.hpp"
#include "node/Condition.hpp"
#include "node/NodeConfig.hpp"
#include "node/Dictionary.hpp"
#include "node/EthernetTap.hpp"
#include "node/SHA512.hpp"
#include "node/C25519.hpp"
#include "node/Poly1305.hpp"

#ifdef __WINDOWS__
#include <tchar.h>
#endif

using namespace ZeroTier;

#include "selftest-crypto-vectors.hpp"

static unsigned char fuzzbuf[1048576];

static int testCrypto()
{
	unsigned char buf1[16384];
	unsigned char buf2[sizeof(buf1)],buf3[sizeof(buf1)];

	std::cout << "[crypto] Testing SHA-512... "; std::cout.flush();
	SHA512::hash(buf1,sha512TV0Input,strlen(sha512TV0Input));
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
	for(unsigned int i=0;i<50;++i) {
		C25519::Pair p1 = C25519::generate();
		C25519::Pair p2 = C25519::generate();
		C25519::Pair p3 = C25519::generate();
		C25519::agree(p1,p2.pub,buf1,64);
		C25519::agree(p2,p1.pub,buf2,64);
		C25519::agree(p3,p1.pub,buf3,64);
		if (memcmp(buf1,buf2,64)) {
			std::cout << "FAIL (1)" << std::endl;
			return -1;
		}
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

	std::cout << "[crypto] Testing Salsa20... "; std::cout.flush();
	for(unsigned int i=0;i<4;++i) {
		for(unsigned int k=0;k<sizeof(buf1);++k)
			buf1[k] = (unsigned char)rand();
		memset(buf2,0,sizeof(buf2));
		memset(buf3,0,sizeof(buf3));
		Salsa20 s20;
		s20.init("12345678123456781234567812345678",256,"12345678");
		s20.encrypt(buf1,buf2,sizeof(buf1));
		s20.init("12345678123456781234567812345678",256,"12345678");
		s20.decrypt(buf2,buf3,sizeof(buf2));
		if (memcmp(buf1,buf3,sizeof(buf1))) {
			std::cout << "FAIL (encrypt/decrypt test)" << std::endl;
			return -1;
		}
	}
	Salsa20 s20(s20TV0Key,256,s20TV0Iv);
	memset(buf1,0,sizeof(buf1));
	memset(buf2,0,sizeof(buf2));
	s20.encrypt(buf1,buf2,64);
	if (memcmp(buf2,s20TV0Ks,64)) {
		std::cout << "FAIL (test vector 0)" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	return 0;
}

static int testIdentity()
{
	Identity id;
	Buffer<512> buf;

	std::cout << "[identity] Generate identity... "; std::cout.flush();
	uint64_t genstart = Utils::now();
	id.generate();
	uint64_t genend = Utils::now();
	std::cout << "(took " << (genend - genstart) << "ms): " << id.toString(true) << std::endl;
	std::cout << "[identity] Locally validate identity: ";
	if (id.locallyValidate(false)) {
		std::cout << "PASS" << std::endl;
	} else {
		std::cout << "FAIL" << std::endl;
		return -1;
	}

	{
		Identity id2;
		buf.clear();
		id.serialize(buf,true);
		id2.deserialize(buf);
		std::cout << "[identity] Serialize and deserialize (w/private): ";
		if ((id == id2)&&(id2.locallyValidate(false))) {
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
		if ((id == id2)&&(id2.locallyValidate(false))) {
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
		if ((id == id2)&&(id2.locallyValidate(false))) {
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
		if ((id == id2)&&(id2.locallyValidate(false))) {
			std::cout << "PASS" << std::endl;
		} else {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}

	return 0;
}

static int testPacket()
{
	unsigned char salsaKey[32],hmacKey[32];
	Packet a,b;

	a.zeroAll();
	b.zeroAll();

	for(unsigned int i=0;i<32;++i) {
		salsaKey[i] = (unsigned char)rand();
		hmacKey[i] = (unsigned char)rand();
	}

	std::cout << "[packet] Testing Packet encoder/decoder... ";

	a.reset(Address(),Address(),Packet::VERB_HELLO);
	for(int i=0;i<32;++i)
		a.append("supercalifragilisticexpealidocious",strlen("supercalifragilisticexpealidocious"));

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

	a.compress();
	a.encrypt(salsaKey);
	a.decrypt(salsaKey);
	a.uncompress();
	if (a != b) {
		std::cout << "FAIL (encrypt-decrypt)" << std::endl;
		return -1;
	}

	a.hmacSet(hmacKey);
	if (!a.hmacVerify(hmacKey)) {
		std::cout << "FAIL (hmacVerify)" << std::endl;
		return -1;
	}

	std::cout << "PASS" << std::endl;
	return 0;
}

static int testOther()
{
	std::cout << "[other] Testing Base64 encode/decode... "; std::cout.flush();
	for(unsigned int k=0;k<1000;++k) {
		unsigned int flen = (rand() % 8194) + 1;
		for(unsigned int i=0;i<flen;++i)
			fuzzbuf[i] = (unsigned char)(rand() & 0xff);
		std::string dec = Utils::base64Decode(Utils::base64Encode(fuzzbuf,flen));
		if ((dec.length() != flen)||(memcmp(dec.data(),fuzzbuf,dec.length()))) {
			std::cout << "FAILED!" << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[other] Testing hex encode/decode... "; std::cout.flush();
	for(unsigned int k=0;k<1000;++k) {
		unsigned int flen = (rand() % 8194) + 1;
		for(unsigned int i=0;i<flen;++i)
			fuzzbuf[i] = (unsigned char)(rand() & 0xff);
		std::string dec = Utils::unhex(Utils::hex(fuzzbuf,flen).c_str());
		if ((dec.length() != flen)||(memcmp(dec.data(),fuzzbuf,dec.length()))) {
			std::cout << "FAILED!" << std::endl;
			std::cout << Utils::hex(fuzzbuf,flen) << std::endl;
			std::cout << Utils::hex(dec.data(),dec.length()) << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[other] Testing command bus encode/decode... "; std::cout.flush();
	try {
		static char key[32] = { 0 };
		for(unsigned int k=0;k<1000;++k) {
			std::vector<std::string> original;
			for(unsigned int i=0,j=rand() % 256,l=(rand() % 1024)+1;i<j;++i)
				original.push_back(std::string(l,'x'));
			std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > packets(NodeConfig::encodeControlMessage(key,1,original));
			//std::cout << packets.size() << ' '; std::cout.flush();
			std::vector<std::string> after;
			for(std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> >::iterator i(packets.begin());i!=packets.end();++i) {
				unsigned long convId = 9999;
				if (!NodeConfig::decodeControlMessagePacket(key,i->data(),i->size(),convId,after)) {
					std::cout << "FAIL (decode)" << std::endl;
					return -1;
				}
				if (convId != 1) {
					std::cout << "FAIL (conversation ID)" << std::endl;
					return -1;
				}
			}
			if (after != original) {
				std::cout << "FAIL (compare)" << std::endl;
				return -1;
			}
		}
	} catch (std::exception &exc) {
		std::cout << "FAIL (" << exc.what() << ")" << std::endl;
		return -1;
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[other] Testing Dictionary... "; std::cout.flush();
	for(int k=0;k<10000;++k) {
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

	srand((unsigned int)time(0));

	r |= testCrypto();
	r |= testPacket();
	r |= testOther();
	r |= testIdentity();

	if (r)
		std::cout << std::endl << "SOMETHING FAILED!" << std::endl;

	return r;
}
