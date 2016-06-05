/*
 * Dwm1000.cpp
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */
#include <Arduino.h>
#include <Dwm1000.h>
#include <Json.h>
extern "C" {
#include "user_interface.h"
}

static Cbor out(100);

Dwm1000::Dwm1000(ActorRef mqtt) :
		Actor("DWM1000") {
	_mqtt = mqtt;
}

Dwm1000::~Dwm1000() {

}

void Dwm1000::init() {

}

void Dwm1000::publish(uint8_t qos, const char* key, Str& value) {
	_mqtt.tell(self(), PUBLISH, qos, _cborOut.putf("sB", key, &value));
}

void Dwm1000::onReceive(Header hdr, Cbor& data) {
	Json json(20);

	switch (hdr._event) {
	case INIT: {
		init();
		setReceiveTimeout(10000);
		break;
	}
	case TIMEOUT: {
		setReceiveTimeout(1000);
		json.add((uint64_t) millis());
		publish(0, "system/up_time", json);
		json.clear();
		json.add((uint64_t) system_get_free_heap_size());
		publish(0, "system/heap_size", json);
		break;
	}
	case REPLY(CONNECT): {
		break;
	}
	default: {
		unhandled(hdr);
		break;
	}
	}
}
