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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "Constants.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <dirent.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"
#include "Salsa20.hpp"

namespace ZeroTier {

const char Utils::HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

const uint64_t Utils::crc64Table[256] = {
	0x0000000000000000ULL,0x7ad870c830358979ULL,
	0xf5b0e190606b12f2ULL,0x8f689158505e9b8bULL,
	0xc038e5739841b68fULL,0xbae095bba8743ff6ULL,
	0x358804e3f82aa47dULL,0x4f50742bc81f2d04ULL,
	0xab28ecb46814fe75ULL,0xd1f09c7c5821770cULL,
	0x5e980d24087fec87ULL,0x24407dec384a65feULL,
	0x6b1009c7f05548faULL,0x11c8790fc060c183ULL,
	0x9ea0e857903e5a08ULL,0xe478989fa00bd371ULL,
	0x7d08ff3b88be6f81ULL,0x07d08ff3b88be6f8ULL,
	0x88b81eabe8d57d73ULL,0xf2606e63d8e0f40aULL,
	0xbd301a4810ffd90eULL,0xc7e86a8020ca5077ULL,
	0x4880fbd87094cbfcULL,0x32588b1040a14285ULL,
	0xd620138fe0aa91f4ULL,0xacf86347d09f188dULL,
	0x2390f21f80c18306ULL,0x594882d7b0f40a7fULL,
	0x1618f6fc78eb277bULL,0x6cc0863448deae02ULL,
	0xe3a8176c18803589ULL,0x997067a428b5bcf0ULL,
	0xfa11fe77117cdf02ULL,0x80c98ebf2149567bULL,
	0x0fa11fe77117cdf0ULL,0x75796f2f41224489ULL,
	0x3a291b04893d698dULL,0x40f16bccb908e0f4ULL,
	0xcf99fa94e9567b7fULL,0xb5418a5cd963f206ULL,
	0x513912c379682177ULL,0x2be1620b495da80eULL,
	0xa489f35319033385ULL,0xde51839b2936bafcULL,
	0x9101f7b0e12997f8ULL,0xebd98778d11c1e81ULL,
	0x64b116208142850aULL,0x1e6966e8b1770c73ULL,
	0x8719014c99c2b083ULL,0xfdc17184a9f739faULL,
	0x72a9e0dcf9a9a271ULL,0x08719014c99c2b08ULL,
	0x4721e43f0183060cULL,0x3df994f731b68f75ULL,
	0xb29105af61e814feULL,0xc849756751dd9d87ULL,
	0x2c31edf8f1d64ef6ULL,0x56e99d30c1e3c78fULL,
	0xd9810c6891bd5c04ULL,0xa3597ca0a188d57dULL,
	0xec09088b6997f879ULL,0x96d1784359a27100ULL,
	0x19b9e91b09fcea8bULL,0x636199d339c963f2ULL,
	0xdf7adabd7a6e2d6fULL,0xa5a2aa754a5ba416ULL,
	0x2aca3b2d1a053f9dULL,0x50124be52a30b6e4ULL,
	0x1f423fcee22f9be0ULL,0x659a4f06d21a1299ULL,
	0xeaf2de5e82448912ULL,0x902aae96b271006bULL,
	0x74523609127ad31aULL,0x0e8a46c1224f5a63ULL,
	0x81e2d7997211c1e8ULL,0xfb3aa75142244891ULL,
	0xb46ad37a8a3b6595ULL,0xceb2a3b2ba0eececULL,
	0x41da32eaea507767ULL,0x3b024222da65fe1eULL,
	0xa2722586f2d042eeULL,0xd8aa554ec2e5cb97ULL,
	0x57c2c41692bb501cULL,0x2d1ab4dea28ed965ULL,
	0x624ac0f56a91f461ULL,0x1892b03d5aa47d18ULL,
	0x97fa21650afae693ULL,0xed2251ad3acf6feaULL,
	0x095ac9329ac4bc9bULL,0x7382b9faaaf135e2ULL,
	0xfcea28a2faafae69ULL,0x8632586aca9a2710ULL,
	0xc9622c4102850a14ULL,0xb3ba5c8932b0836dULL,
	0x3cd2cdd162ee18e6ULL,0x460abd1952db919fULL,
	0x256b24ca6b12f26dULL,0x5fb354025b277b14ULL,
	0xd0dbc55a0b79e09fULL,0xaa03b5923b4c69e6ULL,
	0xe553c1b9f35344e2ULL,0x9f8bb171c366cd9bULL,
	0x10e3202993385610ULL,0x6a3b50e1a30ddf69ULL,
	0x8e43c87e03060c18ULL,0xf49bb8b633338561ULL,
	0x7bf329ee636d1eeaULL,0x012b592653589793ULL,
	0x4e7b2d0d9b47ba97ULL,0x34a35dc5ab7233eeULL,
	0xbbcbcc9dfb2ca865ULL,0xc113bc55cb19211cULL,
	0x5863dbf1e3ac9decULL,0x22bbab39d3991495ULL,
	0xadd33a6183c78f1eULL,0xd70b4aa9b3f20667ULL,
	0x985b3e827bed2b63ULL,0xe2834e4a4bd8a21aULL,
	0x6debdf121b863991ULL,0x1733afda2bb3b0e8ULL,
	0xf34b37458bb86399ULL,0x8993478dbb8deae0ULL,
	0x06fbd6d5ebd3716bULL,0x7c23a61ddbe6f812ULL,
	0x3373d23613f9d516ULL,0x49aba2fe23cc5c6fULL,
	0xc6c333a67392c7e4ULL,0xbc1b436e43a74e9dULL,
	0x95ac9329ac4bc9b5ULL,0xef74e3e19c7e40ccULL,
	0x601c72b9cc20db47ULL,0x1ac40271fc15523eULL,
	0x5594765a340a7f3aULL,0x2f4c0692043ff643ULL,
	0xa02497ca54616dc8ULL,0xdafce7026454e4b1ULL,
	0x3e847f9dc45f37c0ULL,0x445c0f55f46abeb9ULL,
	0xcb349e0da4342532ULL,0xb1eceec59401ac4bULL,
	0xfebc9aee5c1e814fULL,0x8464ea266c2b0836ULL,
	0x0b0c7b7e3c7593bdULL,0x71d40bb60c401ac4ULL,
	0xe8a46c1224f5a634ULL,0x927c1cda14c02f4dULL,
	0x1d148d82449eb4c6ULL,0x67ccfd4a74ab3dbfULL,
	0x289c8961bcb410bbULL,0x5244f9a98c8199c2ULL,
	0xdd2c68f1dcdf0249ULL,0xa7f41839ecea8b30ULL,
	0x438c80a64ce15841ULL,0x3954f06e7cd4d138ULL,
	0xb63c61362c8a4ab3ULL,0xcce411fe1cbfc3caULL,
	0x83b465d5d4a0eeceULL,0xf96c151de49567b7ULL,
	0x76048445b4cbfc3cULL,0x0cdcf48d84fe7545ULL,
	0x6fbd6d5ebd3716b7ULL,0x15651d968d029fceULL,
	0x9a0d8ccedd5c0445ULL,0xe0d5fc06ed698d3cULL,
	0xaf85882d2576a038ULL,0xd55df8e515432941ULL,
	0x5a3569bd451db2caULL,0x20ed197575283bb3ULL,
	0xc49581ead523e8c2ULL,0xbe4df122e51661bbULL,
	0x3125607ab548fa30ULL,0x4bfd10b2857d7349ULL,
	0x04ad64994d625e4dULL,0x7e7514517d57d734ULL,
	0xf11d85092d094cbfULL,0x8bc5f5c11d3cc5c6ULL,
	0x12b5926535897936ULL,0x686de2ad05bcf04fULL,
	0xe70573f555e26bc4ULL,0x9ddd033d65d7e2bdULL,
	0xd28d7716adc8cfb9ULL,0xa85507de9dfd46c0ULL,
	0x273d9686cda3dd4bULL,0x5de5e64efd965432ULL,
	0xb99d7ed15d9d8743ULL,0xc3450e196da80e3aULL,
	0x4c2d9f413df695b1ULL,0x36f5ef890dc31cc8ULL,
	0x79a59ba2c5dc31ccULL,0x037deb6af5e9b8b5ULL,
	0x8c157a32a5b7233eULL,0xf6cd0afa9582aa47ULL,
	0x4ad64994d625e4daULL,0x300e395ce6106da3ULL,
	0xbf66a804b64ef628ULL,0xc5bed8cc867b7f51ULL,
	0x8aeeace74e645255ULL,0xf036dc2f7e51db2cULL,
	0x7f5e4d772e0f40a7ULL,0x05863dbf1e3ac9deULL,
	0xe1fea520be311aafULL,0x9b26d5e88e0493d6ULL,
	0x144e44b0de5a085dULL,0x6e963478ee6f8124ULL,
	0x21c640532670ac20ULL,0x5b1e309b16452559ULL,
	0xd476a1c3461bbed2ULL,0xaeaed10b762e37abULL,
	0x37deb6af5e9b8b5bULL,0x4d06c6676eae0222ULL,
	0xc26e573f3ef099a9ULL,0xb8b627f70ec510d0ULL,
	0xf7e653dcc6da3dd4ULL,0x8d3e2314f6efb4adULL,
	0x0256b24ca6b12f26ULL,0x788ec2849684a65fULL,
	0x9cf65a1b368f752eULL,0xe62e2ad306bafc57ULL,
	0x6946bb8b56e467dcULL,0x139ecb4366d1eea5ULL,
	0x5ccebf68aecec3a1ULL,0x2616cfa09efb4ad8ULL,
	0xa97e5ef8cea5d153ULL,0xd3a62e30fe90582aULL,
	0xb0c7b7e3c7593bd8ULL,0xca1fc72bf76cb2a1ULL,
	0x45775673a732292aULL,0x3faf26bb9707a053ULL,
	0x70ff52905f188d57ULL,0x0a2722586f2d042eULL,
	0x854fb3003f739fa5ULL,0xff97c3c80f4616dcULL,
	0x1bef5b57af4dc5adULL,0x61372b9f9f784cd4ULL,
	0xee5fbac7cf26d75fULL,0x9487ca0fff135e26ULL,
	0xdbd7be24370c7322ULL,0xa10fceec0739fa5bULL,
	0x2e675fb4576761d0ULL,0x54bf2f7c6752e8a9ULL,
	0xcdcf48d84fe75459ULL,0xb71738107fd2dd20ULL,
	0x387fa9482f8c46abULL,0x42a7d9801fb9cfd2ULL,
	0x0df7adabd7a6e2d6ULL,0x772fdd63e7936bafULL,
	0xf8474c3bb7cdf024ULL,0x829f3cf387f8795dULL,
	0x66e7a46c27f3aa2cULL,0x1c3fd4a417c62355ULL,
	0x935745fc4798b8deULL,0xe98f353477ad31a7ULL,
	0xa6df411fbfb21ca3ULL,0xdc0731d78f8795daULL,
	0x536fa08fdfd90e51ULL,0x29b7d047efec8728ULL
};

const char Utils::base64EncMap[64] = {
	0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,
	0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,
	0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,
	0x59,0x5A,0x61,0x62,0x63,0x64,0x65,0x66,
	0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,
	0x6F,0x70,0x71,0x72,0x73,0x74,0x75,0x76,
	0x77,0x78,0x79,0x7A,0x30,0x31,0x32,0x33,
	0x34,0x35,0x36,0x37,0x38,0x39,0x2B,0x2F
};

const char Utils::base64DecMap[128] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x3F,
	0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,
	0x3C,0x3D,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,
	0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
	0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
	0x17,0x18,0x19,0x00,0x00,0x00,0x00,0x00,
	0x00,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,
	0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
	0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,
	0x31,0x32,0x33,0x00,0x00,0x00,0x00,0x00
};

