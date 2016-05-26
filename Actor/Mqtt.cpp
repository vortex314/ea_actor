/*
 * Mqtt.cpp
 *
 *  Created on: 21-mei-2016
 *      Author: 600104947
 */

#include <Mqtt.h>
#include <MqttConstants.h>



Mqtt::~Mqtt() {
	// TODO Auto-generated destructor stub
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
		_framer.delegate(hdr, cbor);
	}
}
#define MQTT_TIME_RECONNECT 3000
void MqttConnector::onReceive(Header hdr, Cbor& cbor) {
	PT_BEGIN();
	DISCONNECTED: {
		while (true) {
			if (hdr.is(REPLY(CONNECT), E_OK))
				goto CONNECTED;
			setReceiveTimeout(MQTT_TIME_RECONNECT);
			_mqtt.tell(Header(_mqtt,_mqtt,TXD,0),connectMsg);
			PT_YIELD();
		}
	};
	CONNECTED :{
		while(true) {

		}
	}
	PT_END();
}
