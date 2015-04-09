/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include "../node/Constants.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <winhttp.h>
#include <locale>
#include <codecvt>
#endif // __WINDOWS__

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif // __UNIX_LIKE__

#include <vector>
#include <utility>
#include <algorithm>

#include "HttpClient.hpp"
#include "Thread.hpp"
#include "OSUtils.hpp"

namespace ZeroTier {

#ifdef __UNIX_LIKE__

// The *nix implementation calls 'curl' externally rather than linking to it.
// This makes it an optional dependency that can be avoided in tiny systems
// provided you don't want to have automatic software updates... or want to
// do them via another method.

#ifdef __APPLE__
// TODO: get proxy configuration
#endif

// Paths where "curl" may be found on the system
#define NUM_CURL_PATHS 6
static const char *CURL_PATHS[NUM_CURL_PATHS] = { "/usr/bin/curl","/bin/curl","/usr/local/bin/curl","/usr/sbin/curl","/sbin/curl","/usr/libexec/curl" };

// Maximum message length
#define CURL_MAX_MESSAGE_LENGTH (1024 * 1024 * 64)

// Internal private thread class that performs request, notifies handler,
// and then commits suicide by deleting itself.
class HttpClient_Private_Request
{
public:
	HttpClient_Private_Request(HttpClient *parent,const char *method,const std::string &url,const std::map<std::string,std::string> &headers,unsigned int timeout,void (*handler)(void *,int,const std::string &,const std::string &),void *arg) :
		_url(url),
		_headers(headers),
		_timeout(timeout),
		_handler(handler),
		_arg(arg),
		_parent(parent),
		_pid(0),
		_cancelled(false)
	{
		_myThread = Thread::start(this);
	}

	~HttpClient_Private_Request()
	{
		Mutex::Lock _l(_parent->_requests_m);
		_parent->_requests.erase((HttpClient::Request)this);
	}

