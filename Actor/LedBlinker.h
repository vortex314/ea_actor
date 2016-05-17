/*
 * LedBlinker2.h
 *
 *  Created on: May 18, 2016
 *      Author: lieven
 */

#ifndef LEDBLINKER_H_
#define LEDBLINKER_H_

#include <Arduino.h>
#include <Actor.h>

class LedBlinker: public Actor {
	uint8_t _pin;
	bool _isOn;
	uint32_t _interval;
public:

	LedBlinker(uint8_t pin);
	static ActorRef create(int pin) ;
	void init() ;
	void blink();
	void onReceive(Header hdr, Cbor& data);
};

#endif /* LEDBLINKER_H_ */
