#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "httpd.h"
#include "JSONData.h"
#include "WebFiles.h"
#include "user_config.h"

extern uint16_t crc16_compute(const uint8_t * p_data, uint32_t size, const uint16_t * p_crc);
extern void save_param(void);
extern struct WEBFILE_INFO WebFiles[];
extern struct saved_param WiFi_config;

static HttpdBuiltInUrl *builtInUrls;

struct HttpdPriv {
	char head[MAX_HEAD_LEN];	// holds the client request header
	int headPos;				// position within the header
	int postPos;				// position within the post data
};

//Connection pool
static HttpdPriv connPrivData[MAX_CONN];
static HttpdConnData connData[MAX_CONN];

static struct espconn httpdConn;
static esp_tcp httpdTcp;

//Looks up the connData info for a specific esp connection
static HttpdConnData ICACHE_FLASH_ATTR *httpdFindConnData(void *arg) {
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn==(struct espconn *)arg) return &connData[i];
	}
	#ifdef PLATFORM_DEBUG
	ets_uart_printf("httpdFindConnData(): ERROR ! Cannot find connection slot for %p\n", arg);
	#endif
	return NULL;
}

//Retires a connection for re-use
static void ICACHE_FLASH_ATTR httpdRetireConn(HttpdConnData *conn) {
	if (conn->postBuff!=NULL) os_free(conn->postBuff);
	conn->postBuff=NULL;
	conn->cgi=NULL;
	conn->conn=NULL;
}

static void ICACHE_FLASH_ATTR httpdSentCb(void *arg) {
#ifdef PLATFORM_DEBUG
	ets_uart_printf("httpdSentCb(): %p\r\n", arg);
#endif
	int r;
	HttpdConnData *conn=httpdFindConnData(arg);
	if (conn==NULL) return;
	if (conn->cgi==NULL) { //Marked for destruction?
		espconn_disconnect(conn->conn);
		httpdRetireConn(conn);
		return;
	}
	r=conn->cgi(conn); //Execute cgi fn.
	if (r==HTTPD_CGI_DONE) {
		conn->cgiData = NULL;
		conn->cgi=NULL; //mark for destruction.
	}
}

static const char *httpNotFoundHeader="HTTP/1.1 404 Not Found\r\nServer: NewTS\r\nContent-Type: text/plain\r\n\r\nNot Found.\r\n";

/*
 * Find the url in the static table and call the corresponding cgi function
 */
static void ICACHE_FLASH_ATTR httpdSendResp(HttpdConnData *conn) {
	int i=0;
	int res;
	//See if the url is somewhere in our internal url table.
	while (builtInUrls[i].url!=NULL && conn->url!=NULL) {
		if (os_strcmp(builtInUrls[i].url, conn->url)==0 || builtInUrls[i].url[0]=='*') {
			conn->cgiData=NULL;
			conn->cgi=builtInUrls[i].cgiCb;
			res=conn->cgi(conn);
			if (res!=HTTPD_CGI_NOTFOUND) {
				if (res==HTTPD_CGI_DONE) conn->cgi=NULL;  //If cgi finishes immediately: mark conn for destruction.
				return;
			}
		}
		i++;
	}
	espconn_sent(conn->conn, (uint8 *)httpNotFoundHeader, os_strlen(httpNotFoundHeader));
	conn->cgi=NULL; //mark for destruction
}

static void ICACHE_FLASH_ATTR httpdParseHeader(char *h, HttpdConnData *conn) {
	int i;
	if (os_strncmp(h, "GET ", 4)==0 || os_strncmp(h, "POST ", 5)==0) {
		char *e;
		//Skip past the space after POST/GET
		i=0;
		while (h[i]!=' ') i++;
		conn->url=h+i+1;
		//Figure out end of url.
		e=(char*)os_strstr(conn->url, " ");
		if (e==NULL) return;
		*e=0; //terminate url part
	#ifdef PLATFORM_DEBUG
		ets_uart_printf("httpdParseHeader(): URL requested= %s\r\n", conn->url);
	#endif
		toUpperCase(conn->url);
	} else if (os_strncmp(h, "Content-Length: ", 16)==0) {
		i=0;
		while (h[i]!=' ') i++;
		conn->postLen=atoi(h+i+1);
		if (conn->postLen>MAX_POST) conn->postLen=MAX_POST;
		conn->postBuff=(char*)os_malloc(conn->postLen+1);
		conn->priv->postPos=0;
	}
}