	void threadMain()
	{
		char *curlArgs[1024];
		char buf[16384];
		fd_set readfds,writefds,errfds;
		struct timeval tv;

		std::string curlPath;
		for(int i=0;i<NUM_CURL_PATHS;++i) {
			if (OSUtils::fileExists(CURL_PATHS[i])) {
				curlPath = CURL_PATHS[i];
				break;
			}
		}

		if (!curlPath.length()) {
			_doH(_arg,-1,_url,"unable to locate 'curl' binary in /usr/bin, /bin, /usr/local/bin, /usr/sbin, or /sbin");
			delete this;
			return;
		}
		if (!_url.length()) {
			_doH(_arg,-1,_url,"cannot fetch empty URL");
			delete this;
			return;
		}

		curlArgs[0] = const_cast <char *>(curlPath.c_str());
		curlArgs[1] = const_cast <char *>("-D");
		curlArgs[2] = const_cast <char *>("-"); // append headers before output
		int argPtr = 3;
		std::vector<std::string> headerArgs;
		for(std::map<std::string,std::string>::const_iterator h(_headers.begin());h!=_headers.end();++h) {
			headerArgs.push_back(h->first);
			headerArgs.back().append(": ");
			headerArgs.back().append(h->second);
		}
		for(std::vector<std::string>::iterator h(headerArgs.begin());h!=headerArgs.end();++h) {
			if (argPtr >= (1024 - 4)) // leave room for terminating NULL and URL
				break;
			curlArgs[argPtr++] = const_cast <char *>("-H");
			curlArgs[argPtr++] = const_cast <char *>(h->c_str());
		}
		curlArgs[argPtr++] = const_cast <char *>(_url.c_str());
		curlArgs[argPtr] = (char *)0;

		if (_cancelled) {
			delete this;
			return;
		}

		int curlStdout[2];
		int curlStderr[2];
		::pipe(curlStdout);
		::pipe(curlStderr);

		_pid = (long)vfork();
		if (_pid < 0) {
			// fork() failed
			::close(curlStdout[0]);
			::close(curlStdout[1]);
			::close(curlStderr[0]);
			::close(curlStderr[1]);
			_doH(_arg,-1,_url,"unable to fork()");
			delete this;
			return;
		} else if (_pid > 0) {
			// fork() succeeded, in parent process
			::close(curlStdout[1]);
			::close(curlStderr[1]);
			fcntl(curlStdout[0],F_SETFL,O_NONBLOCK);
			fcntl(curlStderr[0],F_SETFL,O_NONBLOCK);

			int exitCode = -1;
			unsigned long long timesOutAt = OSUtils::now() + ((unsigned long long)_timeout * 1000ULL);
			bool timedOut = false;
			bool tooLong = false;

			while (!_cancelled) {
				FD_ZERO(&readfds);
				FD_ZERO(&writefds);
				FD_ZERO(&errfds);
				FD_SET(curlStdout[0],&readfds);
				FD_SET(curlStderr[0],&readfds);
				FD_SET(curlStdout[0],&errfds);
				FD_SET(curlStderr[0],&errfds);
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				select(std::max(curlStdout[0],curlStderr[0])+1,&readfds,&writefds,&errfds,&tv);

				if (FD_ISSET(curlStdout[0],&readfds)) {
					int n = (int)::read(curlStdout[0],buf,sizeof(buf));
					if (n > 0) {
						_body.append(buf,n);
						// Reset timeout when data is read...
						timesOutAt = OSUtils::now() + ((unsigned long long)_timeout * 1000ULL);
					} else if (n < 0)
						break;
					if (_body.length() > CURL_MAX_MESSAGE_LENGTH) {
						tooLong = true;
						break;
					}
				}

				if (FD_ISSET(curlStderr[0],&readfds))
					::read(curlStderr[0],buf,sizeof(buf));

				if (FD_ISSET(curlStdout[0],&errfds)||FD_ISSET(curlStderr[0],&errfds))
					break;

				if (OSUtils::now() >= timesOutAt) {
					timedOut = true;
					break;
				}

				if (waitpid(_pid,&exitCode,WNOHANG) > 0) {
					for(;;) {
						// Drain output...
						int n = (int)::read(curlStdout[0],buf,sizeof(buf));
						if (n <= 0)
							break;
						else {
							_body.append(buf,n);
							if (_body.length() > CURL_MAX_MESSAGE_LENGTH) {
								tooLong = true;
								break;
							}
						}
					}
					_pid = 0;
					break;
				}
			}

			if (_pid > 0) {
				::kill(_pid,SIGKILL);
				waitpid(_pid,&exitCode,0);
			}
			_pid = 0;

			::close(curlStdout[0]);
			::close(curlStderr[0]);

			if (timedOut)
				_doH(_arg,-1,_url,"connection timed out");
			else if (tooLong)
				_doH(_arg,-1,_url,"response too long");
			else if (exitCode)
				_doH(_arg,-1,_url,"connection failed (curl returned non-zero exit code)");
			else {
				unsigned long idx = 0;

				// Grab status line and headers, which will prefix output on
				// success and will end with an empty line.
				std::vector<std::string> headers;
				headers.push_back(std::string());
				while (idx < _body.length()) {
					char c = _body[idx++];
					if (c == '\n') {
						if (!headers.back().length()) {
							headers.pop_back();
							break;
						} else headers.push_back(std::string());
					} else if (c != '\r')
						headers.back().push_back(c);
				}
				if (headers.empty()||(!headers.front().length())) {
					_doH(_arg,-1,_url,"HTTP response empty");
					delete this;
					return;
				}

				// Parse first line -- HTTP status code and response
				size_t scPos = headers.front().find(' ');
				if (scPos == std::string::npos) {
					_doH(_arg,-1,_url,"invalid HTTP response (no status line)");
					delete this;
					return;
				}
				++scPos;
				unsigned int rcode = OSUtils::strToUInt(headers.front().substr(scPos,3).c_str());
				if ((!rcode)||(rcode > 999)) {
					_doH(_arg,-1,_url,"invalid HTTP response (invalid response code)");
					delete this;
					return;
				}

				// Serve up the resulting data to the handler
				if (rcode == 200)
					_doH(_arg,rcode,_url,_body.substr(idx));
				else if ((scPos + 4) < headers.front().length())
					_doH(_arg,rcode,_url,headers.front().substr(scPos+4));
				else _doH(_arg,rcode,_url,"(no status message from server)");
			}

			delete this;
			return;
		} else {
			// fork() succeeded, in child process
			::dup2(curlStdout[1],STDOUT_FILENO);
			::close(curlStdout[1]);
			::dup2(curlStderr[1],STDERR_FILENO);
			::close(curlStderr[1]);
			::execv(curlPath.c_str(),curlArgs);
			::exit(-1); // only reached if execv() fails
		}
	}

	inline void cancel()
	{ 
		{
			Mutex::Lock _l(_cancelled_m);
			_cancelled = true;
			if (_pid > 0)
				::kill(_pid,SIGKILL);
		}
		Thread::join(_myThread);
	}

private:
	inline void _doH(void *arg,int code,const std::string &url,const std::string &body)
	{
		Mutex::Lock _l(_cancelled_m);
		try {
			if ((!_cancelled)&&(_handler))
				_handler(arg,code,url,body);
		} catch ( ... ) {}
	}