static const char *DAY_NAMES[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static const char *MONTH_NAMES[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };

std::map<std::string,bool> Utils::listDirectory(const char *path)
{
	std::map<std::string,bool> r;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATA ffd;
	if ((hFind = FindFirstFile((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			r[std::string(ffd.cFileName)] = ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		} while (FindNextFile(hFind,&ffd));
		FindClose(hFind);
	}
#else
	struct dirent de;
	struct dirent *dptr;

	DIR *d = opendir(path);
	if (!d)
		return r;

	dptr = (struct dirent *)0;
	for(;;) {
		if (readdir_r(d,&de,&dptr))
			break;
		if (dptr) {
			if ((strcmp(dptr->d_name,"."))&&(strcmp(dptr->d_name,"..")))
				r[std::string(dptr->d_name)] = (dptr->d_type == DT_DIR);
		} else break;
	}
#endif

	return r;
}

std::string Utils::base64Encode(const void *data,unsigned int len)
{
	if (!len)
		return std::string();

	std::string out;
	unsigned int sidx = 0;

	if (len > 1) {
		while (sidx < (len - 2)) {
			out.push_back(base64EncMap[(((const unsigned char *)data)[sidx] >> 2) & 077]);
			out.push_back(base64EncMap[((((const unsigned char *)data)[sidx + 1] >> 4) & 017) | ((((const unsigned char *)data)[sidx] << 4) & 077)]);
			out.push_back(base64EncMap[((((const unsigned char *)data)[sidx + 2] >> 6) & 003) | ((((const unsigned char *)data)[sidx + 1] << 2) & 077)]);
			out.push_back(base64EncMap[((const unsigned char *)data)[sidx + 2] & 077]);
			sidx += 3;
		}
	}
	if (sidx < len) {
		out.push_back(base64EncMap[(((const unsigned char *)data)[sidx] >> 2) & 077]);
		if (sidx < len - 1) {
			out.push_back(base64EncMap[((((const unsigned char *)data)[sidx + 1] >> 4) & 017) | ((((const unsigned char *)data)[sidx] << 4) & 077)]);
			out.push_back(base64EncMap[(((const unsigned char *)data)[sidx + 1] << 2) & 077]);
		} else out.push_back(base64EncMap[(((const unsigned char *)data)[sidx] << 4) & 077]);
	}
	while (out.length() < (((len + 2) / 3) * 4))
		out.push_back('=');

	return out;
}

std::string Utils::base64Decode(const char *data,unsigned int len)
{
	if (!len)
		return std::string();
	std::string out;

	while ((len)&&(((const unsigned char *)data)[len-1] == '='))
		--len;

	for (unsigned idx=0;idx<len;idx++) {
		unsigned char ch = ((const unsigned char *)data)[idx];
		if ((ch > 47 && ch < 58) || (ch > 64 && ch < 91) || (ch > 96 && ch < 123) || ch == '+' || ch == '/' || ch == '=')
			out.push_back(base64DecMap[ch]);
		else return std::string();
	}

	unsigned outLen = len - ((len + 3) / 4);
	if ((!outLen)||((((outLen + 2) / 3) * 4) < len))
		return std::string();

	unsigned sidx = 0;
	unsigned didx = 0;
	if (outLen > 1) {
		while (didx < outLen - 2) {
			out[didx] = (((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003));
			out[didx + 1] = (((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017));
			out[didx + 2] = (((out[sidx + 2] << 6) & 255) | (out[sidx + 3] & 077));
			sidx += 4;
			didx += 3;
		}
	}

	if (didx < outLen)
		out[didx] = (((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003));
	if (++didx < outLen)
		out[didx] = (((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017));

	return out.substr(0,outLen);
}

std::string Utils::hex(const void *data,unsigned int len)
{
	std::string r;
	r.reserve(len * 2);
	for(unsigned int i=0;i<len;++i) {
		r.push_back(HEXCHARS[(((const unsigned char *)data)[i] & 0xf0) >> 4]);
		r.push_back(HEXCHARS[((const unsigned char *)data)[i] & 0x0f]);
	}
	return r;
}

std::string Utils::unhex(const char *hex)
{
	int n = 1;
	unsigned char c,b = 0;
	std::string r;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1))
				r.push_back((char)(b | (c - 48)));
			else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1))
				r.push_back((char)(b | (c - (65 - 10))));
			else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1))
				r.push_back((char)(b | (c - (97 - 10))));
			else b = (c - (97 - 10)) << 4;
		}
	}

	return r;
}

