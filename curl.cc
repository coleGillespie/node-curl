#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <curl/curl.h>

#include "ev.h"
#include "node.h"
#include "node_buffer.h"

using namespace v8;
using namespace node;

namespace {

Persistent<ObjectTemplate> curlHandleTemplate;

class CurlHandle: public ObjectWrap {
public:
	static Handle<Object> New() {
		CurlHandle* const ch = new CurlHandle();

		// glue C++ object to a V8-managed JS object
		Local<Object> handle = curlHandleTemplate->NewInstance();
		handle->SetPointerInInternalField(1, reinterpret_cast<void*>(&curlHandleTemplate)); // magic cookie
		ch->Wrap(handle);

		return ch->handle_;
	}

	static bool IsInstanceOf(Handle<Value> val) {
		if (val->IsObject()) {
			Local<Object> o = val->ToObject();
			return o->InternalFieldCount() >= 2
				&& o->GetPointerFromInternalField(1) == reinterpret_cast<void*>(&curlHandleTemplate);
		}
		else {
			return false;
		}
	}

	static CurlHandle* Unwrap(Handle<Value> handle) {
		if (IsInstanceOf(handle)) {
			return ObjectWrap::Unwrap<CurlHandle>(handle->ToObject());
		}
		else {
			return NULL;
		}
	}

	~CurlHandle() {
		read_callback_.Dispose();
		write_callback_.Dispose();
		curl_easy_cleanup(ch_);
	}

	operator CURL*() {
		return ch_;
	}

	void SetWriteCallback(Handle<Value> callback) {
		Local<Function> fun = Local<Function>(Function::Cast(*callback));
		write_callback_.Clear();
		write_callback_ = Persistent<Function>::New(fun);
	}

	Handle<Value> InvokeWriteCallback(Buffer* data) {
		HandleScope scope;

		Local<Object> global = Context::GetCurrent()->Global();
		Handle<Value> args[] = { data->handle_ };
		Local<Value> rv = write_callback_->Call(global, 1, args);

		return scope.Close(rv);
	}

private:
	CURL* const ch_;
	Persistent<Function> read_callback_;
	Persistent<Function> write_callback_;