	const std::string _url;
	std::string _body;
	std::map<std::string,std::string> _headers;
	unsigned int _timeout;
	void (*_handler)(void *,int,const std::string &,const std::string &);
	void *_arg;
	HttpClient *_parent;
	long _pid;
	volatile bool _cancelled;
	Mutex _cancelled_m;
	Thread _myThread;
};

#endif // __UNIX_LIKE__

#ifdef __WINDOWS__

#define WIN_MAX_MESSAGE_LENGTH (1024 * 1024 * 64)

// Internal private thread class that performs request, notifies handler,
// and then commits suicide by deleting itself.
class HttpClient_Private_Request : NonCopyable
{
public:
	HttpClient_Private_Request(HttpClient *parent,const char *method,const std::string &url,const std::map<std::string,std::string> &headers,unsigned int timeout,void (*handler)(void *,int,const std::string &,const std::string &),void *arg) :
		_url(url),
		_headers(headers),
		_timeout(timeout),
		_handler(handler),
		_arg(arg),
		_parent(parent),
		_hRequest((HINTERNET)0)
	{
		_myThread = Thread::start(this);
	}

	~HttpClient_Private_Request()
	{
		Mutex::Lock _l(_parent->_requests_m);
		_parent->_requests.erase((HttpClient::Request)this);
	}

