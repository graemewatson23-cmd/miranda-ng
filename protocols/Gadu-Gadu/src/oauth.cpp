/////////////////////////////////////////////////////////////////////////////////////////
// Gadu-Gadu Plugin for Miranda IM
//
// Copyright (c) 2010 Bartosz Białek
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "gg.h"
#include <io.h>
#include <fcntl.h>
#include "protocol.h"

/////////////////////////////////////////////////////////////////////////////////////////
// OAuth 1.0 implementation

// Service Provider must accept the HTTP Authorization header

// RSA-SHA1 signature method (see RFC 3447 section 8.2
// and RSASSA-PKCS1-v1_5 algorithm) is unimplemented

struct OAUTHPARAMETER
{
	char *name;
	char *value;
};

enum OAUTHSIGNMETHOD
{
	HMACSHA1,
	RSASHA1,
	PLAINTEXT
};

static int paramsortFunc(const OAUTHPARAMETER *p1, const OAUTHPARAMETER *p2)
{
	int res = mir_strcmp(p1->name, p2->name);
	return res != 0 ? res : mir_strcmp(p1->value, p2->value);
}

// see RFC 3986 for details
#define isunreserved(c) ( isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_' || c == '~')
char *oauth_uri_escape(const char *str)
{
	int ix = 0;

	if (str == nullptr) return mir_strdup("");

	int size = (int)mir_strlen(str) + 1;
	char *res = (char *)mir_alloc(size);

	while (*str) {
		if (!isunreserved(*str)) {
			size += 2;
			res = (char *)mir_realloc(res, size);
			mir_snprintf(&res[ix], 4, "%%%X%X", (*str >> 4) & 15, *str & 15);
			ix += 3;
		}
		else
			res[ix++] = *str;
		str++;
	}
	res[ix] = 0;

	return res;
}

// generates Signature Base String

char *oauth_generate_signature(LIST<OAUTHPARAMETER> &params, const char *httpmethod, const char *url)
{
	char *res;
	int ix = 0;

	if (httpmethod == nullptr || url == nullptr || !params.getCount()) return mir_strdup("");

	char *urlnorm = (char *)mir_alloc(mir_strlen(url) + 1);
	while (*url) {
		if (*url == '?' || *url == '#')	break; // see RFC 3986 section 3
		urlnorm[ix++] = tolower(*url);
		url++;
	}
	urlnorm[ix] = 0;
	if ((res = strstr(urlnorm, ":80")) != nullptr)
		memmove(res, res + 3, mir_strlen(res) - 2);
	else if ((res = strstr(urlnorm, ":443")) != nullptr)
		memmove(res, res + 4, mir_strlen(res) - 3);

	char *urlenc = oauth_uri_escape(urlnorm);
	mir_free(urlnorm);
	int size = (int)mir_strlen(httpmethod) + (int)mir_strlen(urlenc) + 1 + 2;

	for (auto &p : params) {
		if (!mir_strcmp(p->name, "oauth_signature")) continue;
		if (p != params[0]) size += 3;
		size += (int)mir_strlen(p->name) + (int)mir_strlen(p->value) + 3;
	}

	res = (char *)mir_alloc(size);
	mir_strcpy(res, httpmethod);
	mir_strcat(res, "&");
	mir_strcat(res, urlenc);
	mir_free(urlenc);
	mir_strcat(res, "&");

	for (auto &p : params) {
		if (!mir_strcmp(p->name, "oauth_signature")) continue;
		if (p != params[0]) mir_strcat(res, "%26");
		mir_strcat(res, p->name);
		mir_strcat(res, "%3D");
		mir_strcat(res, p->value);
	}

	return res;
}

char *oauth_getparam(LIST<OAUTHPARAMETER> &params, const char *name)
{
	if (name == nullptr)
		return nullptr;

	for (auto &p : params)
		if (!mir_strcmp(p->name, name))
			return p->value;

	return nullptr;
}

void oauth_setparam(LIST<OAUTHPARAMETER> &params, const char *name, const char *value)
{
	if (name == nullptr)
		return;

	for (auto &p : params)
		if (!mir_strcmp(p->name, name)) {
			mir_free(p->value);
			p->value = oauth_uri_escape(value);
			return;
		}

	OAUTHPARAMETER *p = (OAUTHPARAMETER*)mir_alloc(sizeof(OAUTHPARAMETER));
	p->name = oauth_uri_escape(name);
	p->value = oauth_uri_escape(value);
	params.insert(p);
}

void oauth_freeparams(LIST<OAUTHPARAMETER> &params)
{
	for (auto &p : params) {
		mir_free(p->name);
		mir_free(p->value);
	}
}

