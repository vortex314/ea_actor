/*
 * Dwm1000.h
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */

#ifndef DWM1000_H_
#define DWM1000_H_

#include <Actor.h>

class System: public Actor {
	ActorRef _mqtt;
	System(ActorRef mqtt);
	bool _mqttConnected;
public:
	virtual ~System();
	static ActorRef create(ActorRef mqtt) {
		return ActorRef(new System(mqtt));
	}
	void init();
	void onReceive(Header hdr, Cbor& data);
	bool subscribed(Header hdr);
	void publish(uint8_t qos,const char* key,Str& value);
};

#endif /* DWM1000_H_ */
