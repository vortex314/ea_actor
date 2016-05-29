/*
 * Mqtt.h
 *
 *  Created on: 21-mei-2016
 *      Author: 600104947
 */

#ifndef MQTT_H_
#define MQTT_H_
#include <Actor.h>
#include <MqttMsg.h>
#include <MqttConstants.h>

#define TIME_KEEP_ALIVE 40000
#define TIME_PING 20000
#define MQTT_PUB_MAX_RETRIES 3
#define MQTT_TIME_WAIT_REPLY 1000
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_PONG),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_PING),CONNECT
 */
class Mqtt: public Actor {
	ActorRef _framer;
	ActorRef _connector;
	ActorRef _pinger;
	ActorRef _publisher;
	ActorRef _subscriber;

	Mqtt();
public:
	virtual ~Mqtt();
	void onReceive(Header, Cbor&);
	static ActorRef create();
};
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_PONG),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_PING),CONNECT
 */
class MqttPinger: public Actor {
	ActorRef _mqtt;
	MqttPinger(ActorRef mqtt);
	uint8_t _retries;
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header, Cbor&);
};
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_CONNACK),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_CONNECT),CONNECT
 */
class MqttConnector: public Actor {
	const char* _clientId;
	ActorRef _mqtt;
	MqttConnector(ActorRef mqtt);
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header, Cbor&);
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBACK,MQTT_PUBREC),REPLY(TXD)
 * OUT : TXD(MQTT_PUBLISH,MQTT_PUBREL)
 */
class MqttPublisher: public Actor {
	ActorRef _mqtt;
	Str _prefix;
	MqttPublisher(ActorRef mqtt);
	void sendPublish();
	void sendPubRel();
	enum State {
		ST_READY, ST_BUSY,
	} _state;
	Str _topic;
	Bytes _message;
	uint16_t _messageId;
	uint32_t _flags;
	uint16_t _retries;
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header, Cbor&);
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBLISH,MQTT_PUBREL),REPLY(TXD)
 * OUT : TXD(MQTT_PUBACK,MQTT_PUBREC)
 */
class MqttSubscriber: public Actor {
	ActorRef _mqtt;
	Str _prefix;
	MqttSubscriber(ActorRef mqtt);
	Str _topic;
	Bytes _message;
	uint32_t _flags;
	uint16_t _messageId;
	uint16_t _retries;
	void sendPubRec();
	void sendPubComp();
	void sendPubAck();
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header, Cbor&);
};

extern MqttMsg mqttIn;
extern MqttMsg mqttOut;

#endif /* MQTT_H_ */
