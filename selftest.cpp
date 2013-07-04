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
#include <iostream>
#include <string>
#include <vector>

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
#include "node/Http.hpp"
#include "node/Condition.hpp"

using namespace ZeroTier;

static unsigned char fuzzbuf[1048576];

static const char *hmacShaTV0Key = "key";
static const char *hmacShaTV0Msg = "The quick brown fox jumps over the lazy dog";
static const unsigned char hmacShaTV0Mac[32] = { 0xf7,0xbc,0x83,0xf4,0x30,0x53,0x84,0x24,0xb1,0x32,0x98,0xe6,0xaa,0x6f,0xb1,0x43,0xef,0x4d,0x59,0xa1,0x49,0x46,0x17,0x59,0x97,0x47,0x9d,0xbc,0x2d,0x1a,0x3c,0xd8 };

static const unsigned char s20TV0Key[32] = { 0x0f,0x62,0xb5,0x08,0x5b,0xae,0x01,0x54,0xa7,0xfa,0x4d,0xa0,0xf3,0x46,0x99,0xec,0x3f,0x92,0xe5,0x38,0x8b,0xde,0x31,0x84,0xd7,0x2a,0x7d,0xd0,0x23,0x76,0xc9,0x1c };
static const unsigned char s20TV0Iv[8] = { 0x28,0x8f,0xf6,0x5d,0xc4,0x2b,0x92,0xf9 };
static const unsigned char s20TV0Ks[64] = { 0x5e,0x5e,0x71,0xf9,0x01,0x99,0x34,0x03,0x04,0xab,0xb2,0x2a,0x37,0xb6,0x62,0x5b,0xf8,0x83,0xfb,0x89,0xce,0x3b,0x21,0xf5,0x4a,0x10,0xb8,0x10,0x66,0xef,0x87,0xda,0x30,0xb7,0x76,0x99,0xaa,0x73,0x79,0xda,0x59,0x5c,0x77,0xdd,0x59,0x54,0x2d,0xa2,0x08,0xe5,0x95,0x4f,0x89,0xe4,0x0e,0xb7,0xaa,0x80,0xa8,0x4a,0x61,0x76,0x66,0x3f };

static int testCrypto()
{
	unsigned char buf1[16384];
	unsigned char buf2[sizeof(buf1)],buf3[sizeof(buf1)];

	std::cout << "[crypto] Testing ECDSA... "; std::cout.flush();
	for(unsigned int k=0;k<64;++k) {
		EllipticCurveKeyPair kp;
		kp.generate();
		for(int i=0;i<32;++i)
			buf1[i] = (unsigned char)rand();
		std::string sig = kp.sign(buf1);
		if (!EllipticCurveKeyPair::verify(buf1,kp.pub(),sig.data(),sig.length())) {
			std::cout << "FAIL" << std::endl;
			return -1;
		}
	}
	std::cout << "PASS" << std::endl;

	std::cout << "[crypto] Testing HMAC-SHA256... "; std::cout.flush();
	memset(buf1,0,sizeof(buf1));
	HMAC::sha256(hmacShaTV0Key,strlen(hmacShaTV0Key),hmacShaTV0Msg,strlen(hmacShaTV0Msg),buf1);
	if (memcmp(buf1,hmacShaTV0Mac,32)) {
		std::cout << "FAIL (test vector 0) (" << Utils::hex(buf1,32) << ")" << std::endl;
		return -1;
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

	for(unsigned int i=0;i<32;++i) {
		salsaKey[i] = (unsigned char)rand();
		hmacKey[i] = (unsigned char)rand();
	}

	std::cout << "[packet] Testing Packet encoder/decoder... ";

	a = Packet();
	a.setVerb(Packet::VERB_HELLO);
	for(int i=0;i<32;++i)
		a.append("supercalifragilisticexpealidocious",strlen("supercalifragilisticexpealidocious"));

	b = a;

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

	return 0;
}

static Condition testHttpDoneCondition;

static bool testHttpHandler(Http::Request *req,void *arg,const std::string &url,int code,const std::map<std::string,std::string> &headers,const std::string &body)
{
	if (code)
		std::cout << "[net] " << url << " " << code << " bytes: " << body.length() << std::endl;
	else std::cout << "[net] " << url << " FAILED: " << body << std::endl;
	testHttpDoneCondition.signal();
	return false;
}

static int testNet()
{
	std::cout << "[net] GET http://www.uc.edu/" << std::endl;
	new Http::Request(Http::HTTP_METHOD_GET,"http://www.uc.edu/",Http::EMPTY_HEADERS,std::string(),&testHttpHandler,(void *)0);
	testHttpDoneCondition.wait();
	std::cout << "[net] GET http://zerotier.com/" << std::endl;
	new Http::Request(Http::HTTP_METHOD_GET,"http://zerotier.com/",Http::EMPTY_HEADERS,std::string(),&testHttpHandler,(void *)0);
	testHttpDoneCondition.wait();
	std::cout << "[net] GET http://www.google.com/" << std::endl;
	new Http::Request(Http::HTTP_METHOD_GET,"http://www.google.com/",Http::EMPTY_HEADERS,std::string(),&testHttpHandler,(void *)0);
	testHttpDoneCondition.wait();
}

int main(int argc,char **argv)
{
	int r = 0;

	srand(time(0));

	r |= testNet();
	r |= testCrypto();
	r |= testPacket();
	r |= testOther();
	r |= testIdentity();

	if (r)
		std::cout << std::endl << "SOMETHING FAILED!" << std::endl;

	return r;
}
