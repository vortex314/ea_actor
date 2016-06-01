/*
 * Dwm1000.h
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */

#ifndef DWM1000_H_
#define DWM1000_H_

#include <Actor.h>

class Dwm1000: public Actor {
	ActorRef _mqtt;
	Dwm1000(ActorRef mqtt);
public:
	virtual ~Dwm1000();
	static ActorRef create(ActorRef mqtt) {
		return ActorRef(new Dwm1000(mqtt));
	}
	void init();
	void onReceive(Header hdr, Cbor& data);
	void publish(uint8_t qos,const char* key,Str& value);
};

#endif /* DWM1000_H_ */