int oauth_sign_request(LIST<OAUTHPARAMETER> &params, const char *httpmethod, const char *url,
	const char *consumer_secret, const char *token_secret)
{
	char *sign = nullptr;

	if (!params.getCount())
		return -1;

	char *signmethod = oauth_getparam(params, "oauth_signature_method");
	if (signmethod == nullptr)
		return -1;

	if (!mir_strcmp(signmethod, "HMAC-SHA1")) {
		ptrA text(oauth_generate_signature(params, httpmethod, url));
		ptrA csenc(oauth_uri_escape(consumer_secret));
		ptrA tsenc(oauth_uri_escape(token_secret));
		ptrA key((char *)mir_alloc(mir_strlen(csenc) + mir_strlen(tsenc) + 2));
		mir_strcpy(key, csenc);
		mir_strcat(key, "&");
		mir_strcat(key, tsenc);

		uint8_t digest[MIR_SHA1_HASH_SIZE];
		unsigned len;
		HMAC(EVP_sha1(), key, (int)mir_strlen(key), (uint8_t*)(char*)text, (int)mir_strlen(text), digest, &len);
		sign = mir_base64_encode(digest, MIR_SHA1_HASH_SIZE);
	}
	else { // PLAINTEXT
		ptrA csenc(oauth_uri_escape(consumer_secret));
		ptrA tsenc(oauth_uri_escape(token_secret));

		sign = (char *)mir_alloc(mir_strlen(csenc) + mir_strlen(tsenc) + 2);
		mir_strcpy(sign, csenc);
		mir_strcat(sign, "&");
		mir_strcat(sign, tsenc);
	}

	oauth_setparam(params, "oauth_signature", sign);
	mir_free(sign);

	return 0;
}

char* oauth_generate_nonce()
{
	char randnum[16];
	Utils_GetRandom(randnum, sizeof(randnum));

	CMStringA str(FORMAT, "%ld%s", time(0), randnum);

	uint8_t digest[16];
	mir_md5_hash((uint8_t*)str.GetString(), str.GetLength(), digest);

	return bin2hex(digest, sizeof(digest), (char *)mir_alloc(32 + 1));
}

char *oauth_auth_header(const char *httpmethod, const char *url, OAUTHSIGNMETHOD signmethod,
	const char *consumer_key, const char *consumer_secret,
	const char *token, const char *token_secret)
{
	if (httpmethod == nullptr || url == nullptr)
		return nullptr;

	LIST<OAUTHPARAMETER> oauth_parameters(1, paramsortFunc);
	oauth_setparam(oauth_parameters, "oauth_consumer_key", consumer_key);
	oauth_setparam(oauth_parameters, "oauth_version", "1.0");
	switch (signmethod) {
	case HMACSHA1:
		oauth_setparam(oauth_parameters, "oauth_signature_method", "HMAC-SHA1");
		break;

	case RSASHA1:
		oauth_setparam(oauth_parameters, "oauth_signature_method", "RSA-SHA1");
		break;

	default:
		oauth_setparam(oauth_parameters, "oauth_signature_method", "PLAINTEXT");
		break;
	};

	char timestamp[22];
	mir_snprintf(timestamp, "%ld", time(0));
	oauth_setparam(oauth_parameters, "oauth_timestamp", timestamp);
	oauth_setparam(oauth_parameters, "oauth_nonce", ptrA(oauth_generate_nonce()));
	if (token != nullptr && *token)
		oauth_setparam(oauth_parameters, "oauth_token", token);

	if (oauth_sign_request(oauth_parameters, httpmethod, url, consumer_secret, token_secret)) {
		oauth_freeparams(oauth_parameters);
		return nullptr;
	}

	CMStringA res("OAuth ");
	for (auto &p : oauth_parameters) {
		if (res.GetLength() > 6)
			res.AppendChar(',');
		res.Append(p->name);
		res.Append("=\"");
		res.Append(p->value);
		res.Append("\"");
	}

	oauth_freeparams(oauth_parameters);
	return res.Detach();
}

