/*
 * MqttFramer.h
 *
 *  Created on: Oct 31, 2015
 *      Author: lieven
 */

#ifndef MQTTFRAMER_H_
#define MQTTFRAMER_H_

#include <Actor.h>
#include "MqttMsg.h"
#include "Stream.h"
#include "Tcp.h"

class MqttFramer: public Actor {
private:
	MqttMsg _msg;
	MqttFramer();
public:

	virtual ~MqttFramer();
	void onReceive(Header hdr, Cbor& cbor);
	static ActorRef create();
};

#endif /* MQTTFRAMER_H_ */
