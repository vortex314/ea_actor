/*
 * Dwm1000.cpp
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */
#include <Arduino.h>
#include <Dwm1000.h>
#include <Json.h>

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
	_mqtt.tell(self(), PUBLISH, qos, out.putf("sB", key, &value));
}

void Dwm1000::onReceive(Header hdr, Cbor& data) {
	Json json(20);
	if (hdr.is(INIT))
		init();
	PT_BEGIN()
	;
	while (true) {
		setReceiveTimeout(2000);
		PT_WAIT_UNTIL(hdr.is(TIMEOUT));

		json.add((uint64_t) millis());
		publish(0, "system/time", json);
		PT_WAIT_UNTIL(hdr.is(REPLY(PUBLISH),E_OK));
	}
PT_END()
}
