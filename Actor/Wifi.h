/*
 * Wifi.h
 *
 *  Created on: Oct 24, 2015
 *      Author: lieven
 */

#ifndef WIFI_H_
#define WIFI_H_
#include "Actor.h"

class Wifi: public Actor {
	char _ssid[32];
	char _pswd[64];
	uint32_t _connections;
	static void callback();
	static uint8_t _wifiStatus;
private:
	Wifi(const char* ssid, const char* pswd);
public:

	virtual ~Wifi();
	static ActorRef create(const char* ssid, const char* pswd);
	void onReceive(Header, Cbor&);
	void init();
};

#endif /* WIFI_H_ */
