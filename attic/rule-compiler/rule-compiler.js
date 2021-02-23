/*
 * Copyright (c)2019 ZeroTier, Inc.
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

'use strict';

// Names for bits in characteristics -- 0==LSB, 63==MSB
const CHARACTERISTIC_BITS = {
	'inbound': 63,
	'multicast': 62,
	'broadcast': 61,
	'ipauth': 60,
	'macauth': 59,
	'tcp_fin': 0,
	'tcp_syn': 1,
	'tcp_rst': 2,
	'tcp_psh': 3,
	'tcp_ack': 4,
	'tcp_urg': 5,
	'tcp_ece': 6,
	'tcp_cwr': 7,
	'tcp_ns': 8,
	'tcp_rs2': 9,
	'tcp_rs1': 10,
	'tcp_rs0': 11
};

// Shorthand names for common ethernet types
const ETHERTYPES = {
	'ipv4': 0x0800,
	'arp': 0x0806,
	'wol': 0x0842,
	'rarp': 0x8035,
	'ipv6': 0x86dd,
	'atalk': 0x809b,
	'aarp': 0x80f3,
	'ipx_a': 0x8137,
	'ipx_b': 0x8138
};

// Shorthand names for common IP protocols
const IP_PROTOCOLS = {
	'icmp': 0x01,
	'icmp4': 0x01,
	'icmpv4': 0x01,
	'igmp': 0x02,
	'ipip': 0x04,
	'tcp': 0x06,
	'egp': 0x08,
	'igp': 0x09,
	'udp': 0x11,
	'rdp': 0x1b,
	'esp': 0x32,
	'ah': 0x33,
	'icmp6': 0x3a,
	'icmpv6': 0x3a,
	'l2tp': 0x73,
	'sctp': 0x84,
	'udplite': 0x88
};

// Keywords that open new blocks that must be terminated by a semicolon
const OPEN_BLOCK_KEYWORDS = {
	'macro': true,
	'tag': true,
	'cap': true,
	'drop': true,
	'accept': true,
	'tee': true,
	'watch': true,
	'redirect': true,
	'break': true,
	'priority': true
};

// Reserved words that can't be used as tag, capability, or rule set names
const RESERVED_WORDS = {
	'macro': true,
	'tag': true,
	'cap': true,
	'default': true,

	'drop': true,
	'accept': true,
	'tee': true,
	'watch': true,
	'redirect': true,
	'break': true,
	'priority': true,

	'ztsrc': true,
	'ztdest': true,
	'vlan': true,
	'vlanpcp': true,
	'vlandei': true,
	'ethertype': true,
	'macsrc': true,
	'macdest': true,
	'ipsrc': true,
	'ipdest': true,
	'iptos': true,
	'ipprotocol': true,
	'icmp': true,
	'sport': true,
	'dport': true,
	'chr': true,
	'framesize': true,
	'random': true,
	'tand': true,
	'tor': true,
	'txor': true,
	'tdiff': true,
	'teq': true,
	'tseq': true,
	'treq': true,

	'type': true,
	'enum': true,
	'class': true,
	'define': true,
	'import': true,
	'include': true,
	'log': true,
	'not': true,
	'xor': true,
	'or': true,
	'and': true,
	'set': true,
	'var': true,
	'let': true
};

const KEYWORD_TO_API_MAP = {
	'drop': 'ACTION_DROP',
	'accept': 'ACTION_ACCEPT',
	'tee': 'ACTION_TEE',
	'watch': 'ACTION_WATCH',
	'redirect': 'ACTION_REDIRECT',
	'break': 'ACTION_BREAK',
	'priority': 'ACTION_PRIORITY',

	'ztsrc': 'MATCH_SOURCE_ZEROTIER_ADDRESS',
	'ztdest': 'MATCH_DEST_ZEROTIER_ADDRESS',
	'vlan': 'MATCH_VLAN_ID',
	'vlanpcp': 'MATCH_VLAN_PCP',
	'vlandei': 'MATCH_VLAN_DEI',
	'ethertype': 'MATCH_ETHERTYPE',
	'macsrc': 'MATCH_MAC_SOURCE',
	'macdest': 'MATCH_MAC_DEST',
	//'ipsrc': '', // special handling since we programmatically differentiate between V4 and V6
	//'ipdest': '', // special handling
	'iptos': 'MATCH_IP_TOS',
	'ipprotocol': 'MATCH_IP_PROTOCOL',
	'icmp': 'MATCH_ICMP',
	'sport': 'MATCH_IP_SOURCE_PORT_RANGE',
	'dport': 'MATCH_IP_DEST_PORT_RANGE',
	'chr': 'MATCH_CHARACTERISTICS',
	'framesize': 'MATCH_FRAME_SIZE_RANGE',
	'random': 'MATCH_RANDOM',
	'tand': 'MATCH_TAGS_BITWISE_AND',
	'tor': 'MATCH_TAGS_BITWISE_OR',
	'txor': 'MATCH_TAGS_BITWISE_XOR',
	'tdiff': 'MATCH_TAGS_DIFFERENCE',
	'teq': 'MATCH_TAGS_EQUAL',
	'tseq': 'MATCH_TAG_SENDER',
	'treq': 'MATCH_TAG_RECEIVER'
};

// Number of args for each match
const MATCH_ARG_COUNTS = {
	'ztsrc': 1,
	'ztdest': 1,
	'vlan': 1,
	'vlanpcp': 1,
	'vlandei': 1,
	'ethertype': 1,
	'macsrc': 1,
	'macdest': 1,
	'ipsrc': 1,
	'ipdest': 1,
	'iptos': 2,
	'ipprotocol': 1,
	'icmp': 2,
	'sport': 1,
	'dport': 1,
	'chr': 1,
	'framesize': 1,
	'random': 1,
	'tand': 2,
	'tor': 2,
	'txor': 2,
	'tdiff': 2,
	'teq': 2,
	'tseq': 2,
	'treq': 2
};

// Regex of all alphanumeric characters in Unicode
const INTL_ALPHANUM_REGEX = new RegExp('[0-9A-Za-z\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u0527\u0531-\u0556\u0559\u0561-\u0587\u05D0-\u05EA\u05F0-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u08A0\u08A2-\u08AC\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0977\u0979-\u097F\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C33\u0C35-\u0C39\u0C3D\u0C58\u0C59\u0C60\u0C61\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D60\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F4\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1877\u1880-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191C\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19C1-\u19C7\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2183\u2184\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005\u3006\u3031-\u3035\u303B\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FCC\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA697\uA6A0-\uA6E5\uA717-\uA71F\uA722-\uA788\uA78B-\uA78E\uA790-\uA793\uA7A0-\uA7AA\uA7F8-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA80-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uABC0-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]');

// Checks whether something is a valid capability, tag, or macro name
function _isValidName(n)
{
	if ((typeof n !== 'string')||(n.length === 0)) return false;
	if ("0123456789".indexOf(n.charAt(0)) >= 0) return false;
	for(let i=0;i<n.length;++i) {
		let c = n.charAt(i);
		if ((c !== '_')&&(!INTL_ALPHANUM_REGEX.test(c))) return false;
	}
	return true;
}

// Regexes for checking the basic syntactic validity of IP addresses
const IPV6_REGEX = new RegExp('(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))');
const IPV4_REGEX = new RegExp('((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])');

function _parseNum(n)
{
	try {
		if ((typeof n !== 'string')||(n.length === 0))
			return -1;
		n = n.toLowerCase();
		if ((n.length > 2)&&(n.substr(0,2) === '0x'))
			n = parseInt(n.substr(2),16);
		else n = parseInt(n,10);
		return (((typeof n === 'number')&&(n !== null)&&(!isNaN(n))) ? n : -1);
	} catch (e) {
		return -1;
	}
}

function _cleanMac(m)
{
	m = m.toLowerCase();
	var m2 = '';
	let charcount = 0;
	for(let i=0;((i<m.length)&&(m2.length<17));++i) {
		let c = m.charAt(i);
		if ("0123456789abcdef".indexOf(c) >= 0) {
			m2 += c;
			charcount++;
			if ((m2.length > 0)&&(m2.length !== 17)&&(charcount >= 2) ) {
				m2 += ':';
				charcount=0;
			}
		}
	}
	return m2;
}

function _cleanHex(m)
{
	m = m.toLowerCase();
	var m2 = '';
	for(let i=0;i<m.length;++i) {
		let c = m.charAt(i);
		if ("0123456789abcdef".indexOf(c) >= 0)
			m2 += c;
	}
	return m2;
}

function _renderMatches(mtree,rules,macros,caps,tags,params)
{
	let not = false;
	let or = false;
	for(let k=0;k<mtree.length;++k) {
		let match = (typeof mtree[k][0] === 'string') ? mtree[k][0].toLowerCase() : '';
		if ((match.length === 0)||(match === 'and')) { // AND is the default
			continue;
		} else if (match === 'not') {
			not = true;
		} else if (match === 'or') {
			or = true;
		} else {
			let args = [];
			let argCount = MATCH_ARG_COUNTS[match];
			if (!argCount)
				return [ mtree[k][1],mtree[k][2],'Unrecognized match type "'+match+'".' ];
			for(let i=0;i<argCount;++i) {
				if (++k >= mtree.length)
					return [ mtree[k - 1][1],mtree[k - 1][2],'Missing argument(s) to match.' ];
				let arg = mtree[k][0];
				if ((typeof arg !== 'string')||(arg in RESERVED_WORDS)||(arg.length === 0))
					return [ mtree[k - 1][1],mtree[k - 1][2],'Missing argument(s) to match (invalid argument or argument is reserved word).' ];
				if (arg.charAt(0) === '$') {
					let tmp = params[arg];
					if (typeof tmp === 'undefined')
						return [ mtree[k][1],mtree[k][2],'Undefined variable name.' ];
					args.push([ tmp,mtree[k][1],mtree[k][2] ]);
				} else {
					args.push(mtree[k]);
				}
			}

			switch(match) {
				case 'ztsrc':
				case 'ztdest': {
					let zt = _cleanHex(args[0][0]);
					if (zt.length !== 10)
						return [ args[0][1],args[0][2],'Invalid ZeroTier address.' ];
					rules.push({
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or,
						'zt': zt
					});
				}	break;

				case 'vlan':
				case 'vlanpcp':
				case 'vlandei':
				case 'ethertype':
				case 'ipprotocol': {
					let num = null;
					switch (match) {
						case 'ethertype': num = ETHERTYPES[args[0][0]]; break;
						case 'ipprotocol': num = IP_PROTOCOLS[args[0][0]]; break;
					}
					if (typeof num !== 'number')
						num = _parseNum(args[0][0]);
					if ((typeof num !== 'number')||(num < 0)||(num > 0xffffffff)||(num === null))
						return [ args[0][1],args[0][2],'Invalid numeric value.' ];
					let r = {
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or
					};
					switch(match) {
						case 'vlan': r['vlanId'] = num; break;
						case 'vlanpcp': r['vlanPcp'] = num; break;
						case 'vlandei': r['vlanDei'] = num; break;
						case 'ethertype': r['etherType'] = num; break;
						case 'ipprotocol': r['ipProtocol'] = num; break;
					}
					rules.push(r);
				}	break;

				case 'random': {
					let num = parseFloat(args[0][0])||0.0;
					if (num < 0.0) num = 0.0;
					if (num > 1.0) num = 1.0;
					rules.push({
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or,
						'probability': Math.floor(4294967295 * num)
					});
				}	break;

				case 'macsrc':
				case 'macdest': {
					let mac = _cleanMac(args[0][0]);
					if (mac.length !== 17)
						return [ args[0][1],args[0][2],'Invalid MAC address.' ];
					rules.push({
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or,
						'mac': mac
					});
				}	break;

				case 'ipsrc':
				case 'ipdest': {
					let ip = args[0][0];
					let slashIdx = ip.indexOf('/');
					if (slashIdx <= 0)
						return [ args[0][1],args[0][2],'Missing /bits netmask length designation in IP.' ];
					let ipOnly = ip.substr(0,slashIdx);
					if (IPV6_REGEX.test(ipOnly)) {
						rules.push({
							'type': ((match === 'ipsrc') ? 'MATCH_IPV6_SOURCE' : 'MATCH_IPV6_DEST'),
							'not': not,
							'or': or,
							'ip': ip
						});
					} else if (IPV4_REGEX.test(ipOnly)) {
						rules.push({
							'type': ((match === 'ipsrc') ? 'MATCH_IPV4_SOURCE' : 'MATCH_IPV4_DEST'),
							'not': not,
							'or': or,
							'ip': ip
						});
					} else {
						return [ args[0][1],args[0][2],'Invalid IP address (not valid IPv4 or IPv6).' ];
					}
				}	break;

				case 'icmp': {
					let icmpType = _parseNum(args[0][0]);
					if ((icmpType < 0)||(icmpType > 0xff))
						return [ args[0][1],args[0][2],'Missing or invalid ICMP type.' ];
					let icmpCode = _parseNum(args[1][0]); // -1 okay, indicates don't match code
					if (icmpCode > 0xff)
						return [ args[1][1],args[1][2],'Invalid ICMP code (use -1 for none).' ];
					rules.push({
						'type': 'MATCH_ICMP',
						'not': not,
						'or': or,
						'icmpType': icmpType,
						'icmpCode': ((icmpCode < 0) ? null : icmpCode)
					});
				}	break;

				case 'sport':
				case 'dport':
				case 'framesize': {
					let arg = args[0][0];
					let fn = null;
					let tn = null;
					if (arg.indexOf('-') > 0) {
						let asplit = arg.split('-');
						if (asplit.length !== 2) {
							return [ args[0][1],args[0][2],'Invalid numeric range.' ];
						} else {
							fn = _parseNum(asplit[0]);
							tn = _parseNum(asplit[1]);
						}
					} else {
						fn = _parseNum(arg);
						tn = fn;
					}
					if ((fn < 0)||(fn > 0xffff)||(tn < 0)||(tn > 0xffff)||(tn < fn))
						return [ args[0][1],args[0][2],'Invalid numeric range.' ];
					rules.push({
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or,
						'start': fn,
						'end': tn
					});
				}	break;

				case 'iptos': {
					let mask = _parseNum(args[0][0]);
					if ((typeof mask !== 'number')||(mask < 0)||(mask > 0xff)||(mask === null))
						return [ args[0][1],args[0][2],'Invalid mask.' ];
					let arg = args[1][0];
					let fn = null;
					let tn = null;
					if (arg.indexOf('-') > 0) {
						let asplit = arg.split('-');
						if (asplit.length !== 2) {
							return [ args[1][1],args[1][2],'Invalid value range.' ];
						} else {
							fn = _parseNum(asplit[0]);
							tn = _parseNum(asplit[1]);
						}
					} else {
						fn = _parseNum(arg);
						tn = fn;
					}
					if ((fn < 0)||(fn > 0xff)||(tn < 0)||(tn > 0xff)||(tn < fn))
						return [ args[1][1],args[1][2],'Invalid value range.' ];
					rules.push({
						'type': 'MATCH_IP_TOS',
						'not': not,
						'or': or,
						'mask': mask,
						'start': fn,
						'end': tn
					});
				}	break;

				case 'chr': {
					let chrb = args[0][0].split(/[,]+/);
					let maskhi = 0;
					let masklo = 0;
					for(let i=0;i<chrb.length;++i) {
						if (chrb[i].length > 0) {
							let tmp = CHARACTERISTIC_BITS[chrb[i]];
							let bit = (typeof tmp === 'number') ? tmp : _parseNum(chrb[i]);
							if ((bit < 0)||(bit > 63))
								return [ args[0][1],args[0][2],'Invalid bit index (range 0-63) or unrecognized name.' ];
							if (bit >= 32)
								maskhi |= Math.abs(1 << (bit - 32));
							else masklo |= Math.abs(1 << bit);
						}
					}
					maskhi = Math.abs(maskhi).toString(16);
					while (maskhi.length < 8) maskhi = '0' + maskhi;
					masklo = Math.abs(masklo).toString(16);
					while (masklo.length < 8) masklo = '0' + masklo;
					rules.push({
						'type': 'MATCH_CHARACTERISTICS',
						'not': not,
						'or': or,
						'mask': (maskhi + masklo)
					});
				}	break;

				case 'tand':
				case 'tor':
				case 'txor':
				case 'tdiff':
				case 'teq':
				case 'tseq':
				case 'treq': {
					let tag = tags[args[0][0]];
					let tagId = -1;
					let tagValue = -1;
					if (tag) {
						tagId = tag.id;
						tagValue = args[1][0];
						if (tagValue in tag.flags)
							tagValue = tag.flags[tagValue];
						else if (tagValue in tag.enums)
							tagValue = tag.enums[tagValue];
						else tagValue = _parseNum(tagValue);
					} else {
						tagId = _parseNum(args[0][0]);
						tagValue = _parseNum(args[1][0]);
					}
					if ((tagId < 0)||(tagId > 0xffffffff))
						return [ args[0][1],args[0][2],'Undefined tag name and invalid tag value.' ];
					if ((tagValue < 0)||(tagValue > 0xffffffff))
						return [ args[1][1],args[1][2],'Invalid tag value or unrecognized flag/enum name.' ];
					rules.push({
						'type': KEYWORD_TO_API_MAP[match],
						'not': not,
						'or': or,
						'id': tagId,
						'value': tagValue
					});
				}	break;
			}

			not = false;
			or = false;
		}
	}
	return null;
}

function _renderActions(rtree,rules,macros,caps,tags,params)
{
	for(let k=0;k<rtree.length;++k) {
		let action = (typeof rtree[k][0] === 'string') ? rtree[k][0].toLowerCase() : '';
		if (action.length === 0) {
			continue;
		} else if (action === 'include') {
			if ((k + 1) >= rtree.length)
				return [ rtree[k][1],rtree[k][2],'Include directive is missing a macro name.' ];
			let macroName = rtree[k + 1][0];
			++k;

			let macroParamArray = [];
			let parenIdx = macroName.indexOf('(');
			if (parenIdx > 0) {
				let pns = macroName.substr(parenIdx + 1).split(/[,)]+/);
				for(let k=0;k<pns.length;++k) {
					if (pns[k].length > 0)
						macroParamArray.push(pns[k]);
				}
				macroName = macroName.substr(0,parenIdx);
			}

			let macro = macros[macroName];
			if (!macro)
				return [ rtree[k][1],rtree[k][2],'Macro name not found.' ];
			let macroParams = {};
			for(let param in macro.params) {
				let pidx = macro.params[param];
				if (pidx >= macroParamArray.length)
					return [ rtree[k][1],rtree[k][2],'Missing one or more required macro parameter.' ];
				macroParams[param] = macroParamArray[pidx];
			}

			let err = _renderActions(macro.rules,rules,macros,caps,tags,macroParams);
			if (err !== null)
				return err;
		} else if ((action === 'drop')||(action === 'accept')||(action === 'break')) { // actions without arguments
			if (((k + 1) < rtree.length)&&(Array.isArray(rtree[k + 1][0]))) {
				let mtree = rtree[k + 1]; ++k;
				let err = _renderMatches(mtree,rules,macros,caps,tags,params);
				if (err !== null)
					return err;
			}
			rules.push({
				'type': KEYWORD_TO_API_MAP[action]
			});
		} else if ((action === 'tee')||(action === 'watch')) { // actions with arguments (ZeroTier address)
			if (((k + 1) < rtree.length)&&(Array.isArray(rtree[k + 1][0]))&&(rtree[k + 1][0].length >= 2)) {
				let mtree = rtree[k + 1]; ++k;
				let maxLength = _parseNum(mtree[0][0]);
				if ((maxLength < -1)||(maxLength > 0xffff))
					return [ mtree[0][1],mtree[1][2],'Tee/watch max packet length to forward invalid or out of range.' ];
				let target = mtree[1][0];
				if ((typeof target !== 'string')||(target.length !== 10))
					return [ mtree[1][1],mtree[1][2],'Missing or invalid ZeroTier address target for tee/watch.' ];
				let err = _renderMatches(mtree.slice(2),rules,macros,caps,tags,params);
				if (err !== null)
					return err;
				rules.push({
					'type': KEYWORD_TO_API_MAP[action],
					'address': target,
					'length': maxLength
				});
			} else {
				return [ rtree[k][1],rtree[k][2],'The tee and watch actions require two paremters (max length or 0 for all, target).' ];
			}
		} else if (action === 'redirect') {
			if (((k + 1) < rtree.length)&&(Array.isArray(rtree[k + 1][0]))&&(rtree[k + 1][0].length >= 1)) {
				let mtree = rtree[k + 1]; ++k;
				let target = mtree[0][0];
				if ((typeof target !== 'string')||(target.length !== 10))
					return [ mtree[0][1],mtree[0][2],'Missing or invalid ZeroTier address target for redirect.' ];
				let err = _renderMatches(mtree.slice(1),rules,macros,caps,tags,params);
				if (err !== null)
					return err;
				rules.push({
					'type': KEYWORD_TO_API_MAP[action],
					'address': target
				});
			} else {
				return [ rtree[k][1],rtree[k][2],'The redirect action requires a target parameter.' ];
			}
		} else {
			return [ rtree[k][1],rtree[k][2],'Unrecognized action or directive in rule set.' ];
		}
	}

	return null;
}

function compile(src,rules,caps,tags)
{
	try {
		if (typeof src !== 'string')
			return [ 0,0,'"src" parameter must be a string.' ];

		// Pass 1: parse source into a tree of arrays of elements. Each element is a 3-item
		// tuple consisting of string, line number, and character index in line to enable
		// informative error messages to be returned.

		var blockStack = [ [] ];
		var curr = [ '',-1,-1 ];
		var skipRestOfLine = false;
		for(let idx=0,lineNo=1,lineIdx=0;idx<src.length;++idx,++lineIdx) {
			let ch = src.charAt(idx);
			if (skipRestOfLine) {
				if ((ch === '\r')||(ch === '\n')) {
					skipRestOfLine = false;
					++lineNo;
					lineIdx = 0;
				}
			} else {
				switch(ch) {
					case '\n':
						++lineNo;
						lineIdx = 0;
					case '\r':
					case '\t':
					case ' ':
						if (curr[0].length > 0) {
							let endOfBlock = false;
							if (curr[0].charAt(curr[0].length - 1) === ';') {
								endOfBlock = true;
								curr[0] = curr[0].substr(0,curr[0].length - 1);
							}

							if (curr[0].length > 0) {
								blockStack[blockStack.length - 1].push(curr);
							}
							if ((endOfBlock)&&(blockStack.length > 1)&&(blockStack[blockStack.length - 1].length > 0)) {
								blockStack[blockStack.length - 2].push(blockStack[blockStack.length - 1]);
								blockStack.pop();
							} else if (curr[0] in OPEN_BLOCK_KEYWORDS) {
								blockStack.push([]);
							}

							curr = [ '',-1,-1 ];
						}
						break;
					default:
						if (curr[0].length === 0) {
							if (ch === '#') {
								skipRestOfLine = true;
								continue;
							} else {
								curr[1] = lineNo;
								curr[2] = lineIdx;
							}
						}
						curr[0] += ch;
						break;
				}
			}
		}

		if (curr[0].length > 0) {
			if (curr[0].charAt(curr[0].length - 1) === ';')
				curr[0] = curr[0].substr(0,curr[0].length - 1);
			if (curr[0].length > 0)
				blockStack[blockStack.length - 1].push(curr);
		}
		while ((blockStack.length > 1)&&(blockStack[blockStack.length - 1].length > 0)) {
			blockStack[blockStack.length - 2].push(blockStack[blockStack.length - 1]);
			blockStack.pop();
		}
		var parsed = blockStack[0];

		// Pass 2: parse tree into capabilities, tags, rule sets, and document-level rules.

		let baseRuleTree = [];
		let macros = {};
		for(let i=0;i<parsed.length;++i) {
			let keyword = (typeof parsed[i][0] === 'string') ? parsed[i][0].toLowerCase() : null;
			if (keyword === 'macro') {
				// Define macros

				if ( ((i + 1) >= parsed.length) || (!Array.isArray(parsed[i + 1])) || (parsed[i + 1].length < 1) || (!Array.isArray(parsed[i + 1][0])) )
					return [ parsed[i][1],parsed[i][2],'Macro definition is missing name.' ];
				let macro = parsed[++i];
				let macroName = macro[0][0].toLowerCase();

				let params = {};
				let parenIdx = macroName.indexOf('(');
				if (parenIdx > 0) {
					let pns = macroName.substr(parenIdx + 1).split(/[,)]+/);
					for(let k=0;k<pns.length;++k) {
						if (pns[k].length > 0)
							params[pns[k]] = k;
					}
					macroName = macroName.substr(0,parenIdx);
				}

				if (!_isValidName(macroName))
					return [ macro[0][1],macro[0][2],'Invalid macro name.' ];
				if (macroName in RESERVED_WORDS)
					return [ macro[0][1],macro[0][2],'Macro name is a reserved word.' ];

				if (macroName in macros)
					return [ macro[0][1],macro[0][2],'Multiple definition of macro name.' ];

				macros[macroName] = {
					params: params,
					rules: macro.slice(1)
				};
			} else if (keyword === 'tag') {
				// Define tags

				if ( ((i + 1) >= parsed.length) || (!Array.isArray(parsed[i + 1])) || (parsed[i + 1].length < 1) || (!Array.isArray(parsed[i + 1][0])) )
					return [ parsed[i][1],parsed[i][2],'Tag definition is missing name.' ];
				let tag = parsed[++i];
				let tagName = tag[0][0].toLowerCase();

				if (!_isValidName(tagName))
					return [ tag[0][1],tag[0][2],'Invalid tag name.' ];
				if (tagName in RESERVED_WORDS)
					return [ tag[0][1],tag[0][2],'Tag name is a reserved word.' ];

				if (tagName in tags)
					return [ tag[0][1],tag[0][2],'Multiple definition of tag name.' ];

				let flags = {};
				let enums = {};
				let id = -1;
				let dfl = null;
				for(let k=1;k<tag.length;++k) {
					let tkeyword = tag[k][0].toLowerCase();
					if (tkeyword === 'id') {
						if (id >= 0)
							return [ tag[k][1],tag[k][2],'Duplicate tag id definition.' ];
						if ((k + 1) >= tag.length)
							return [ tag[k][1],tag[k][2],'Missing numeric value for ID.' ];
						id = _parseNum(tag[++k][0]);
						if ((id < 0)||(id > 0xffffffff))
							return [ tag[k][1],tag[k][2],'Invalid or out of range tag ID.' ];
					} else if (tkeyword === 'default') {
						if (dfl !== null)
							return [ tag[k][1],tag[k][2],'Duplicate tag default directive.' ];
						if ((k + 1) >= tag.length)
							return [ tag[k][1],tag[k][2],'Missing value for default.' ];
						dfl = tag[++k][0];
					} else if (tkeyword === 'flag') {
						if ((k + 2) >= tag.length)
							return [ tag[k][1],tag[k][2],'Missing tag flag name or bit index.' ];
						++k;
						let bits = tag[k][0].split(/[,]+/);
						let mask = 0;
						for(let j=0;j<bits.length;++j) {
							let b = bits[j].toLowerCase();
							if (b in flags) {
								mask |= flags[b];
							} else {
								b = _parseNum(b);
								if ((b < 0)||(b > 31))
									return [ tag[k][1],tag[k][2],'Bit index invalid, out of range, or references an undefined flag name.' ];
								mask |= (1 << b);
							}
						}
						let flagName = tag[++k][0].toLowerCase();
						if (!_isValidName(flagName))
							return [ tag[k][1],tag[k][2],'Invalid or reserved flag name.' ];
						if (flagName in flags)
							return [ tag[k][1],tag[k][2],'Duplicate flag name in tag definition.' ];
						flags[flagName] = mask;
					} else if (tkeyword === 'enum') {
						if ((k + 2) >= tag.length)
							return [ tag[k][1],tag[k][2],'Missing tag enum name or value.' ];
						++k;
						let value = _parseNum(tag[k][0]);
						if ((value < 0)||(value > 0xffffffff))
							return [ tag[k][1],tag[k][2],'Tag enum value invalid or out of range.' ];
						let enumName = tag[++k][0].toLowerCase();
						if (!_isValidName(enumName))
							return [ tag[k][1],tag[k][2],'Invalid or reserved tag enum name.' ];
						if (enumName in enums)
							return [ tag[k][1],tag[k][2],'Duplicate enum name in tag definition.' ];
						enums[enumName] = value;
					} else {
						return [ tag[k][1],tag[k][2],'Unrecognized keyword in tag definition.' ];
					}
				}
				if (id < 0)
					return [ tag[0][1],tag[0][2],'Tag definition is missing a numeric ID.' ];

				if (typeof dfl === 'string') {
					let dfl2 = enums[dfl];
					if (typeof dfl2 === 'number') {
						dfl = dfl2;
					} else {
						dfl2 = flags[dfl];
						if (typeof dfl2 === 'number') {
							dfl = dfl2;
						} else {
							dfl = Math.abs(parseInt(dfl)||0) & 0xffffffff;
						}
					}
				} else if (typeof dfl === 'number') {
					dfl = Math.abs(dfl) & 0xffffffff;
				}

				tags[tagName] = {
					'id': id,
					'default': dfl,
					'enums': enums,
					'flags': flags
				};
			} else if (keyword === 'cap') {
				// Define capabilities

				if ( ((i + 1) >= parsed.length) || (!Array.isArray(parsed[i + 1])) || (parsed[i + 1].length < 1) || (!Array.isArray(parsed[i + 1][0])) )
					return [ parsed[i][1],parsed[i][2],'Capability definition is missing name.' ];
				let cap = parsed[++i];
				let capName = cap[0][0].toLowerCase();

				if (!_isValidName(capName))
					return [ cap[0][1],cap[0][2],'Invalid capability name.' ];
				if (capName in RESERVED_WORDS)
					return [ cap[0][1],cap[0][2],'Capability name is a reserved word.' ];

				if (capName in caps)
					return [ cap[0][1],cap[0][2],'Multiple definition of capability name.' ];

				let capRules = [];
				let id = -1;
				let dfl = false;
				for(let k=1;k<cap.length;++k) {
					let dn = (typeof cap[k][0] === 'string') ? cap[k][0].toLowerCase() : null;
					if (dn === 'id') {
						if (id >= 0)
							return [ cap[k][1],cap[k][2],'Duplicate id directive in capability definition.' ];
						if ((k + 1) >= cap.length)
							return [ cap[k][1],cap[k][2],'Missing value for ID.' ];
						id = _parseNum(cap[++k][0]);
						if ((id < 0)||(id > 0xffffffff))
							return [ cap[k - 1][1],cap[k - 1][2],'Invalid or out of range capability ID.' ];
						for(let cn in caps) {
							if (caps[cn].id === id)
								return [ cap[k - 1][1],cap[k - 1][2],'Duplicate capability ID.' ];
						}
					} else if (dn === 'default') {
						dfl = true;
					} else {
						capRules.push(cap[k]);
					}
				}
				if (id < 0)
					return [ cap[0][1],cap[0][2],'Capability definition is missing a numeric ID.' ];

				caps[capName] = {
					'id': id,
					'default': dfl,
					'rules': capRules
				};
			} else {
				baseRuleTree.push(parsed[i]);
			}
		}

		// Pass 3: render low-level ZeroTier rules arrays for capabilities and base.

		for(let capName in caps) {
			let r = [];
			let err = _renderActions(caps[capName].rules,r,macros,caps,tags,{});
			if (err !== null)
				return err;
			caps[capName].rules = r;
		}

		let err = _renderActions(baseRuleTree,rules,macros,caps,tags,{});
		if (err !== null)
			return err;

		return null;
	} catch (e) {
		console.log(e.stack);
		return [ 0,0,'Unexpected exception: '+e.toString() ];
	}
}

exports.compile = compile;
