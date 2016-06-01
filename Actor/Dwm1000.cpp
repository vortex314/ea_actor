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

Dwm1000::Dwm1000(ActorRef mqtt) {
	_mqtt = mqtt;
}

Dwm1000::~Dwm1000() {

}

void Dwm1000::publish(uint8_t qos,const char* key,Str& value) {
	_mqtt.tell(self(), PUBLISH, qos,
					out.putf("sB", key, value));
}



void Dwm1000::onReceive(Header hdr, Cbor& data) {
	PT_BEGIN()
	;
	PT_WAIT_UNTIL(hdr.is(INIT));
	init();
	while (true) {
		setReceiveTimeout(2000);
		PT_WAIT_UNTIL(hdr.is(TIMEOUT));
		int qos = 0;
		Json json(20);
		json.add(millis());
		publish(0,Str("system/time"),json);
		PT_WAIT_UNTIL(hdr.is(REPLY(PUBLISH),E_OK));
	}
PT_END()
}
