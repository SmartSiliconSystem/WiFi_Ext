#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#include "httpd.h"
#include "driver/uart.h"
//#include "esp_sdk_ver.h"

HttpdBuiltInUrl builtInUrls[]={
	{"/SENDDATA", cgiSendData},
	{"/GETDATA", cgiGetData},
	{"*", cgiFileSystem}, //Catch-all cgi function for the filesystem
	{NULL, NULL}
};

static os_timer_t client_timer;
static uint8_t wifi_sta_nb_conn_retry = 0;

static struct station_config stconfig;
static struct softap_config apconfig;
struct saved_param WiFi_config STORE_ATTR;
static int WifiConfigValid = 0;

static void setup_wifi_ap_mode(void);
static void setup_wifi_st_mode(void);

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void);

#define myos_printf ets_uart_printf

uint16_t crc16_compute(const uint8_t * p_data, uint32_t size, const uint16_t * p_crc)
{
    uint32_t i;
    uint16_t crc = (p_crc == NULL) ? 0xffff : *p_crc;

    for (i = 0; i < size; i++)
    {
        crc  = (unsigned char)(crc >> 8) | (crc << 8);
        crc ^= p_data[i];
        crc ^= (unsigned char)(crc & 0xff) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xff) << 4) << 1;
    }

    return crc;
}

/*
 * Save WiFi configuration for SoftAP & Station from WiFi_config struct
 */
void save_param(void)
{
	spi_flash_erase_sector(PARAM_SECTOR);
	WiFi_config.dummy = 0;
	WiFi_config.crc = crc16_compute((uint8_t*)&WiFi_config, sizeof(struct saved_param) - sizeof(uint16_t), NULL);
	spi_flash_write(PARAM_SECTOR * SPI_FLASH_SEC_SIZE, (uint32_t*)&WiFi_config, sizeof(struct saved_param));
	myos_printf("configuration saved crc 0x%04x\n", WiFi_config.crc);
}

/*
 * Load WiFi configuration for SoftAP & Station to WiFi_config struct
 */
static int load_param(void) {
	myos_printf("user_rf_cal_sector_set(): Sector %x, load WiFi configuration from sector %x\r\n", user_rf_cal_sector_set(), PARAM_SECTOR);
	os_bzero(&WiFi_config, sizeof(WiFi_config));
	spi_flash_read(PARAM_SECTOR * SPI_FLASH_SEC_SIZE, (uint32_t*)&WiFi_config, sizeof(struct saved_param));
	uint16_t crc = crc16_compute((uint8_t*)&WiFi_config, sizeof(struct saved_param) - sizeof(uint16_t), NULL);
	if (crc != WiFi_config.crc) {
		myos_printf("Load WiFi configuration failed: CRC 0x%04x != 0x%04x\r\n", crc, WiFi_config.crc);
		return 0;
	}
	return 1;
}

/*
 * Process WiFi station events
 * In case of connection error, switch to SoftAP mode
 */