unsigned int Utils::unhex(const char *hex,void *buf,unsigned int len)
{
	int n = 1;
	unsigned char c,b = 0;
	unsigned int l = 0;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - 48));
			} else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (65 - 10)));
			} else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (97 - 10)));
			} else b = (c - (97 - 10)) << 4;
		}
	}

	return l;
}

void Utils::getSecureRandom(void *buf,unsigned int bytes)
{
	static Mutex randomLock;
	static char randbuf[32768];
	static unsigned int randptr = sizeof(randbuf);
#ifdef __WINDOWS__
	static Salsa20 s20;
	volatile bool s20Initialized = false;
#endif

	Mutex::Lock _l(randomLock);
	for(unsigned int i=0;i<bytes;++i) {
		if (randptr >= sizeof(randbuf)) {
#ifdef __UNIX_LIKE__
			int fd = ::open("/dev/urandom",O_RDONLY);
			if (fd < 0) {
				fprintf(stderr,"FATAL ERROR: unable to open /dev/urandom: %s"ZT_EOL_S,strerror(errno));
				exit(-1);
			}
			if ((int)::read(fd,randbuf,sizeof(randbuf)) != (int)sizeof(randbuf)) {
				fprintf(stderr,"FATAL ERROR: unable to read from /dev/urandom"ZT_EOL_S);
				exit(-1);
			}
			::close(fd);
#else
#ifdef __WINDOWS__
			if (!s20Initialized) {
				s20Initialized = true;
				char ktmp[32];
				char ivtmp[8];
				for(int i=0;i<32;++i) ktmp[i] = (char)rand();
				for(int i=0;i<8;++i) ivtmp[i] = (char)rand();
				double now = Utils::nowf();
				memcpy(ktmp,&now,sizeof(now));
				DWORD tmp = GetCurrentProcessId();
				memcpy(ktmp + sizeof(double),&tmp,sizeof(tmp));
				tmp = GetTickCount();
				memcpy(ktmp + sizeof(double) + sizeof(DWORD),&tmp,sizeof(tmp));
				s20.init(ktmp,256,ivtmp);
				for(int i=0;i<sizeof(randbuf);++i) randbuf[i] = (char)rand();
			}
			s20.encrypt(randbuf,randbuf,sizeof(randbuf));
#else
no getSecureRandom() implementation;
#endif
#endif
			randptr = 0;
		}
		((char *)buf)[i] = randbuf[randptr++];
	}
}

