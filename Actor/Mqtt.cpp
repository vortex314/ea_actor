/*
 * Mqtt.cpp
 *
 *  Created on: 21-mei-2016
 *      Author: 600104947
 */

#include <Mqtt.h>
#include <MqttConstants.h>
#include <MqttMsg.h>
#include <espmissingincludes.h>
#include <stdio.h>
extern "C" {
#include <user_interface.h>
}

MqttMsg mqttIn(256);
MqttMsg mqttOut(256);

Mqtt::Mqtt(const char *host, uint16_t port) :
		Actor("Mqtt") {
	_connector = MqttConnector::create(self());
	_publisher = ActorRef::dummy();
	_subscriber = ActorRef::dummy();
	_pinger = ActorRef::dummy();

}

Mqtt::~Mqtt() {
	// TODO Auto-generated destructor stub
}

ActorRef Mqtt::create(const char* host, uint16_t port) {
	return ActorRef(new Mqtt(host, port));
}
/*
 * #define MQTT_MSG_CONNECT       	(1<<4)
 #define MQTT_MSG_CONNACK       	(2<<4)
 #define MQTT_MSG_PUBLISH      	(3<<4)	// in:subscriber , out:publisher
 #define MQTT_MSG_PUBACK        	(4<<4)	// in:publisher, out : subscriber
 #define MQTT_MSG_PUBREC			(5<<4)	// in:publisher, out: subscriber
 #define MQTT_MSG_PUBREL        	(6<<4)	// in: subscriber,out : publisher
 #define MQTT_MSG_PUBCOMP       	(7<<4)	// in: publisher,out : subscriber
 #define MQTT_MSG_SUBSCRIBE    	(8<<4)
 #define MQTT_MSG_SUBACK       	(9<<4)
 #define MQTT_MSG_UNSUBSCRIBE  	(10<<4)
 #define MQTT_MSG_UNSUBACK     	(11<<4)
 #define MQTT_MSG_PINGREQ      	(12<<4)
 #define MQTT_MSG_PINGRESP   	(13<<4)
 #define MQTT_MSG_DISCONNECT   	(14<<4)
 */

void Mqtt::onReceive(Header hdr, Cbor& cbor) {
	if (hdr.is(RXD, ANY)) {
		switch (hdr._detail) {
		case MQTT_MSG_PINGREQ:
		case MQTT_MSG_PINGRESP: {
			_pinger.delegate(hdr, cbor);
			break;
		}
		case MQTT_MSG_PUBLISH:
		case MQTT_MSG_PUBREL:
		case MQTT_MSG_SUBACK:
		case MQTT_MSG_UNSUBACK: {
			_subscriber.delegate(hdr, cbor);
			break;
		}
		case MQTT_MSG_PUBACK:
		case MQTT_MSG_PUBCOMP: {
			_publisher.delegate(hdr, cbor);
			break;
		}
		case MQTT_MSG_DISCONNECT:
		case MQTT_MSG_CONNACK: {
			_connector.delegate(hdr, cbor);
			break;
		}
		default: {
			unhandled(hdr);
		}
		}
	} else if (hdr.is(REPLY(CONNECT), ANY)) {
		_connector.delegate(hdr, cbor);
	} else if (hdr.is(TXD, ANY)) {
		left().delegate(hdr, cbor);
	}
}
#define MQTT_TIME_RECONNECT 3000

MqttConnector::MqttConnector(ActorRef mqtt) :
		Actor("MqttConnector"), _clientId("nocl") {
	_mqtt = mqtt;
	sprintf((char*) _clientId, "ESP%X", system_get_chip_id());
}

ActorRef MqttConnector::create(ActorRef mqtt) {
	return ActorRef(new MqttConnector(mqtt));
}
Cbor out(256);
void MqttConnector::onReceive(Header hdr, Cbor& cbor) {
	Str onlineFalse("false");
	PT_BEGIN()
	if (hdr.is(INIT, E_OK)) {
		goto DISCONNECTED;
	}
	DISCONNECTED: {
		while (true) {
			PT_YIELD()
			;
			if (hdr.is(REPLY(CONNECT), E_OK))
				goto CONNECTED;
		}
	}
	CONNECTED: {
		while (true) {
			setReceiveTimeout(MQTT_TIME_RECONNECT);

			mqttOut.Connect(MQTT_QOS2_FLAG, _clientId, MQTT_CLEAN_SESSION,
					"system/alive", onlineFalse, "", "",
					TIME_KEEP_ALIVE / 1000);

			out.clear();
			out.add(mqttOut);
			_mqtt.tell(Header(_mqtt, _mqtt, TXD, 0),  out);
			PT_YIELD()
			;
			if (hdr.is(REPLY(DISCONNECT), E_OK))
				goto DISCONNECTED;
		}
	}
PT_END()
}
