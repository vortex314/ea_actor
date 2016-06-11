/*
 * Dwm1000.cpp
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */
#include <Arduino.h>
#include <System.h>
#include <Json.h>
extern "C" {
#include "user_interface.h"
}

static Cbor out(100);

System::System(ActorRef mqtt) :
		Actor("System") {
	_mqtt = mqtt;
	_mqttConnected=false;
}

System::~System() {

}

//___________________________________________________
//
bool System::subscribed(Header hdr) {
	return Actor::subscribed(hdr) || hdr.is(_mqtt, REPLY(CONNECT))
			|| hdr.is(_mqtt, REPLY(DISCONNECT));
}

void System::init() {

}

void System::publish(uint8_t qos, const char* key, Str& value) {
	if ( _mqttConnected ) _mqtt.tell(self(), PUBLISH, qos, _cborOut.putf("sB", key, &value));
}

void System::onReceive(Header hdr, Cbor& data) {
	Json json(20);
	if (hdr.is(_mqtt, REPLY(CONNECT))) {
		_mqttConnected = true;
		return;
	} else if (hdr.is(_mqtt, REPLY(DISCONNECT))) {
		_mqttConnected = false;
		return;
	} else if (hdr.is(INIT)) {
		setReceiveTimeout(10000);
		init();
		return;
	};
	switch (hdr._event) {
	case TIMEOUT: {
		setReceiveTimeout(100);
		json.clear();
		json.add(true);
		publish(0, "system/online", json);
		json.clear();
		json.add((uint64_t) millis());
		publish(0, "system/up_time", json);
		json.clear();
		json.add((uint64_t) system_get_free_heap_size());
		publish(0, "system/heap_size", json);
		break;
	}
	default: {
		unhandled(hdr);
		break;
	}
	}
}
