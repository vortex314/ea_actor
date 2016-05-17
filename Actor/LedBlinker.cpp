/*
 * LedBlinker2.cpp
 *
 *  Created on: May 18, 2016
 *      Author: lieven
 */

#include "LedBlinker.h"
const int led = 16;

LedBlinker::LedBlinker(uint8_t pin) :
		Actor("Led") {
	_isOn = true;
	_pin = pin;
	_interval = 100;
}
ActorRef LedBlinker::create(int pin) {
	return ActorRef(new LedBlinker(pin));
}
void LedBlinker::init() {
	pinMode(_pin, OUTPUT);
	digitalWrite(_pin, 1);
}
void LedBlinker::blink() {
	if (_isOn) {
		_isOn = false;
		digitalWrite(_pin, 1);
	} else {
		_isOn = true;
		digitalWrite(_pin, 0);
	}
}
void LedBlinker::onReceive(Header hdr, Cbor& data) {
	if (hdr.event == INIT) {
		init();
		setReceiveTimeout(_interval);
	} else if (hdr.event == TIMEOUT) {
		blink();
		setReceiveTimeout(_interval);
	} else if (hdr.event == CONNECTED) {
		_interval = 500;
	} else if (hdr.event == DISCONNECTED) {
		_interval = 100;
	} else {
		unhandled(hdr);
	}
}
