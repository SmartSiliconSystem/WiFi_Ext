#ifndef HTTPD_H
#define HTTPD_H
#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

#define HTTPD_CGI_MORE 		0
#define HTTPD_CGI_DONE 		1
#define HTTPD_CGI_NOTFOUND 	2

//Max post buffer len
#define MAX_POST 1024
//Max get buffer len
#define MAX_GET 1024
//Max length of request head
#define MAX_HEAD_LEN 1024
//Max amount of connections
#define MAX_CONN 8

typedef struct HttpdPriv HttpdPriv;
typedef struct HttpdConnData HttpdConnData;

typedef int (* cgiSendCallback)(HttpdConnData *connData);

//A struct describing a http connection. This gets passed to cgi functions.
struct HttpdConnData {
	struct espconn *conn;
	HttpdPriv *priv;
	char *url;					// requested url
	cgiSendCallback cgi;
	uint8_t *cgiData;
	uint16_t cgiDataLength;
	uint16_t cgiDataPos;
	char *postBuff;				// holds client post data
	uint16_t postLen;			// size of client post data
};

//A struct describing an url. This is the main struct that's used to send different URL requests to
//different routines.
typedef struct {
	const char *url;
	cgiSendCallback cgiCb;
} HttpdBuiltInUrl;

char* toUpperCase(char* str);
char* float2str(float f, char *str, uint8_t prec);
void ICACHE_FLASH_ATTR JSONToMyAppliance(HttpdConnData *connData);

int ICACHE_FLASH_ATTR cgiFileSystem(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiGetData(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiSendData(HttpdConnData *connData);

int httpdUrlDecode(char *val, int valLen, char *ret, int retLen);
void ICACHE_FLASH_ATTR httpdInit(HttpdBuiltInUrl *fixedUrls, int port);
void ICACHE_FLASH_ATTR httpdSendHeader(HttpdConnData *conn, uint16_t len);

#endif
