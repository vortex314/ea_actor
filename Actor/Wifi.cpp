/*
 * Wifi.cpp
 *
 *  Created on: Oct 24, 2015
 *      Author: lieven
 */

#include "Wifi.h"

/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
extern "C" {
#include "stdint.h"
#include <string.h>
//#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
//#include "mqtt_msg.h"
//#include "debug.h"
#include "user_config.h"
//#include "config.h"
#include "ets_sys.h"
//#include "espmissingincludes.h"
}
#include "Sys.h"
#include <Logger.h>

Wifi::~Wifi() {

}

ActorRef Wifi::create(const char* ssid, const char* pswd) {
	return ActorRef(new Wifi(ssid,pswd));
}

Wifi::Wifi(const char* ssid, const char* pswd) :
				Actor("Wifi"){
	strncpy(_ssid, ssid, sizeof(_ssid));
	strncpy(_pswd, pswd, sizeof(_pswd));
}

uint8_t Wifi::_wifiStatus = STATION_IDLE;


void Wifi::onReceive(Header hdr,Cbor& cbor) {
//	LOGF("line : %d ",_ptLine);
// LOGF("msg : %d:%d",msg.src(),msg.signal());
	PT_BEGIN()
	;
	INIT: {
		PT_WAIT_UNTIL(hdr._event==INIT);
		struct station_config stationConf;
		int erc;
//	ets_delay_us(300000);
//		LOGF("WIFI_INIT");
//		if ((erc = wifi_set_opmode(STATION_MODE))) {
//		ets_delay_us(300000);
//		LOGF("op mode %d", wifi_get_opmode());
//		LOGF("op mode default %d", wifi_get_opmode_default());
//		LOGF("phy mode %d", wifi_get_phy_mode());
//		 wifi_set_opmode(STATION_MODE);
			wifi_set_opmode_current(STATION_MODE);
//			ets_delay_us(30000);
			; // STATIONAP_MODE was STATION_MODE
			if (wifi_set_phy_mode(PHY_MODE_11B)) { // true or false
				os_memset(&stationConf, 0, sizeof(struct station_config));
				ets_strncpy((char*) stationConf.ssid, _ssid,
						sizeof(stationConf.ssid));
				ets_strncpy((char*) stationConf.password, _pswd,
						sizeof(stationConf.password));
//				LOGF("%s:%s",stationConf.ssid,stationConf.password);
				stationConf.bssid_set = 0;

				wifi_station_set_config(&stationConf);
				if (wifi_station_set_config_current(&stationConf)) {
					if (wifi_station_connect()) {
						goto DISCONNECTED;
						//	wifi_station_set_auto_connect(TRUE);
					}
				}
//			}
		}
		//	wifi_station_set_auto_connect(FALSE);
		LOGF(" WIFI INIT failed , retrying... %d", erc);
		goto DISCONNECTED;
	}
	;
	DISCONNECTED: {
		while (true) {
			setReceiveTimeout(1000);
			PT_YIELD_UNTIL(timeout());
			struct ip_info ipConfig;
			wifi_get_ip_info(STATION_IF, &ipConfig);
			_wifiStatus = wifi_station_get_connect_status();
			if (wifi_station_get_connect_status() == STATION_NO_AP_FOUND
					|| wifi_station_get_connect_status()
							== STATION_WRONG_PASSWORD
					|| wifi_station_get_connect_status()
							== STATION_CONNECT_FAIL) {
//				LOGF("NOT CONNECTED");
				wifi_station_connect();
			} else if (_wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0) {
				_connections++;
				union {
					uint32_t addr;
					uint8_t ip[4];
				} v;
				v.addr = ipConfig.ip.addr;
				LOGF("IP Address : %d.%d.%d.%d", v.ip[0], v.ip[1], v.ip[2],
						v.ip[3]);
//				LOGF("CONNECTED");
				right().tell(self(), CONNECTED,0);

				goto CONNECTED;
			} else {
//				LOGF("STATION_IDLE");
			}
			setReceiveTimeout(500);
		}
	}
	;
	CONNECTED: {
		while (true) {
			setReceiveTimeout(2000);
			PT_YIELD_UNTIL(hdr._event==TIMEOUT);
			struct ip_info ipConfig;
			wifi_get_ip_info(STATION_IF, &ipConfig);
			_wifiStatus = wifi_station_get_connect_status();
			if (_wifiStatus != STATION_GOT_IP) {
				right().tell(self(), DISCONNECTED,0);
				LOGF("DISCONNECTED");
				goto DISCONNECTED;
			}
		}

	}
	;
PT_END()
;

}

