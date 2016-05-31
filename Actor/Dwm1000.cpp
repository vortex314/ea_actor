/*
 * Dwm1000.cpp
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */

#include <Dwm1000.h>
#include <Json.h>

Dwm1000::Dwm1000(ActorRef mqtt) {
	_mqtt = mqtt;
}

Dwm1000::~Dwm1000() {

}

static Cbor out(100);
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
		_mqtt.tell(self(), PUBLISH, qos,
				out.putf("SB", "dwm1000/status", json));
		PT_WAIT_UNTIL(hdr.is(REPLY(PUBLISH),E_OK));
	}
PT_END()
}