static void ICACHE_FLASH_ATTR wait_for_ip(void *timer_arg)
{
	os_timer_disarm(&client_timer);

	switch(wifi_station_get_connect_status()) {
	case STATION_GOT_IP:
	{
		struct ip_info ipconfig;
		wifi_get_ip_info(STATION_IF, &ipconfig);
		if( ipconfig.ip.addr != 0) {
		#ifdef PLATFORM_DEBUG
			ets_uart_printf("Connected to WiFi: Access point %s @ip: %d.%d.%d.%d\n  SDK version: %s\n  Boot version: %d\n  CPU frequency: %d\n  Free heap: %d bytes\r\n", stconfig.ssid, ipconfig.ip.addr & 0xff, (ipconfig.ip.addr>>8) & 0xff, (ipconfig.ip.addr>>16) & 0xff, (ipconfig.ip.addr>>24), system_get_sdk_version(), system_get_boot_version (),  system_get_cpu_freq(), system_get_free_heap_size());
		#endif
			httpdInit(builtInUrls, 80);
		} else {
			os_timer_setfn(&client_timer, (os_timer_func_t *)wait_for_ip, NULL);
			os_timer_arm(&client_timer, 500, 0);
		}
	}
	break;
	case STATION_WRONG_PASSWORD:
	#ifdef PLATFORM_DEBUG
		ets_uart_printf("WiFi connection error: invalid password\r\n");
	#endif
		setup_wifi_ap_mode();
		break;
	case STATION_NO_AP_FOUND:
	#ifdef PLATFORM_DEBUG
		if (WifiConfigValid)
		{
			ets_uart_printf("WiFi connection error: Access Point %s not found\r\n", stconfig.ssid);
		}
		else
		{
			ets_uart_printf("WiFi connection error: No Access Point configured yet\r\n");
		}
	#endif
		setup_wifi_ap_mode();
		break;
	case STATION_CONNECT_FAIL:
#ifdef PLATFORM_DEBUG
		ets_uart_printf("WiFi connection failed\r\n");
#endif
		setup_wifi_ap_mode();
		break;
	default:
#ifdef PLATFORM_DEBUG
		if (WifiConfigValid)
		{
			ets_uart_printf("Connecting to WiFi: Access Point %s, attempt %d/%d...\r\n", stconfig.ssid, wifi_sta_nb_conn_retry, WIFI_STA_MAX_CONN_RETRY);
		}
		else
		{
			ets_uart_printf("Connecting to WiFi: No Access Point configured yet\r\n");
		}
#endif
		wifi_sta_nb_conn_retry++;
		if (wifi_sta_nb_conn_retry == WIFI_STA_MAX_CONN_RETRY)
		{
			setup_wifi_ap_mode();
		}
		else
		{
		    os_timer_setfn(&client_timer, (os_timer_func_t *)wait_for_ip, NULL);
		    os_timer_arm(&client_timer, 1000, 0);
		}
	}
}

static void ICACHE_FLASH_ATTR wifi_handle_event_cb(System_Event_t *evt);

/*
 * Callback of RF system configuration for Station mode
 */
static void system_is_done(void){
	myos_printf("System configured\r\n");

	//Bringing up WLAN
	wifi_set_event_handler_cb(wifi_handle_event_cb);
	wifi_station_connect();
	wifi_station_dhcpc_start();

	os_timer_disarm(&client_timer);
	os_timer_setfn(&client_timer, wait_for_ip, NULL);
	os_timer_arm(&client_timer, 500, 0);
}

/*
 * Setup WiFi in SoftAP mode
 */
void ICACHE_FLASH_ATTR setup_wifi_ap_mode(void)
{
	struct ip_info ipinfo;
	// Since we are coming from a Station connection error, force disconnect
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	// Setup SoftAP
	wifi_set_opmode(STATIONAP_MODE);
	wifi_softap_dhcps_stop();

	os_bzero(&apconfig, sizeof(apconfig));
	if (WifiConfigValid == 0) { 
		os_strncpy(WiFi_config.SoftAPSSID, WIFI_APSSID, sizeof(WiFi_config.SoftAPSSID)-1);
		os_strncpy(WiFi_config.SoftAPPwd, WIFI_APPASSWORD, sizeof(WiFi_config.SoftAPPwd)-1);
	}

	os_strncpy((char *)apconfig.ssid, WiFi_config.SoftAPSSID, sizeof(apconfig.ssid)-1);
	apconfig.ssid_len = strlen((char *)apconfig.ssid);
	os_strncpy((char *)apconfig.password, WiFi_config.SoftAPPwd, sizeof(apconfig.password)-1);

	apconfig.authmode = AUTH_WPA_WPA2_PSK;
	apconfig.ssid_hidden = 0;
	apconfig.channel = 7;
	apconfig.max_connection = 4;
	if(!wifi_softap_set_config(&apconfig))	{
#ifdef PLATFORM_DEBUG
		ets_uart_printf("Access Point configuration error. AP=%s, Password=%s\r\n", apconfig.ssid, apconfig.password);
#endif
		return;
	}

	/* All good start the failover server */
	IP4_ADDR(&ipinfo.ip, 192, 168, 8, 1);
	IP4_ADDR(&ipinfo.gw, 192, 168, 8, 1);
	IP4_ADDR(&ipinfo.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &ipinfo);
	wifi_softap_dhcps_start();
#ifdef PLATFORM_DEBUG
	ets_uart_printf("WiFi configured in Access Point mode. Access point %s\n  SDK version: %s\n  Boot version: %d\n  CPU frequency: %d\n  Free heap: %d bytes\r\n", apconfig.ssid, system_get_sdk_version(), system_get_boot_version (),  system_get_cpu_freq(), system_get_free_heap_size());
#endif
	os_delay_us(1000);
	httpdInit(builtInUrls, 80);
}

