/*
 * MqttFramer.cpp
 *
 *  Created on: Oct 31, 2015
 *      Author: lieven
 */

#include "MqttFramer.h"

MqttFramer::MqttFramer() :
		Actor("MqttFrame"), _msg(256) {
//	_msg = new MqttMsg(256);
	_msg.reset();
}

MqttFramer::~MqttFramer() {
}

ActorRef MqttFramer::create() {
	return ActorRef(new MqttFramer());
}
#define LOGHEADER(__hdr) LOGF("  %s => %s => %s ",Actor::idxToPath(__hdr._src),Actor::eventToString(__hdr._event),Actor::idxToPath(__hdr._dst))

void MqttFramer::onReceive(Header hdr, Cbor& cbor) {
	PT_BEGIN()
	PT_WAIT_UNTIL(hdr.is(INIT, ANY));
	while (true) {
		PT_YIELD();
//		LOGHEADER(hdr);
		switch (hdr._event) {
		case RXD: { // check data for full mqtt frame, forward when complete
			Bytes bytes(100);
			if (cbor.get(bytes)) {
				bytes.offset(0);
				while (bytes.hasData()) {
					uint8_t byte;
					byte = bytes.read();
//					LOGF(" byte rxd : %x ",byte);
					if (_msg.feed(byte)) { //MQTT message complete
						right().tell(
								Header(right(), self(), RXD, _msg.header()),
								_cborOut.putf("B", &_msg));
						_msg.reset();
					}
				}
			}
			break;
		}
		case REPLY(CONNECT): {
			right().tell(self(), REPLY(CONNECT), 0);
			break;
		}
		case REPLY(DISCONNECT): {
			right().tell(self(), REPLY(DISCONNECT), 0);
			break;
		}
		case REPLY(TXD): {
			right().tell(self(), REPLY(TXD), 0);
			break;
		}
		case DISCONNECT: {
			left().tell(self(), DISCONNECT, 0);
			break;
			}
		case TXD: {
			left().tell(Header(left(), self(), TXD, 0), cbor);
			break;
		}
		default: {
			unhandled(hdr);
			break;
		}
		}
	}
PT_END();
}