	CurlHandle(): ch_(curl_easy_init()) {
		if (ch_ == 0) {
			// raise OOM exception?
			V8::LowMemoryNotification();
		}
	}
};

Handle<Value> Error(const char* message) {
	return ThrowException(
		Exception::Error(
			String::New(message)));
}

Handle<Value> TypeError(const char* message) {
	return ThrowException(
		Exception::TypeError(
			String::New(message)));
}

Handle<Value> CurlError(CURLcode status) {
	return Error(curl_easy_strerror(status));
}

size_t WriteFunction(char* data, size_t size, size_t nmemb, void* arg) {
	CurlHandle* ch = reinterpret_cast<CurlHandle*>(arg);

	TryCatch tc;

	ch->InvokeWriteCallback(Buffer::New(data, size * nmemb));

	if (tc.HasCaught()) {
		FatalException(tc);
		return 0;
	}

	return nmemb;
}

Handle<Value> curl_easy_init_g(const Arguments& args) {
	return CurlHandle::New();
}

Handle<Value> curl_easy_setopt_g(const Arguments& args) {
	CURLcode status = CURLE_OK;

	if (!CurlHandle::IsInstanceOf(args[0])) {
		return TypeError("Argument #1 must be a node-curl handle.");
	}
	CurlHandle* ch = CurlHandle::Unwrap(args[0]);

	if (!args[1]->IsInt32()) {
		return TypeError("Argument #2 must be a CURL_* constant.");
	}
	const CURLoption option = (CURLoption) args[1]->Int32Value();

	switch (option) {
	case CURLOPT_URL:
		if (args[2]->IsString()) {
			String::Utf8Value s(args[2]);
			status = curl_easy_setopt(*ch, option, *s);
		}
		else {
			return TypeError("Argument #3 must be a string.");
		}
		break;

	case CURLOPT_ADDRESS_SCOPE:
	case CURLOPT_APPEND:
	case CURLOPT_AUTOREFERER:
	case CURLOPT_BUFFERSIZE:
	case CURLOPT_CERTINFO:
	case CURLOPT_CLOSEPOLICY:
	case CURLOPT_CONNECT_ONLY:
	case CURLOPT_CONNECTTIMEOUT:
	case CURLOPT_CONNECTTIMEOUT_MS:
	case CURLOPT_COOKIESESSION:
	case CURLOPT_CRLF:
	case CURLOPT_DIRLISTONLY:
	case CURLOPT_DNS_CACHE_TIMEOUT:
	case CURLOPT_DNS_USE_GLOBAL_CACHE:
	case CURLOPT_FAILONERROR:
	case CURLOPT_FILETIME:
	case CURLOPT_FOLLOWLOCATION:
	case CURLOPT_FORBID_REUSE:
	case CURLOPT_FRESH_CONNECT:
	case CURLOPT_FTP_CREATE_MISSING_DIRS:
	case CURLOPT_FTP_FILEMETHOD:
	case CURLOPT_FTP_RESPONSE_TIMEOUT:
	case CURLOPT_FTP_SKIP_PASV_IP:
	case CURLOPT_FTPSSLAUTH:
	case CURLOPT_FTP_SSL_CCC:
	case CURLOPT_FTP_USE_EPRT:
	case CURLOPT_FTP_USE_EPSV:
//	case CURLOPT_FTP_USE_PRET:
	case CURLOPT_HEADER:
	case CURLOPT_HTTPAUTH:
	case CURLOPT_HTTP_CONTENT_DECODING:
	case CURLOPT_HTTPGET:
	case CURLOPT_HTTPPROXYTUNNEL:
	case CURLOPT_HTTP_TRANSFER_DECODING:
	case CURLOPT_HTTP_VERSION:
	case CURLOPT_IGNORE_CONTENT_LENGTH:
	case CURLOPT_INFILESIZE:
	case CURLOPT_IPRESOLVE:
	case CURLOPT_LOCALPORT:
	case CURLOPT_LOCALPORTRANGE:
	case CURLOPT_LOW_SPEED_LIMIT:
	case CURLOPT_LOW_SPEED_TIME:
	case CURLOPT_MAXCONNECTS:
	case CURLOPT_MAXFILESIZE:
	case CURLOPT_MAXREDIRS:
	case CURLOPT_NETRC:
	case CURLOPT_NEW_DIRECTORY_PERMS:
	case CURLOPT_NEW_FILE_PERMS:
	case CURLOPT_NOBODY:
	case CURLOPT_NOPROGRESS:
	case CURLOPT_NOSIGNAL:
	case CURLOPT_PORT:
	case CURLOPT_POST:
	case CURLOPT_POSTFIELDSIZE:
	case CURLOPT_POSTREDIR:
	case CURLOPT_PROTOCOLS:
	case CURLOPT_PROXYAUTH:
	case CURLOPT_PROXYPORT:
	case CURLOPT_PROXY_TRANSFER_MODE:
	case CURLOPT_PROXYTYPE:
	case CURLOPT_PUT:
	case CURLOPT_REDIR_PROTOCOLS:
	case CURLOPT_RESUME_FROM:
//	case CURLOPT_RTSP_CLIENT_CSEQ:
//	case CURLOPT_RTSP_REQUEST:
//	case CURLOPT_RTSP_SERVER_CSEQ:
	case CURLOPT_SOCKS5_GSSAPI_NEC:
	case CURLOPT_SSH_AUTH_TYPES:
	case CURLOPT_SSLENGINE_DEFAULT:
	case CURLOPT_SSL_SESSIONID_CACHE:
	case CURLOPT_SSL_VERIFYHOST:
	case CURLOPT_SSL_VERIFYPEER:
	case CURLOPT_SSLVERSION:
	case CURLOPT_TCP_NODELAY:
	case CURLOPT_TFTP_BLKSIZE:
	case CURLOPT_TIMECONDITION:
	case CURLOPT_TIMEOUT:
	case CURLOPT_TIMEOUT_MS:
	case CURLOPT_TIMEVALUE:
//	case CURLOPT_TRANSFER_ENCODING:
	case CURLOPT_TRANSFERTEXT:
	case CURLOPT_UNRESTRICTED_AUTH:
	case CURLOPT_UPLOAD:
	case CURLOPT_USE_SSL:
	case CURLOPT_VERBOSE:
//	case CURLOPT_WILDCARDMATCH:
		if (args[2]->IsInt32()) { // special-case booleans? think CURLOPT_VERBOSE
			const long val = args[2]->Int32Value();
			status = curl_easy_setopt(*ch, option, val);
		}
		else {
			return TypeError("Argument #3 must be an integer.");
		}
		break;

	case CURLOPT_WRITEFUNCTION:
		if (args[2]->IsFunction()) {
			ch->SetWriteCallback(args[2]);
			curl_easy_setopt(*ch, CURLOPT_WRITEFUNCTION, WriteFunction);
			curl_easy_setopt(*ch, CURLOPT_WRITEDATA, ch);
		}
		else {
			return TypeError("Argument #3 must be a function.");
		}
		break;

	default:
		return TypeError("Argument #3 must be a CURLOPT_* constant.");
	}

	if (status != CURLE_OK) {
		// TODO throw a {code, message} exception
		return CurlError(status);
	}

	return Undefined();
}

unsigned SListSize(const curl_slist* slist) {
	unsigned entries = 0;

	for (; slist != NULL; ++entries, slist = slist->next);

	return entries;
}

Handle<Array> SListToArray(const curl_slist* slist) {
	Local<Array> array = Array::New(SListSize(slist));

	for (unsigned i = 0; slist != NULL; ++i, slist = slist->next) {
		array->Set(i, String::New(slist->data));
	}

	return array;
}

Handle<Array> CertInfoToArray(const struct curl_certinfo* certinfo) {
	Local<Array> array = Array::New(certinfo->num_of_certs);

	for (int i = 0; i < certinfo->num_of_certs; ++i) {
		array->Set(i, SListToArray(certinfo->certinfo[i]));
	}

	return array;
}

Handle<Value> curl_easy_getinfo_g(const Arguments& args) {
	if (!CurlHandle::IsInstanceOf(args[0])) {
		return TypeError("Argument #1 must be a node-curl handle.");
	}
	CurlHandle* ch = CurlHandle::Unwrap(args[0]);

	if (!args[1]->IsInt32()) {
		return TypeError("Argument #2 must be a CURLINFO_* constant.");
	}
	const CURLINFO option = (CURLINFO) args[1]->Int32Value();

	union {
		const struct curl_certinfo* certinfo_;
		const curl_slist* slist_;
		const char* string_;
		double double_;
		long long_;
	} value;

	CURLcode status = curl_easy_getinfo(*ch, option, &value);
	if (status != CURLE_OK) {
		return CurlError(status);
	}

	Handle<Value> rv;

	switch (option) {
	case CURLINFO_CONDITION_UNMET:
	case CURLINFO_FILETIME:
	case CURLINFO_HEADER_SIZE:
	case CURLINFO_HTTPAUTH_AVAIL:
	case CURLINFO_HTTP_CONNECTCODE:
	case CURLINFO_LASTSOCKET:
//	case CURLINFO_LOCAL_PORT:
	case CURLINFO_NUM_CONNECTS:
	case CURLINFO_OS_ERRNO:
//	case CURLINFO_PRIMARY_PORT:
	case CURLINFO_PROXYAUTH_AVAIL:
	case CURLINFO_REDIRECT_COUNT:
	case CURLINFO_REQUEST_SIZE:
	case CURLINFO_RESPONSE_CODE:
//	case CURLINFO_RTSP_CLIENT_CSEQ:
//	case CURLINFO_RTSP_CSEQ_RECV:
//	case CURLINFO_RTSP_SERVER_CSEQ:
	case CURLINFO_SSL_VERIFYRESULT:
		rv = Integer::New(value.long_);
		break;

	case CURLINFO_APPCONNECT_TIME:
	case CURLINFO_CONNECT_TIME:
	case CURLINFO_CONTENT_LENGTH_DOWNLOAD:
	case CURLINFO_CONTENT_LENGTH_UPLOAD:
	case CURLINFO_NAMELOOKUP_TIME:
	case CURLINFO_PRETRANSFER_TIME:
	case CURLINFO_REDIRECT_TIME:
	case CURLINFO_SIZE_DOWNLOAD:
	case CURLINFO_SIZE_UPLOAD:
	case CURLINFO_SPEED_DOWNLOAD:
	case CURLINFO_SPEED_UPLOAD:
	case CURLINFO_STARTTRANSFER_TIME:
	case CURLINFO_TOTAL_TIME:
		rv = Number::New(value.double_);
		break;

	case CURLINFO_CONTENT_TYPE:
	case CURLINFO_EFFECTIVE_URL:
	case CURLINFO_FTP_ENTRY_PATH:
//	case CURLINFO_LOCAL_IP:
	case CURLINFO_PRIMARY_IP:
	case CURLINFO_PRIVATE:
	case CURLINFO_REDIRECT_URL:
//	case CURLINFO_RTSP_SESSION_ID:
		if (value.string_) {
			rv = String::New(value.string_);
		}
		else {
			rv = String::Empty();
		}
		break;

	case CURLINFO_COOKIELIST:
	case CURLINFO_SSL_ENGINES:
		rv = SListToArray(value.slist_);
		break;

	case CURLINFO_CERTINFO:
		rv = CertInfoToArray(value.certinfo_);
		break;

	default:
		return TypeError("Argument #2 must be a CURLINFO_* constant.");
	}

	return rv;
}

Handle<Value> curl_easy_perform_g(const Arguments& args) {
	if (!CurlHandle::IsInstanceOf(args[0])) {
		return TypeError("Argument #1 must be a node-curl handle.");
	}
	CurlHandle* ch = CurlHandle::Unwrap(args[0]);

	CURLcode status = curl_easy_perform(*ch);
	if (status != CURLE_OK) {
		// TODO throw a {code, message} exception
		return CurlError(status);
	}

	return Undefined();
}

void RegisterModule(Handle<Object> target) {
	curlHandleTemplate = Persistent<ObjectTemplate>::New(ObjectTemplate::New());
	curlHandleTemplate->SetInternalFieldCount(2);

	CURLcode status = curl_global_init(CURL_GLOBAL_ALL);
	if (status != CURLE_OK) {
		CurlError(status); // raises an exception
		return;
	}
	atexit(curl_global_cleanup);

	target->Set(
		String::NewSymbol("curl_easy_init"),
		FunctionTemplate::New(curl_easy_init_g)->GetFunction());
	target->Set(
		String::NewSymbol("curl_easy_setopt"),
		FunctionTemplate::New(curl_easy_setopt_g)->GetFunction());
	target->Set(
		String::NewSymbol("curl_easy_perform"),
		FunctionTemplate::New(curl_easy_perform_g)->GetFunction());
	target->Set(
		String::NewSymbol("curl_easy_getinfo"),
		FunctionTemplate::New(curl_easy_getinfo_g)->GetFunction());

#define EXPORT(symbol) target->Set(String::NewSymbol(#symbol), Integer::New(symbol))
//	EXPORT(CURLOPT_ACCEPT_ENCODING);
	EXPORT(CURLOPT_ADDRESS_SCOPE);
	EXPORT(CURLOPT_APPEND);
	EXPORT(CURLOPT_AUTOREFERER);
	EXPORT(CURLOPT_BUFFERSIZE);
	EXPORT(CURLOPT_CAINFO);
	EXPORT(CURLOPT_CAPATH);
	EXPORT(CURLOPT_CERTINFO);
//	EXPORT(CURLOPT_CHUNK_BGN_FUNCTION);
//	EXPORT(CURLOPT_CHUNK_DATA);
//	EXPORT(CURLOPT_CHUNK_END_FUNCTION);
	EXPORT(CURLOPT_CLOSEPOLICY);
	EXPORT(CURLOPT_CONNECT_ONLY);
	EXPORT(CURLOPT_CONNECTTIMEOUT);
	EXPORT(CURLOPT_CONNECTTIMEOUT_MS);
	EXPORT(CURLOPT_CONV_FROM_NETWORK_FUNCTION);
	EXPORT(CURLOPT_CONV_FROM_UTF8_FUNCTION);
	EXPORT(CURLOPT_CONV_TO_NETWORK_FUNCTION);
	EXPORT(CURLOPT_COOKIE);
	EXPORT(CURLOPT_COOKIEFILE);
	EXPORT(CURLOPT_COOKIEJAR);
	EXPORT(CURLOPT_COOKIELIST);
	EXPORT(CURLOPT_COOKIESESSION);
	EXPORT(CURLOPT_COPYPOSTFIELDS);
	EXPORT(CURLOPT_CRLF);
	EXPORT(CURLOPT_CRLFILE);
	EXPORT(CURLOPT_CUSTOMREQUEST);
	EXPORT(CURLOPT_DEBUGDATA);
	EXPORT(CURLOPT_DEBUGFUNCTION);
	EXPORT(CURLOPT_DIRLISTONLY);
	EXPORT(CURLOPT_DNS_CACHE_TIMEOUT);
	EXPORT(CURLOPT_DNS_USE_GLOBAL_CACHE);
	EXPORT(CURLOPT_EGDSOCKET);
	EXPORT(CURLOPT_ERRORBUFFER);
	EXPORT(CURLOPT_FAILONERROR);
	EXPORT(CURLOPT_FILE);
	EXPORT(CURLOPT_FILETIME);
//	EXPORT(CURLOPT_FNMATCH_DATA);
//	EXPORT(CURLOPT_FNMATCH_FUNCTION);
	EXPORT(CURLOPT_FOLLOWLOCATION);
	EXPORT(CURLOPT_FORBID_REUSE);
	EXPORT(CURLOPT_FRESH_CONNECT);
	EXPORT(CURLOPT_FTP_ACCOUNT);
	EXPORT(CURLOPT_FTP_ALTERNATIVE_TO_USER);
	EXPORT(CURLOPT_FTP_CREATE_MISSING_DIRS);
	EXPORT(CURLOPT_FTP_FILEMETHOD);
	EXPORT(CURLOPT_FTPPORT);
	EXPORT(CURLOPT_FTP_RESPONSE_TIMEOUT);
	EXPORT(CURLOPT_FTP_SKIP_PASV_IP);
	EXPORT(CURLOPT_FTPSSLAUTH);
	EXPORT(CURLOPT_FTP_SSL_CCC);
	EXPORT(CURLOPT_FTP_USE_EPRT);
	EXPORT(CURLOPT_FTP_USE_EPSV);
//	EXPORT(CURLOPT_FTP_USE_PRET);
	EXPORT(CURLOPT_HEADER);
	EXPORT(CURLOPT_HEADERFUNCTION);
	EXPORT(CURLOPT_HTTP200ALIASES);
	EXPORT(CURLOPT_HTTPAUTH);
	EXPORT(CURLOPT_HTTP_CONTENT_DECODING);
	EXPORT(CURLOPT_HTTPGET);
	EXPORT(CURLOPT_HTTPHEADER);
	EXPORT(CURLOPT_HTTPPOST);
	EXPORT(CURLOPT_HTTPPROXYTUNNEL);
	EXPORT(CURLOPT_HTTP_TRANSFER_DECODING);
	EXPORT(CURLOPT_HTTP_VERSION);
	EXPORT(CURLOPT_IGNORE_CONTENT_LENGTH);
	EXPORT(CURLOPT_INFILE);
	EXPORT(CURLOPT_INFILESIZE);
	EXPORT(CURLOPT_INFILESIZE_LARGE);
	EXPORT(CURLOPT_INTERFACE);
//	EXPORT(CURLOPT_INTERLEAVEDATA);
//	EXPORT(CURLOPT_INTERLEAVEFUNCTION);
	EXPORT(CURLOPT_IOCTLDATA);
	EXPORT(CURLOPT_IOCTLFUNCTION);
	EXPORT(CURLOPT_IPRESOLVE);
	EXPORT(CURLOPT_ISSUERCERT);
	EXPORT(CURLOPT_KEYPASSWD);
	EXPORT(CURLOPT_KRBLEVEL);
	EXPORT(CURLOPT_LOCALPORT);
	EXPORT(CURLOPT_LOCALPORTRANGE);
	EXPORT(CURLOPT_LOW_SPEED_LIMIT);
	EXPORT(CURLOPT_LOW_SPEED_TIME);
//	EXPORT(CURLOPT_MAIL_FROM);
//	EXPORT(CURLOPT_MAIL_RCPT);
	EXPORT(CURLOPT_MAXCONNECTS);
	EXPORT(CURLOPT_MAXFILESIZE);
	EXPORT(CURLOPT_MAXFILESIZE_LARGE);
	EXPORT(CURLOPT_MAX_RECV_SPEED_LARGE);
	EXPORT(CURLOPT_MAXREDIRS);
	EXPORT(CURLOPT_MAX_SEND_SPEED_LARGE);
	EXPORT(CURLOPT_NETRC);
	EXPORT(CURLOPT_NETRC_FILE);
	EXPORT(CURLOPT_NEW_DIRECTORY_PERMS);
	EXPORT(CURLOPT_NEW_FILE_PERMS);
	EXPORT(CURLOPT_NOBODY);
	EXPORT(CURLOPT_NOPROGRESS);
	EXPORT(CURLOPT_NOPROXY);
	EXPORT(CURLOPT_NOSIGNAL);
	EXPORT(CURLOPT_OPENSOCKETDATA);
	EXPORT(CURLOPT_OPENSOCKETFUNCTION);
	EXPORT(CURLOPT_PASSWORD);
	EXPORT(CURLOPT_PORT);
	EXPORT(CURLOPT_POST);
	EXPORT(CURLOPT_POSTFIELDS);
	EXPORT(CURLOPT_POSTFIELDSIZE);
	EXPORT(CURLOPT_POSTFIELDSIZE_LARGE);
	EXPORT(CURLOPT_POSTQUOTE);
	EXPORT(CURLOPT_POSTREDIR);
	EXPORT(CURLOPT_PREQUOTE);
	EXPORT(CURLOPT_PRIVATE);
	EXPORT(CURLOPT_PROGRESSDATA);
	EXPORT(CURLOPT_PROGRESSFUNCTION);
	EXPORT(CURLOPT_PROTOCOLS);
	EXPORT(CURLOPT_PROXY);
	EXPORT(CURLOPT_PROXYAUTH);
	EXPORT(CURLOPT_PROXYPASSWORD);
	EXPORT(CURLOPT_PROXYPORT);
	EXPORT(CURLOPT_PROXY_TRANSFER_MODE);
	EXPORT(CURLOPT_PROXYTYPE);
	EXPORT(CURLOPT_PROXYUSERNAME);
	EXPORT(CURLOPT_PROXYUSERPWD);
	EXPORT(CURLOPT_PUT);
	EXPORT(CURLOPT_QUOTE);
	EXPORT(CURLOPT_RANDOM_FILE);
	EXPORT(CURLOPT_RANGE);
	EXPORT(CURLOPT_READFUNCTION);
	EXPORT(CURLOPT_REDIR_PROTOCOLS);
	EXPORT(CURLOPT_REFERER);
//	EXPORT(CURLOPT_RESOLVE);
	EXPORT(CURLOPT_RESUME_FROM);
	EXPORT(CURLOPT_RESUME_FROM_LARGE);
//	EXPORT(CURLOPT_RTSP_CLIENT_CSEQ);
//	EXPORT(CURLOPT_RTSP_REQUEST);
//	EXPORT(CURLOPT_RTSP_SERVER_CSEQ);
//	EXPORT(CURLOPT_RTSP_SESSION_ID);
//	EXPORT(CURLOPT_RTSP_STREAM_URI);
//	EXPORT(CURLOPT_RTSP_TRANSPORT);
	EXPORT(CURLOPT_SEEKDATA);
	EXPORT(CURLOPT_SEEKFUNCTION);
	EXPORT(CURLOPT_SHARE);
	EXPORT(CURLOPT_SOCKOPTDATA);
	EXPORT(CURLOPT_SOCKOPTFUNCTION);
	EXPORT(CURLOPT_SOCKS5_GSSAPI_NEC);
	EXPORT(CURLOPT_SOCKS5_GSSAPI_SERVICE);
	EXPORT(CURLOPT_SSH_AUTH_TYPES);
	EXPORT(CURLOPT_SSH_HOST_PUBLIC_KEY_MD5);
	EXPORT(CURLOPT_SSH_KEYDATA);
	EXPORT(CURLOPT_SSH_KEYFUNCTION);
	EXPORT(CURLOPT_SSH_KNOWNHOSTS);
	EXPORT(CURLOPT_SSH_PRIVATE_KEYFILE);
	EXPORT(CURLOPT_SSH_PUBLIC_KEYFILE);
	EXPORT(CURLOPT_SSLCERT);
	EXPORT(CURLOPT_SSLCERTTYPE);
	EXPORT(CURLOPT_SSL_CIPHER_LIST);
	EXPORT(CURLOPT_SSL_CTX_DATA);
	EXPORT(CURLOPT_SSL_CTX_FUNCTION);
	EXPORT(CURLOPT_SSLENGINE);
	EXPORT(CURLOPT_SSLENGINE_DEFAULT);
	EXPORT(CURLOPT_SSLKEY);
	EXPORT(CURLOPT_SSLKEYTYPE);
	EXPORT(CURLOPT_SSL_SESSIONID_CACHE);
	EXPORT(CURLOPT_SSL_VERIFYHOST);
	EXPORT(CURLOPT_SSL_VERIFYPEER);
	EXPORT(CURLOPT_SSLVERSION);
	EXPORT(CURLOPT_STDERR);
	EXPORT(CURLOPT_TCP_NODELAY);
	EXPORT(CURLOPT_TELNETOPTIONS);
	EXPORT(CURLOPT_TFTP_BLKSIZE);
	EXPORT(CURLOPT_TIMECONDITION);
	EXPORT(CURLOPT_TIMEOUT);
	EXPORT(CURLOPT_TIMEOUT_MS);
	EXPORT(CURLOPT_TIMEVALUE);
//	EXPORT(CURLOPT_TLSAUTH_PASSWORD);
//	EXPORT(CURLOPT_TLSAUTH_TYPE);
//	EXPORT(CURLOPT_TLSAUTH_USERNAME);
//	EXPORT(CURLOPT_TRANSFER_ENCODING);
	EXPORT(CURLOPT_TRANSFERTEXT);
	EXPORT(CURLOPT_UNRESTRICTED_AUTH);
	EXPORT(CURLOPT_UPLOAD);
	EXPORT(CURLOPT_URL);
	EXPORT(CURLOPT_USERAGENT);
	EXPORT(CURLOPT_USERNAME);
	EXPORT(CURLOPT_USERPWD);
	EXPORT(CURLOPT_USE_SSL);
	EXPORT(CURLOPT_VERBOSE);
//	EXPORT(CURLOPT_WILDCARDMATCH);
	EXPORT(CURLOPT_WRITEFUNCTION);
	EXPORT(CURLOPT_WRITEHEADER);
	EXPORT(CURLOPT_WRITEINFO);

	EXPORT(CURLINFO_APPCONNECT_TIME);
	EXPORT(CURLINFO_CERTINFO);
	EXPORT(CURLINFO_CERTINFO);
	EXPORT(CURLINFO_CONDITION_UNMET);
	EXPORT(CURLINFO_CONNECT_TIME);
	EXPORT(CURLINFO_CONTENT_LENGTH_DOWNLOAD);
	EXPORT(CURLINFO_CONTENT_LENGTH_UPLOAD);
	EXPORT(CURLINFO_CONTENT_TYPE);
	EXPORT(CURLINFO_COOKIELIST);
//	EXPORT(CURLINFO_DATA_IN);
//	EXPORT(CURLINFO_DATA_OUT);
	EXPORT(CURLINFO_EFFECTIVE_URL);
//	EXPORT(CURLINFO_END);
	EXPORT(CURLINFO_FILETIME);
	EXPORT(CURLINFO_FTP_ENTRY_PATH);
//	EXPORT(CURLINFO_HEADER_IN);
//	EXPORT(CURLINFO_HEADER_OUT);
	EXPORT(CURLINFO_HEADER_SIZE);
	EXPORT(CURLINFO_HTTPAUTH_AVAIL);
	EXPORT(CURLINFO_HTTP_CONNECTCODE);
//	EXPORT(CURLINFO_LASTONE);
	EXPORT(CURLINFO_LASTSOCKET);
//	EXPORT(CURLINFO_LOCAL_IP);
//	EXPORT(CURLINFO_LOCAL_PORT);
	EXPORT(CURLINFO_NAMELOOKUP_TIME);
	EXPORT(CURLINFO_NUM_CONNECTS);
	EXPORT(CURLINFO_OS_ERRNO);
	EXPORT(CURLINFO_PRETRANSFER_TIME);
	EXPORT(CURLINFO_PRIMARY_IP);
//	EXPORT(CURLINFO_PRIMARY_PORT);
	EXPORT(CURLINFO_PRIVATE);
	EXPORT(CURLINFO_PROXYAUTH_AVAIL);
	EXPORT(CURLINFO_REDIRECT_COUNT);
	EXPORT(CURLINFO_REDIRECT_TIME);
	EXPORT(CURLINFO_REDIRECT_URL);
	EXPORT(CURLINFO_REQUEST_SIZE);
	EXPORT(CURLINFO_RESPONSE_CODE);
//	EXPORT(CURLINFO_RTSP_CLIENT_CSEQ);
//	EXPORT(CURLINFO_RTSP_CSEQ_RECV);
//	EXPORT(CURLINFO_RTSP_SERVER_CSEQ);
//	EXPORT(CURLINFO_RTSP_SESSION_ID);
	EXPORT(CURLINFO_SIZE_DOWNLOAD);
	EXPORT(CURLINFO_SIZE_UPLOAD);
	EXPORT(CURLINFO_SPEED_DOWNLOAD);
	EXPORT(CURLINFO_SPEED_UPLOAD);
//	EXPORT(CURLINFO_SSL_DATA_IN);
//	EXPORT(CURLINFO_SSL_DATA_OUT);
	EXPORT(CURLINFO_SSL_ENGINES);
	EXPORT(CURLINFO_SSL_VERIFYRESULT);
	EXPORT(CURLINFO_STARTTRANSFER_TIME);
//	EXPORT(CURLINFO_TEXT);
	EXPORT(CURLINFO_TOTAL_TIME);
#undef EXPORT
}

} // namespace

NODE_MODULE(curl, RegisterModule);
