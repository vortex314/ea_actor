/*
 * Wifi.h
 *
 *  Created on: May 16, 2016
 *      Author: lieven
 */

#ifndef WIFI_H_
#define WIFI_H_

#include "Actor.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

class Wifi: public Actor {
private:
	const char* _ssid;
	const char* _pswd;
	static Wifi* _wifi;
public:
	Wifi(const char* ssid, const char* pswd) :
			Actor("Wifi") {
		_ssid = ssid;
		_pswd = pswd;
		_wifi = this;
	}
	virtual ~Wifi() {

	}
	static ActorRef create(const char* ssid, const char* pswd) {
		return ActorRef(new Wifi(ssid, pswd));
	}
	void init() {
		WiFi.begin(_ssid, _pswd);
		WiFi.onEvent(callback);
		WiFi.setAutoConnect(true);
	}

	void onReceive(Header header, Cbor& data) {
		if (header.event == INIT)
			init();
	}

	static void callback(WiFiEvent_t event) {
		if (event == WIFI_EVENT_STAMODE_CONNECTED)
			_wifi->right().tell(_wifi->self(), CONNECTED, 0);
		if (event == WIFI_EVENT_STAMODE_DISCONNECTED)
			_wifi->right().tell(_wifi->self(), DISCONNECTED, 0);
	}

};

#endif /* WIFI_H_ */