void Utils::lockDownFile(const char *path,bool isDir)
{
#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
	chmod(path,isDir ? 0700 : 0600);
#else
#ifdef _WIN32
	// TODO: windows ACL hell...
#endif
#endif
}

uint64_t Utils::getLastModified(const char *path)
{
	struct stat s;
	if (stat(path,&s))
		return 0;
	return (((uint64_t)s.st_mtime) * 1000ULL);
}

std::string Utils::toRfc1123(uint64_t t64)
{
	struct tm t;
	char buf[128];
	time_t utc = (time_t)(t64 / 1000ULL);
#ifdef __WINDOWS__
	gmtime_s(&t,&utc);
#else
	gmtime_r(&utc,&t);
#endif
	sprintf(buf,"%3s, %02d %3s %4d %02d:%02d:%02d GMT",DAY_NAMES[t.tm_wday],t.tm_mday,MONTH_NAMES[t.tm_mon],t.tm_year + 1900,t.tm_hour,t.tm_min,t.tm_sec);
	return std::string(buf);
}

#ifdef __WINDOWS__
static int is_leap(unsigned y) {
        y += 1900;
        return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}
static time_t timegm(struct tm *tm) {
        static const unsigned ndays[2][12] = {
                {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
        };
        time_t res = 0;
        int i;
        for (i = 70; i < tm->tm_year; ++i)
                res += is_leap(i) ? 366 : 365;

        for (i = 0; i < tm->tm_mon; ++i)
                res += ndays[is_leap(tm->tm_year)][i];
        res += tm->tm_mday - 1;
        res *= 24;
        res += tm->tm_hour;
        res *= 60;
        res += tm->tm_min;
        res *= 60;
        res += tm->tm_sec;
        return res;
}
#endif

uint64_t Utils::fromRfc1123(const char *tstr)
{
	struct tm t;
	char wdays[128],mons[128];

	int l = strlen(tstr);
	if ((l < 29)||(l > 64))
		return 0;
	int assigned = sscanf(tstr,"%3s, %02d %3s %4d %02d:%02d:%02d GMT",wdays,&t.tm_mday,mons,&t.tm_year,&t.tm_hour,&t.tm_min,&t.tm_sec);
	if (assigned != 7)
		return 0;

	wdays[3] = '\0';
	for(t.tm_wday=0;t.tm_wday<7;++t.tm_wday) {
#ifdef __WINDOWS__
		if (!_stricmp(DAY_NAMES[t.tm_wday],wdays))
			break;
#else
		if (!strcasecmp(DAY_NAMES[t.tm_wday],wdays))
			break;
#endif
	}
	if (t.tm_wday == 7)
		return 0;
	mons[3] = '\0';
	for(t.tm_mon=0;t.tm_mon<12;++t.tm_mon) {
#ifdef __WINDOWS__
		if (!_stricmp(MONTH_NAMES[t.tm_mday],mons))
			break;
#else
		if (!strcasecmp(MONTH_NAMES[t.tm_mday],mons))
			break;
#endif
	}
	if (t.tm_mon == 12)
		return 0;

	t.tm_wday = 0; // ignored by timegm
	t.tm_yday = 0; // ignored by timegm
	t.tm_isdst = 0; // ignored by timegm

	time_t utc = timegm(&t);

	return ((utc > 0) ? (1000ULL * (uint64_t)utc) : 0ULL);
}

bool Utils::readFile(const char *path,std::string &buf)
{
	char tmp[4096];
	FILE *f = fopen(path,"rb");
	if (f) {
		for(;;) {
			long n = (long)fread(tmp,1,sizeof(tmp),f);
			if (n > 0)
				buf.append(tmp,n);
			else break;
		}
		fclose(f);
		return true;
	}
	return false;
}

bool Utils::writeFile(const char *path,const void *buf,unsigned int len)
{
	FILE *f = fopen(path,"wb");
	if (f) {
		if ((long)fwrite(buf,1,len,f) != (long)len) {
			fclose(f);
			return false;
		} else {
			fclose(f);
			return true;
		}
	}
	return false;
}

std::vector<std::string> Utils::split(const char *s,const char *const sep,const char *esc,const char *quot)
{
	std::vector<std::string> fields;
	std::string buf;

	if (!esc)
		esc = "";
	if (!quot)
		quot = "";

	bool escapeState = false;
	char quoteState = 0;
	while (*s) {
		if (escapeState) {
			escapeState = false;
			buf.push_back(*s);
		} else if (quoteState) {
			if (*s == quoteState) {
				quoteState = 0;
				fields.push_back(buf);
				buf.clear();
			} else buf.push_back(*s);
		} else {
			const char *quotTmp;
			if (strchr(esc,*s))
				escapeState = true;
			else if ((buf.size() <= 0)&&((quotTmp = strchr(quot,*s))))
				quoteState = *quotTmp;
			else if (strchr(sep,*s)) {
				if (buf.size() > 0) {
					fields.push_back(buf);
					buf.clear();
				} // else skip runs of seperators
			} else buf.push_back(*s);
		}
		++s;
	}

	if (buf.size())
		fields.push_back(buf);

	return fields;
}

std::string Utils::trim(const std::string &s)
{
	unsigned long end = s.length();
	while (end) {
		char c = s[end - 1];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			--end;
		else break;
	}
	unsigned long start = 0;
	while (start < end) {
		char c = s[start];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			++start;
		else break;
	}
	return s.substr(start,end - start);
}

void Utils::stdsprintf(std::string &s,const char *fmt,...)
	throw(std::bad_alloc,std::length_error)
{
	char buf[65536];
	va_list ap;

	va_start(ap,fmt);
	int n = vsnprintf(buf,sizeof(buf),fmt,ap);
	va_end(ap);

	if ((n >= (int)sizeof(buf))||(n < 0))
		throw std::length_error("printf result too large");

	s.append(buf);
}

} // namespace ZeroTier
