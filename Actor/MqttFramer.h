/*
 * MqttFramer.h
 *
 *  Created on: Oct 31, 2015
 *      Author: lieven
 */

#ifndef MQTTFRAMER_H_
#define MQTTFRAMER_H_

#include <Handler.h>
#include "MqttMsg.h"
#include "Stream.h"
#include "Tcp.h"

class MqttFramer: public Handler {
private :
	Tcp* _stream;
	MqttMsg* _msg;
public:
	IROM MqttFramer(Tcp* stream);
	IROM virtual ~MqttFramer();
	IROM bool isConnected();
	IROM void connect();
	IROM void disconnect();
	IROM void send(MqttMsg& msg);
	IROM bool dispatch(Msg& msg);
};

#endif /* MQTTFRAMER_H_ */