int GaduProto::oauth_receivetoken()
{
	char uin[32], *token = nullptr, *token_secret = nullptr;
	int res = 0;
	HNETLIBCONN nlc = nullptr;

	UIN2IDA(getDword(GG_KEY_UIN, 0), uin);
	char *password = getStringA(GG_KEY_PASSWORD);

	// 1. Obtaining an Unauthorized Request Token
	debugLogA("oauth_receivetoken(): Obtaining an Unauthorized Request Token...");

	NLHR_PTR resp(0);
	{
		MHttpRequest req(REQUEST_POST);
		req.m_szUrl = "http://api.gadu-gadu.pl/request_token";
		req.flags = NLHRF_NODUMP | NLHRF_HTTP11 | NLHRF_PERSISTENT;
		req.AddHeader("User-Agent", GG8_VERSION);
		req.AddHeader("Authorization", ptrA(oauth_auth_header("POST", req.m_szUrl, HMACSHA1, uin, password, nullptr, nullptr)));
		req.AddHeader("Accept", "*/*");

		resp = Netlib_HttpTransaction(m_hNetlibUser, &req);
		if (resp) {
			nlc = resp->nlc;
			if (resp->resultCode == 200 && !resp->body.IsEmpty()) {
				TiXmlDocument doc;
				if (0 == doc.Parse(resp->body)) {
					TiXmlConst hXml(doc.FirstChildElement("result"));
					if (auto *p = hXml["oauth_token"].ToElement())
						token = mir_strdup(p->GetText());

					if (auto *p = hXml["oauth_token_secret"].ToElement())
						token_secret = mir_strdup(p->GetText());
				}
			}
			else debugLogA("oauth_receivetoken(): Invalid response code from HTTP request");
		}
		else debugLogA("oauth_receivetoken(): No response from HTTP request");
	}

	// 2. Obtaining User Authorization
	debugLogA("oauth_receivetoken(): Obtaining User Authorization...");
	{
		MHttpRequest req(REQUEST_POST);
		req.m_szUrl = "https://login.gadu-gadu.pl/authorize";
		req.flags = NLHRF_NODUMP | NLHRF_HTTP11;
		req.m_szParam.Format("callback_url=%s&request_token=%s&uin=%s&password=%s", ptrA(oauth_uri_escape("http://www.mojageneracja.pl")), token, uin, password);
		req.AddHeader("User-Agent", GG8_VERSION);
		req.AddHeader("Content-Type", "application/x-www-form-urlencoded");
		req.AddHeader("Accept", "*/*");

		resp = Netlib_HttpTransaction(m_hNetlibUser, &req);
		if (!resp)
			debugLogA("oauth_receivetoken(): No response from HTTP request");
	}

	// 3. Obtaining an Access Token
	debugLogA("oauth_receivetoken(): Obtaining an Access Token...");

	mir_free(token);
	mir_free(token_secret);
	token = nullptr;
	token_secret = nullptr;
	{
		MHttpRequest req(REQUEST_POST);
		req.m_szUrl = "http://api.gadu-gadu.pl/access_token";
		req.flags = NLHRF_NODUMP | NLHRF_HTTP11 | NLHRF_PERSISTENT;
		req.nlc = nlc;
		req.AddHeader("User-Agent", GG8_VERSION);
		req.AddHeader("Authorization", ptrA(oauth_auth_header("POST", req.m_szUrl, HMACSHA1, uin, password, token, token_secret)));
		req.AddHeader("Accept", "*/*");

		resp = Netlib_HttpTransaction(m_hNetlibUser, &req);
		if (resp) {
			if (resp->resultCode == 200 && !resp->body.IsEmpty()) {
				TiXmlDocument doc;
				if (0 == doc.Parse(resp->body)) {
					TiXmlConst hXml(doc.FirstChildElement("result"));
					if (auto *p = hXml["oauth_token"].ToElement())
						token = mir_strdup(p->GetText());

					if (auto *p = hXml["oauth_token_secret"].ToElement())
						token_secret = mir_strdup(p->GetText());
				}
			}
			else debugLogA("oauth_receivetoken(): Invalid response code from HTTP request");

			Netlib_CloseHandle(resp->nlc);
		}
		else debugLogA("oauth_receivetoken(): No response from HTTP request");
	}
	mir_free(password);

	if (token != nullptr && token_secret != nullptr) {
		setString(GG_KEY_TOKEN, token);
		setString(GG_KEY_TOKENSECRET, token_secret);
		debugLogA("oauth_receivetoken(): Access Token obtained successfully.");
		res = 1;
	}
	else {
		delSetting(GG_KEY_TOKEN);
		delSetting(GG_KEY_TOKENSECRET);
		debugLogA("oauth_receivetoken(): Failed to obtain Access Token.");
	}
	mir_free(token);
	mir_free(token_secret);

	return res;
}

int GaduProto::oauth_checktoken(int force)
{
	if (force)
		return oauth_receivetoken();

	ptrA token(getStringA(GG_KEY_TOKEN));
	ptrA token_secret(getStringA(GG_KEY_TOKENSECRET));
	if (token == NULL || token_secret == NULL)
		return oauth_receivetoken();

	return 1;
}
