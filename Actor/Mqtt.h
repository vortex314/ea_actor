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

#define TIME_KEEP_ALIVE 2000
#define TIME_PING 1000
#define MQTT_PUB_MAX_RETRIES 3
#define MQTT_TIME_WAIT_REPLY 1000
#define MQTT_TOPIC_MAX_LENGTH 40
#define MQTT_VALUE_MAX_LENGTH	256
#define MQTT_MSG_MAX_LENGTH (MQTT_VALUE_MAX_LENGTH+MQTT_TOPIC_MAX_LENGTH)
/*
 * IN : INIT, CONFIG("host",port,...), RXD(MQTT_PONG),REPLY(TXD),REPLY(CONNECT)
 * OUT : TXD(MQTT_PING),CONNECT
 */


enum {
	PREFIX
} MqttConfigKey;

class Mqtt: public Actor {
	ActorRef _framer;
	ActorRef _connector;
	ActorRef _pinger;
	ActorRef _publisher;
	ActorRef _subscriber;
	Str _prefix;
	Mqtt(const char *prefix);
	static MqttMsg _mqttIn;
	static MqttMsg _mqttOut;
public:

	virtual ~Mqtt();
	void onReceive(Header, Cbor&);
	static ActorRef create(const char* prefix);
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
	Str _prefix;
	ActorRef _mqtt;
	MqttConnector(ActorRef mqtt,const char* prefix);
public:
	static ActorRef create(ActorRef mqtt,const char* prefix);
	void onReceive(Header, Cbor&);
	void init();
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBACK,MQTT_PUBREC),REPLY(TXD)
 * OUT : TXD(MQTT_PUBLISH,MQTT_PUBREL)
 */
class MqttPublisher: public Actor {
	ActorRef _mqtt;
	Str _prefix;
	MqttPublisher(ActorRef mqtt,const char* prefix);
	void sendPublish();
	void sendPubRel();
	uint8_t _qos;
	Str _topic;
	Bytes _message;
	uint16_t _messageId;
//	uint32_t _flags;
	uint16_t _retries;
public:
	static ActorRef create(ActorRef mqtt,const char* prefix);
	void onReceive(Header, Cbor&);
};
/*
 * IN : INIT, CONFIG("prefix"), RXD(MQTT_PUBLISH,MQTT_PUBREL),REPLY(TXD)
 * OUT : TXD(MQTT_PUBACK,MQTT_PUBREC)
 */
class MqttSubscriber: public Actor {
	ActorRef _mqtt;
	ActorRef _sender;

	MqttSubscriber(ActorRef mqtt,const char* prefix);
	Str _prefix;
	Str _topic;
	Bytes _message;
	uint32_t _flags;
	uint16_t _messageId;
	uint16_t _retries;
	uint8_t _qos;
	void sendPubRec();
	void sendPubComp();
	void sendPubAck();
	void sendSubscribe();
public:
	static ActorRef create(ActorRef mqtt,const char* prefix);
	void onReceive(Header, Cbor&);
};


#endif /* MQTT_H_ */
