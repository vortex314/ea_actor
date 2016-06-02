/*
 * Config.cpp
 *
 *  Created on: 1-jun.-2016
 *      Author: 600104947
 */

#include <Config.h>

Config::Config(): Actor("Config") {
}

Config::~Config() {
}

void Config::init(){

}

void Config::onReceive(Header hdr, Cbor& data) {
	Str _key;
	Str _value;
	PT_BEGIN()
	;
	PT_WAIT_UNTIL(hdr.is(INIT));
	init();
	while (true) {
		PT_WAIT_UNTIL(hdr.is(CONFIG, RXD));

		tell(Header(hdr._src, hdr._dst, REPLY(CONFIG), RXD),
				_cborOut.putf("B", &_value));
		PT_WAIT_UNTIL(hdr.is(REPLY(PUBLISH),E_OK));
	}
PT_END()
}