static void ICACHE_FLASH_ATTR httpdRecvCb(void *arg, char *data, unsigned short len) {
#ifdef PLATFORM_DEBUG
	ets_uart_printf("httpdRecvCb(): %p\r\n", arg);
#endif
	int x;
	char *p, *e;
	HttpdConnData *conn=httpdFindConnData(arg);
	if (conn==NULL) return;


	for (x=0; x<len; x++) {

		if (conn->priv->headPos!=-1) {
			//This byte is a header byte.
			if (conn->priv->headPos!=MAX_HEAD_LEN) conn->priv->head[conn->priv->headPos++]=data[x];
			conn->priv->head[conn->priv->headPos]=0;
			//Scan for /r/n/r/n
			if (data[x]=='\n' && (char *)os_strstr(conn->priv->head, "\r\n\r\n")!=NULL) {
				//Reset url data
				conn->url=NULL;
				//Find end of next header line
				p=conn->priv->head;
				while(p<(&conn->priv->head[conn->priv->headPos-4])) {
					e=(char *)os_strstr(p, "\r\n");
					if (e==NULL) break;
					e[0]=0;
					httpdParseHeader(p, conn);
					p=e+2;
				}
				//If we don't need to receive post data, we can send the response now.
				if (conn->postLen==0) {
					httpdSendResp(conn);
				}
				conn->priv->headPos=-1; //Indicate we're done with the headers.
			}
		} else if (conn->priv->postPos!=-1 && conn->postLen!=0 && conn->priv->postPos <= conn->postLen) {
			//This byte is a POST byte.
			conn->postBuff[conn->priv->postPos++]=data[x];
			if (conn->priv->postPos>=conn->postLen) {
				//Received post stuff.
				conn->postBuff[conn->priv->postPos]=0; //zero-terminate
				conn->priv->postPos=-1;
				//Send the response.
				httpdSendResp(conn);
				return;
			}
		}
	}
}

static void ICACHE_FLASH_ATTR httpdReconCb(void *arg, sint8 err) {
#ifdef PLATFORM_DEBUG
	ets_uart_printf("httpdReconCb(): %p, err=%d\r\n", arg, err);
#endif
	HttpdConnData *conn=httpdFindConnData(arg);
	if (conn==NULL) return;
	//Yeah... No idea what to do here. ToDo: figure something out.
}

static void ICACHE_FLASH_ATTR httpdDisconCb(void *arg) {
#if 0
	//Stupid esp sdk passes through wrong arg here, namely the one of the *listening* socket.
	//If it ever gets fixed, be sure to update the code in this snippet; it's probably out-of-date.
	HttpdConnData *conn=httpdFindConnData(arg);
	ets_uart_printf("Disconnected, conn=%p\n", conn);
	if (conn==NULL) return;
	conn->conn=NULL;
	if (conn->cgi!=NULL) conn->cgi(conn); //flush cgi data
#endif
	//Just look at all the sockets and kill the slot if needed.
	int i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].conn!=NULL) {
			if (connData[i].conn->state==ESPCONN_NONE || connData[i].conn->state==ESPCONN_CLOSE) {
				connData[i].conn=NULL;
				if (connData[i].cgi!=NULL) connData[i].cgi(&connData[i]); //flush cgi data
				httpdRetireConn(&connData[i]);
			}
		}
	}
}


static void ICACHE_FLASH_ATTR httpdConnectCb(void *arg) {
	struct espconn *conn=arg;
	int i;
	//Find empty conndata in pool
	for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;

	connData[i].priv=&connPrivData[i];
	if (i==MAX_CONN) {
	#ifdef PLATFORM_DEBUG
		ets_uart_printf("httpdConnectCb(): Maximum number of connection slots reached (>%d)\r\n", MAX_CONN);
	#endif
		espconn_disconnect(conn);
		return;
	}
	connData[i].conn=conn;
	connData[i].priv->headPos=0;
	connData[i].priv->postPos=0;
	connData[i].postBuff=NULL;
	connData[i].postLen=0;
	connData[i].cgiDataLength = 0;
	connData[i].cgiDataPos = 0;
	connData[i].cgiData = NULL;

	espconn_regist_recvcb(conn, httpdRecvCb);
	espconn_regist_reconcb(conn, httpdReconCb);
	espconn_regist_disconcb(conn, httpdDisconCb);
	espconn_regist_sentcb(conn, httpdSentCb);
}