/*
 * Setup WiFi in Station mode
 */
void ICACHE_FLASH_ATTR setup_wifi_st_mode(void)
{
	wifi_set_opmode(STATION_MODE);
	wifi_station_set_auto_connect(0);
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	
	os_bzero(&stconfig, sizeof(stconfig));
	if(wifi_station_get_config(&stconfig))
	{
		stconfig.bssid_set = 0;
		os_strncpy((char*)stconfig.ssid, WiFi_config.StationSSID, sizeof(stconfig.ssid)-1);
		os_strncpy((char*)stconfig.password,  WiFi_config.StationPwd, sizeof(stconfig.password)-1);
		if(!wifi_station_set_config(&stconfig))
		{
		#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi station mode setup error\r\n");
		#endif
		}
	}
#ifdef PLATFORM_DEBUG
	ets_uart_printf("WiFi configured in Station mode\r\n");
#endif
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 8;
		break;

        case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;

        default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void)
{
	system_phy_set_max_tpw(82);
}

static void ICACHE_FLASH_ATTR wifi_handle_event_cb(System_Event_t *evt)
{
#if (PLATFORM_DEBUG>1)
	myos_printf("event %x\n", evt->event);
	switch (evt->event) {
	case EVENT_STAMODE_CONNECTED:
		myos_printf("connect to ssid %s, channel %d\n",
			evt->event_info.connected.ssid,
			evt->event_info.connected.channel);
		break;
	case EVENT_STAMODE_DISCONNECTED:
		myos_printf("disconnect from ssid %s, reason %d\n",
			evt->event_info.disconnected.ssid,
			evt->event_info.disconnected.reason);
		break;
	case EVENT_STAMODE_AUTHMODE_CHANGE:
		myos_printf("mode: %d -> %d\n",
			evt->event_info.auth_change.old_mode,
			evt->event_info.auth_change.new_mode);
		break;
	case EVENT_STAMODE_GOT_IP:
		myos_printf("ip:" IPSTR ", mask:" IPSTR ", gw:" IPSTR,
			IP2STR(&evt->event_info.got_ip.ip),
			IP2STR(&evt->event_info.got_ip.mask),
			IP2STR(&evt->event_info.got_ip.gw));
		myos_printf("\n");
		break;
	case EVENT_SOFTAPMODE_STACONNECTED:
		myos_printf("station: " MACSTR " join, AID = %d\n",
			MAC2STR(evt->event_info.sta_connected.mac),
			evt->event_info.sta_connected.aid);
		break;
	case EVENT_SOFTAPMODE_STADISCONNECTED:
		myos_printf("station: " MACSTR " leave, AID = %d\n",
			MAC2STR(evt->event_info.sta_disconnected.mac),
			evt->event_info.sta_disconnected.aid);
		break;
	case EVENT_SOFTAPMODE_PROBEREQRECVED:
		myos_printf("station: " MACSTR " probereqreceived, AID = %d\n",
			MAC2STR(evt->event_info.sta_disconnected.mac),
			evt->event_info.sta_disconnected.aid);
		break;
	default:
		break;
	}
#endif
}

void ICACHE_FLASH_ATTR user_init(void)
{
	// 115200
	uart_init(115200, 115200);
	os_delay_us(1000);
	// Configure GPIO
	PIN_FUNC_SELECT(RJ45_YELLOW_MUX, RJ45_YELLOW_FUNC);
	GPIO_OUTPUT_SET(RJ45_YELLOW, 0);

	PIN_FUNC_SELECT(RJ45_GREEN_MUX, RJ45_GREEN_FUNC);
	GPIO_OUTPUT_SET(RJ45_GREEN, 0);

	PIN_FUNC_SELECT(RS485_DIR_MUX, RS485_DIR_FUNC);
	GPIO_OUTPUT_SET(RS485_DIR, 1); // Transmit by default

	// Load saved params
	WifiConfigValid = load_param();
	setup_wifi_st_mode(); // start with station mode even if no access point has been defined

	system_init_done_cb(&system_is_done);
}