	void threadMain()
	{
		HINTERNET hSession = (HINTERNET)0;
		HINTERNET hConnect = (HINTERNET)0;
		HINTERNET hRequest = (HINTERNET)0;

		try {
			hSession = WinHttpOpen(L"ZeroTier One HttpClient/1.0 (WinHttp)",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
			if (!hSession) {
				_handler(_arg,-1,_url,"WinHttpOpen() failed");
				goto closeAndReturnFromHttp;
			}
			int timeoutMs = (int)_timeout * 1000;
			WinHttpSetTimeouts(hSession,timeoutMs,timeoutMs,timeoutMs,timeoutMs);

			std::wstring_convert< std::codecvt_utf8<wchar_t> > wcconv;
			std::wstring wurl(wcconv.from_bytes(_url));

			URL_COMPONENTS uc;
			memset(&uc,0,sizeof(uc));
			uc.dwStructSize = sizeof(uc);
			uc.dwSchemeLength = -1;
			uc.dwHostNameLength = -1;
			uc.dwUrlPathLength = -1;
			uc.dwExtraInfoLength = -1;
			if (!WinHttpCrackUrl(wurl.c_str(),(DWORD)wurl.length(),0,&uc)) {
				_handler(_arg,-1,_url,"unable to parse URL: WinHttpCrackUrl() failed");
				goto closeAndReturnFromHttp;
			}
			if ((!uc.lpszHostName)||(!uc.lpszUrlPath)||(!uc.lpszScheme)||(uc.dwHostNameLength <= 0)||(uc.dwUrlPathLength <= 0)||(uc.dwSchemeLength <= 0)) {
				_handler(_arg,-1,_url,"unable to parse URL: missing scheme, host name, or path");
				goto closeAndReturnFromHttp;
			}
			std::wstring urlScheme(uc.lpszScheme,uc.dwSchemeLength);
			std::wstring urlHostName(uc.lpszHostName,uc.dwHostNameLength);
			std::wstring urlPath(uc.lpszUrlPath,uc.dwUrlPathLength);
			if ((uc.lpszExtraInfo)&&(uc.dwExtraInfoLength > 0))
				urlPath.append(uc.lpszExtraInfo,uc.dwExtraInfoLength);

			if (urlScheme != L"http") {
				_handler(_arg,-1,_url,"only 'http' scheme is supported");
				goto closeAndReturnFromHttp;
			}

			hConnect = WinHttpConnect(hSession,urlHostName.c_str(),((uc.nPort > 0) ? uc.nPort : 80),0);
			if (!hConnect) {
				_handler(_arg,-1,_url,"connection failed");
				goto closeAndReturnFromHttp;
			}

			{
				Mutex::Lock _rl(_hRequest_m);
				_hRequest = WinHttpOpenRequest(hConnect,L"GET",urlPath.c_str(),NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);
				if (!_hRequest) {
					_handler(_arg,-1,_url,"error sending request (1)");
					goto closeAndReturnFromHttp;
				}
				if (!WinHttpSendRequest(_hRequest,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)) {
					_handler(_arg,-1,_url,"error sending request (2)");
					goto closeAndReturnFromHttp;
				}
				hRequest = _hRequest;
			}

			if (WinHttpReceiveResponse(hRequest,NULL)) {
				DWORD dwStatusCode = 0;
				DWORD dwTmp = sizeof(dwStatusCode);
				WinHttpQueryHeaders(hRequest,WINHTTP_QUERY_STATUS_CODE| WINHTTP_QUERY_FLAG_NUMBER,NULL,&dwStatusCode,&dwTmp,NULL);

				DWORD dwSize;
				do {
					dwSize = 0;
					if (!WinHttpQueryDataAvailable(hRequest,&dwSize)) {
						_handler(_arg,-1,_url,"receive error (1)");
						goto closeAndReturnFromHttp;
					}

					{
						Mutex::Lock _rl(_hRequest_m);
						if (!_hRequest) {
							_handler(_arg,-1,_url,"request cancelled");
							goto closeAndReturnFromHttp;
						}
					}

					char *outBuffer = new char[dwSize];
					DWORD dwRead = 0;
					if (!WinHttpReadData(hRequest,(LPVOID)outBuffer,dwSize,&dwRead)) {
						_handler(_arg,-1,_url,"receive error (2)");
						goto closeAndReturnFromHttp;
					}

					{
						Mutex::Lock _rl(_hRequest_m);
						if (!_hRequest) {
							_handler(_arg,-1,_url,"request cancelled");
							goto closeAndReturnFromHttp;
						}

						_body.append(outBuffer,dwRead);
						delete [] outBuffer;
						if (_body.length() > WIN_MAX_MESSAGE_LENGTH) {
							_handler(_arg,-1,_url,"result too large");
							goto closeAndReturnFromHttp;
						}
					}
				} while ((dwSize > 0)&&(_hRequest));

				{
					Mutex::Lock _rl(_hRequest_m);
					if (!_hRequest) {
						_handler(_arg,-1,_url,"request cancelled");
						goto closeAndReturnFromHttp;
					}

					_handler(_arg,dwStatusCode,_url,_body);
				}
			} else {
				_handler(_arg,-1,_url,"receive response failed");
			}
		} catch ( ... ) {
			_handler(_arg,-1,_url,"unexpected exception");
		}

closeAndReturnFromHttp:
		{
			Mutex::Lock _rl(_hRequest_m);
			if (_hRequest) {
				WinHttpCloseHandle(_hRequest);
				_hRequest = (HINTERNET)0;
			}
		}
		if (hConnect)
			WinHttpCloseHandle(hConnect);
		if (hSession)
			WinHttpCloseHandle(hSession);
		delete this;
		return;
	}

	inline void cancel()
	{
		Mutex::Lock _rl(_hRequest_m);
		if (_hRequest) {
			WinHttpCloseHandle(_hRequest);
			_hRequest = (HINTERNET)0;
		}
	}

	const std::string _url;
	std::string _body;
	std::map<std::string,std::string> _headers;
	unsigned int _timeout;
	void (*_handler)(void *,int,const std::string &,const std::string &);
	void *_arg;
	HttpClient *_parent;
	HINTERNET _hRequest;
	Mutex _hRequest_m;
	Thread _myThread;
};

#endif // __WINDOWS__

const std::map<std::string,std::string> HttpClient::NO_HEADERS;

HttpClient::HttpClient()
{
}

HttpClient::~HttpClient()
{
	std::set<Request> reqs;
	{
		Mutex::Lock _l(_requests_m);
		reqs = _requests;
	}

	for(std::set<Request>::iterator r(reqs.begin());r!=reqs.end();++r)
		this->cancel(*r);

	for(;;) {
		_requests_m.lock();
		if (_requests.empty()) {
			_requests_m.unlock();
			break;
		} else {
			_requests_m.unlock();
			Thread::sleep(250);
		}
	}
}

void HttpClient::cancel(HttpClient::Request req)
{
	Mutex::Lock _l(_requests_m);
	if (_requests.count(req) == 0)
		return;
	((HttpClient_Private_Request *)req)->cancel();
}

HttpClient::Request HttpClient::_do(
	const char *method,
	const std::string &url,
	const std::map<std::string,std::string> &headers,
	unsigned int timeout,
	void (*handler)(void *,int,const std::string &,const std::string &),
	void *arg)
{
	HttpClient::Request r = (HttpClient::Request)(new HttpClient_Private_Request(this,method,url,headers,timeout,handler,arg));
	Mutex::Lock _l(_requests_m);
	_requests.insert(r);
	return r;
}

} // namespace ZeroTier
