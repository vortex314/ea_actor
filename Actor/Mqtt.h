/*
 * Mqtt.h
 *
 *  Created on: 21-mei-2016
 *      Author: 600104947
 */

#ifndef MQTT_H_
#define MQTT_H_
#include <Actor.h>
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_PONG),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_PING),CONNECT
 */
class Mqtt : public Actor {
	ActorRef _framer;
	ActorRef _connector;
	ActorRef _pinger;
	ActorRef _publisher;
	ActorRef _subscriber;

	Mqtt(const char* host,uint16_t port);
public:
	virtual ~Mqtt();
	void onReceive(Header,Cbor&);
	static ActorRef create(const char* host, uint16_t port);
};
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_PONG),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_PING),CONNECT
 */
class MqttPinger : public Actor {
	ActorRef _mqtt;
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header,Cbor&);
};
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_CONNACK),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_CONNECT),CONNECT
 */
class MqttConnector : public Actor {
	const char* _host;
	const uint16_t _port;
	ActorRef _mqtt;
	MqttConnector();
public:
	static ActorRef create(ActorRef mqtt,const char* host, uint16_t port);
	void onReceive(Header,Cbor&);
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBACK,MQTT_PUBREC),REPLY(TXD)
 * OUT : TXD(MQTT_PUBLISH,MQTT_PUBREL)
 */
class MqttPublisher : public Actor {
	ActorRef _mqtt;
	Str _prefix;
	MqttPublisher();
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header,Cbor&);
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBLISH,MQTT_PUBREL),REPLY(TXD)
 * OUT : TXD(MQTT_PUBACK,MQTT_PUBREC)
 */
class MqttSubscriber : public Actor {
	ActorRef _mqtt;
	Str _prefix;
	MqttSubscriber();
public:
	static ActorRef create(ActorRef mqtt);
	void onReceive(Header,Cbor&);
};

#endif /* MQTT_H_ */