void ICACHE_FLASH_ATTR httpdInit(HttpdBuiltInUrl *fixedUrls, int port) {
	int i;

	for (i=0; i<MAX_CONN; i++) {
		connData[i].conn=NULL;
	}
	httpdConn.type=ESPCONN_TCP;
	httpdConn.state=ESPCONN_NONE;
	httpdTcp.local_port=port;
	httpdConn.proto.tcp=&httpdTcp;
	builtInUrls=fixedUrls;

	espconn_regist_connectcb(&httpdConn, httpdConnectCb);
	espconn_accept(&httpdConn);
#ifdef PLATFORM_DEBUG
	ets_uart_printf("HTTP server started on port %d\r\n", port);
#endif
}

int ICACHE_FLASH_ATTR cgiFileSystem(HttpdConnData *connData)
{
	uint16_t len;
	char buff[MAX_GET];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (connData->cgiData == NULL) {
		//First call to this cgi
		uint8_t i = 0;
		while (WebFiles[i].filename)
		{
			// Find file resource
			if (strcmp(connData->url, WebFiles[i].filename) == 0) {
				break;
			}
			i++;
		}

		if (WebFiles[i].filename == NULL) {
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData = (uint8_t*) WebFiles[i].filecontent;
		connData->cgiDataLength = WebFiles[i].filesize;
		connData->cgiDataPos = 0;
	#ifdef PLATFORM_DEBUG
		ets_uart_printf("cgiFileSystem(): file= %s, size= %d\r\n", WebFiles[i].filename, WebFiles[i].filesize);
	#endif
		os_bzero(connData->priv->head, MAX_HEAD_LEN);
		len = os_sprintf(connData->priv->head, "HTTP/1.1 %d OK\r\nServer: NewTS\r\n", 200);
		len += os_sprintf(connData->priv->head+len, "Content-Type: %s\r\n", WebFiles[i].mimetype);
		char s[6]; os_bzero(s, 6);
		os_sprintf(s, "%d", WebFiles[i].filesize);
		len += os_sprintf(connData->priv->head+len, "Content-Length: %s\r\n\r\n", s);
		espconn_sent(connData->conn, connData->priv->head, len);
		return HTTPD_CGI_MORE;
	}
	if ((connData->cgiDataLength - connData->cgiDataPos) > MAX_GET) {
		os_memcpy(buff, (uint8_t*)connData->cgiData + connData->cgiDataPos, MAX_GET);
		connData->cgiDataPos += MAX_GET;
		espconn_sent(connData->conn, (uint8 *)buff, MAX_GET);
		return HTTPD_CGI_MORE;
	}
	else
	{
		os_bzero(buff, MAX_GET);
		uint8_t not_word_aligned = (connData->cgiDataLength - (connData->cgiDataPos)) % 4;
		if (not_word_aligned)
		{
			os_memcpy(buff,
					 (uint8_t*)connData->cgiData + connData->cgiDataPos,
					  connData->cgiDataLength - (connData->cgiDataPos) + (4-not_word_aligned)
					 );
			espconn_sent(connData->conn, (uint8 *)buff, connData->cgiDataLength - (connData->cgiDataPos));
		}
		else
		{
			os_memcpy(buff,
					 (uint8_t*)connData->cgiData + connData->cgiDataPos,
					  connData->cgiDataLength - (connData->cgiDataPos)
					 );
			espconn_sent(connData->conn, (uint8 *)buff, connData->cgiDataLength - (connData->cgiDataPos));
		}
		connData->cgi=NULL;
		return HTTPD_CGI_DONE;
	}
}

int ICACHE_FLASH_ATTR cgiGetData(HttpdConnData *connData) {
	int lenh, len;
	char buff[MAX_POST];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	os_bzero(connData->priv->head, MAX_HEAD_LEN);
	lenh = os_sprintf(connData->priv->head, "HTTP/1.1 %d OK\r\nServer: NewTS\r\n", 200);
	lenh += os_sprintf(connData->priv->head+lenh, "Content-Type: application/json\r\n");

	char s[16];
	len=os_sprintf(buff,"{\"Status\":%d,\"WaterVolume\":%s,\"ApplianceName\":\"%s\",\"key1\":%d,\"key2\":%d,\"key3\":%d,\"WaterHardness\":%d,\"PH\":%s,\"Turbidity\":%d,\"WaterTemp\":%s,\"CopperReleased\":%s,\"WaterFlow\":%s,\"WaterColor\":%d,\"ContainerMLCapacity\":%d,\"CopperElectrodeMass\":%d,\"ContainerCuLevel\":%d,\"ContainerPHLevel\":%d,\"OnTime\":%d,\"StationSSID\":\"%s\",\"StationPwd\":\"%s\",\"SoftAPSSID\":\"%s\",\"SoftAPPwd\":\"%s\"}",
				MyAppliance.Status,
				float2str(MyAppliance.WaterVolume, s, 2),
				MyAppliance.ApplianceName,
				MyAppliance.key1,
				MyAppliance.key2,
				MyAppliance.key3,
				MyAppliance.WaterHardness,
				float2str(MyAppliance.PH, s, 2),
				MyAppliance.Turbidity,
				float2str(MyAppliance.WaterTemp, s, 2),
				float2str(MyAppliance.CopperReleased, s, 2),
				float2str(MyAppliance.WaterFlow, s, 2),
				MyAppliance.WaterColor,
				MyAppliance.ContainerMLCapacity,
				MyAppliance.CopperElectrodeMass,
				MyAppliance.ContainerCuLevel,
				MyAppliance.ContainerPHLevel,
				MyAppliance.OnTime,
				WiFi_config.StationSSID,
				WiFi_config.StationPwd,
				WiFi_config.SoftAPSSID,
				WiFi_config.SoftAPPwd
				);
	os_bzero(s, 16);
	os_sprintf(s, "%d", len);
	lenh += os_sprintf(connData->priv->head+lenh, "Content-Length: %s\r\n\r\n", s);
	os_memcpy(connData->priv->head+lenh, buff, len);
	espconn_sent(connData->conn, connData->priv->head, lenh+len);
#ifdef PLATFORM_DEBUG
	ets_uart_printf("cgiGetData(): JSON data sent, %d bytes\r\n", len);
#endif
	connData->cgi=NULL;
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiSendData(HttpdConnData *connData) {
	int lenh;
#ifdef PLATFORM_DEBUG
	ets_uart_printf("cgiSendData(): JSON data received, %d bytes\r\n", connData->postLen);
#endif

	JSONToMyAppliance(connData); // Update MyAppliance with Received JSON data

	os_bzero(connData->priv->head, MAX_HEAD_LEN);
	lenh = os_sprintf(connData->priv->head, "HTTP/1.1 %d OK\r\nServer: NewTS\r\n", 200);
	espconn_sent(connData->conn, connData->priv->head, lenh);
	connData->cgi=NULL;
	return HTTPD_CGI_DONE;
}

char* toUpperCase(char* str)
{
	uint16_t i = 0;
	while(str[i])
	{
		if ((str[i]<123) && (str[i]>96)) str[i] = str[i] - 32;
		i++;
	}
	return str;
}

float str2float(char *x)
{
	static float total, num;
	total = 0;
	int df=10,i=0;
	bool decimal=false;
	while(x[i])
	{
		num=x[i]-'0';
		if(decimal)
		{
			num=num/df;
			df=df*10;
		}
		else if (!decimal && ( x[i]>='0' && x[i]<='9'))
			total=total*10;

		if(x[i]=='.')
			decimal=true;
		else total+=num;

		i++;
	}
	return total;
}

char* float2str(float f, char *str, uint8_t prec)
{
	uint8_t i, l;
	uint32_t p = 10;
	for (i = 0;i < prec-1;i++)
	{
		p = p * 10;
	}
	int32_t e = (int32_t)(f * p);
	l=os_sprintf(str, "%s%d.%d", f < 0 ? "-" : "", (int32_t) f, e - ((int32_t) f) * p);
	str[l]=0;
	return str;
 }

void ICACHE_FLASH_ATTR UpdateData(char *key,char *value){

#ifdef PLATFORM_DEBUG
	ets_uart_printf("  UpdateData(): |%s=%s|\r\n", key, value);
#endif

	if (strcmpi(key, "Status") == 0){
		WebData.Status = atoi(value);
		// Perform specific update of MyAppliance status bit (only those being managed through the user interface)
	}
	else if (strcmpi(key, "WaterVolume") == 0){
		WebData.WaterVolume = str2float(value);
		MyAppliance.WaterVolume = WebData.WaterVolume;
	}
	else if (strcmpi(key, "ApplianceName") == 0){
		strcpy(WebData.ApplianceName, value);
		strcpy(MyAppliance.ApplianceName, WebData.ApplianceName);
	}
	else if (strcmpi(key, "key1") == 0){
		WebData.key1 = atoi(value);
		MyAppliance.key1 = WebData.key1;
	}
	else if (strcmpi(key, "key2") == 0){
		WebData.key2 = atoi(value);
		MyAppliance.key2 = WebData.key2;
	}
	else if (strcmpi(key, "key3") == 0){
		WebData.key3 = atoi(value);
		MyAppliance.key3 = WebData.key3;
	}
	else if (strcmpi(key, "StationSSID") == 0){
		strcpy(WebData.StationSSID, value);
	}
	else if (strcmpi(key, "StationPwd") == 0){
		strcpy(WebData.StationPwd, value);
	}
	else if (strcmpi(key, "SoftAPSSID") == 0){
		strcpy(WebData.SoftAPSSID, value);
	}
	else if (strcmpi(key, "SoftAPPwd") == 0){
		strcpy(WebData.SoftAPPwd, value);
	}
}

void ICACHE_FLASH_ATTR JSONToMyAppliance(HttpdConnData *connData){
	// Update MyAppliance with Received JSON data
	uint16_t CurrentPos = 0;
	uint16_t s = 0;
	uint16_t e = 0;
	uint16_t dp = 0;
	uint16_t i;
	char key[32]; key[0] = 0;
	char value[32]; value[0] = 0;
	while (CurrentPos < connData->postLen){
		if (connData->postBuff[CurrentPos] == 34) { // " char
			if (s == 0) s = CurrentPos+1; else e = CurrentPos+1; // s points to 1st char, e points to
		}
		else if (connData->postBuff[CurrentPos] == 58) { // : char
			if (e) { // key found having ": char sequence
				for (i=0;i<CurrentPos-s-1;i++) key[i] = connData->postBuff[s+i];
				key[CurrentPos-s-1] = 0;
				s = 0; e = 0;
			}
			dp = CurrentPos+1; // keep track of value 1st char pos
		}
		else if ((connData->postBuff[CurrentPos] == 44) || (connData->postBuff[CurrentPos] == 125)) { // , or } char
			if (key[0]) {
				if (s && e) { // Value is between ""
					for (i=0;i<CurrentPos-s-1;i++) value[i] = connData->postBuff[s+i];
					//value[e-s] = 0;
					value[CurrentPos-s-1] = 0;
				} else if (dp) { // Numeric value
					for (i=0;i<CurrentPos-dp;i++) value[i] = connData->postBuff[dp+i];
					value[CurrentPos-dp] = 0;
				}
				UpdateData(key, value);
				key[0] = 0; value[0] = 0;
				s = 0; e = 0; dp = 0;
			}
		}
		CurrentPos++;
	}
	struct saved_param User_WiFi_Param;
	os_bzero(&User_WiFi_Param, sizeof(struct saved_param));
	os_strncpy(User_WiFi_Param.StationSSID, WebData.StationSSID, sizeof(User_WiFi_Param.StationSSID)-1);
	os_strncpy(User_WiFi_Param.StationPwd, WebData.StationPwd, sizeof(User_WiFi_Param.StationPwd)-1);
	os_strncpy(User_WiFi_Param.SoftAPSSID, WebData.SoftAPSSID, sizeof(User_WiFi_Param.SoftAPSSID)-1);
	os_strncpy(User_WiFi_Param.SoftAPPwd, WebData.SoftAPPwd, sizeof(User_WiFi_Param.SoftAPPwd)-1);
	uint16_t crc = crc16_compute((uint8_t*)&User_WiFi_Param, sizeof(struct saved_param) - sizeof(uint16_t), NULL);
	if (crc != WiFi_config.crc) {
		os_memcpy(&WiFi_config, &User_WiFi_Param, sizeof(struct saved_param));
		save_param();
		system_restart();
		while (1) {
			os_delay_us(10000);
		}
	}
}


