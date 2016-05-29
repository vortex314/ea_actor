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
Cbor out(256);

//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
Mqtt::Mqtt() :
		Actor("Mqtt") {
	_connector = MqttConnector::create(self());
	_publisher = MqttPublisher::create(self());
	_subscriber = MqttSubscriber::create(self());
	_pinger = MqttPinger::create(self());
}

Mqtt::~Mqtt() {
}

ActorRef Mqtt::create() {
	return ActorRef(new Mqtt());
}

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
			_subscriber.tell(hdr, cbor);
			break;
		}
		case MQTT_MSG_PUBACK:
		case MQTT_MSG_PUBCOMP: {
			_publisher.tell(hdr, cbor);
			break;
		}
		case MQTT_MSG_DISCONNECT:
		case MQTT_MSG_CONNACK: {
			_connector.tell(hdr, cbor);
			break;
		}
		default: {
			unhandled(hdr);
		}
		}
	} else if (hdr.matches(self(), _connector, REPLY(CONNECT), 0)) { // CONNACK received
		_pinger.tell(hdr, cbor);
		_publisher.tell(hdr, cbor);
		_subscriber.tell(hdr, cbor);
		right().tell(hdr, cbor);
	} else if (hdr.matches(self(), left(), REPLY(CONNECT), 0)) { // TCP conected
		_connector.tell(hdr, cbor);
	} else if (hdr.matches(self(), left(), REPLY(DISCONNECT), 0)) { // TCP disconnected
		_connector.tell(hdr, cbor);
		_pinger.tell(hdr, cbor);
		_publisher.tell(hdr, cbor);
		_subscriber.tell(hdr, cbor);
		right().tell(hdr, cbor);
	} else if (hdr.is(DISCONNECT)) { // pinger asks to disconnect after ping failures
		left().tell(self(), DISCONNECT, 0);
	} else if (hdr.is(TXD, ANY)) {	// child wants to transmit, send to framer
		hdr._src = self().idx();
		left().tell(hdr, cbor);
	}
}
#define MQTT_TIME_RECONNECT 5000
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttConnector::MqttConnector(ActorRef mqtt) :
		Actor("MqttConnector"), _clientId("nocl") {
	_mqtt = mqtt;
	sprintf((char*) _clientId, "ESP%X", system_get_chip_id());
}

ActorRef MqttConnector::create(ActorRef mqtt) {
	return ActorRef(new MqttConnector(mqtt));
}

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
			_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
			PT_YIELD()
			;
			if (hdr.is(REPLY(DISCONNECT), E_OK))
				goto DISCONNECTED;
			if (hdr.is(RXD, MQTT_MSG_CONNACK)) {
				_mqtt.tell(self(), REPLY(CONNECT), E_OK);
				goto MQTT_CONNECTED;
			}
		}
	}
	MQTT_CONNECTED: {
		setReceiveTimeout(UINT32_MAX);
		while (true) {
			PT_YIELD()
			;
			if (hdr.is(REPLY(DISCONNECT), E_OK))
				goto DISCONNECTED;
		}
	}
PT_END()
}
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttPinger::MqttPinger(ActorRef mqtt) :
	Actor("MqttPinger"), _retries(0) {
_mqtt = mqtt;
}

ActorRef MqttPinger::create(ActorRef mqtt) {
return ActorRef(new MqttPinger(mqtt));
}

void MqttPinger::onReceive(Header hdr, Cbor& cbor) {

PT_BEGIN()
PT_WAIT_UNTIL(hdr.is(INIT));
DISCONNECTED: {
	setReceiveTimeout(UINT32_MAX);
	while (true) {
		PT_YIELD()
		;
		if (hdr.is(REPLY(CONNECT)))
			goto PINGING;
	}
}
WAITING: { // wait between PING's
	while (true) {
		setReceiveTimeout((TIME_KEEP_ALIVE / 3));
		PT_WAIT_UNTIL(hdr.matches(self(), _mqtt, RXD, ANY) || hdr.is(TIMEOUT));
		if (hdr.is(TIMEOUT)) {
			goto PINGING;
		}
		if (hdr.is(DISCONNECT))
			goto DISCONNECTED;
	}

}
PINGING: {
	_retries = 1;
	while (true) {
		mqttOut.PingReq();
		out.clear();
		out.add(mqttOut);
		_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
		setReceiveTimeout(TIME_PING);

		PT_YIELD_UNTIL(hdr.is(RXD,MQTT_MSG_PINGRESP)||hdr.is(TIMEOUT,ANY));

		if (hdr.is(RXD, MQTT_MSG_PINGRESP)) {
			goto WAITING;
		} else {
			_retries++;
			if (_retries > 3)
				_mqtt.tell(self(), DISCONNECT, 0);
		};
		if (hdr.is(DISCONNECT))
			goto DISCONNECTED;
	}
}

PT_END()
;
}
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttPublisher::MqttPublisher(ActorRef mqtt) :
Actor("MqttPublisher") {
_mqtt = mqtt;
_flags = 0;
_messageId = 0;
_retries = 0;
}

ActorRef MqttPublisher::create(ActorRef mqtt) {
return ActorRef(new MqttPublisher(mqtt));
}

void MqttPublisher::sendPublish() {
uint8_t header = 0;
if ((_flags & MQTT_QOS_MASK) == MQTT_QOS0_FLAG) {
_state = ST_READY;
} else if ((_flags & MQTT_QOS_MASK) == MQTT_QOS1_FLAG) {
header += MQTT_QOS1_FLAG;
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
} else if ((_flags & MQTT_QOS_MASK) == MQTT_QOS2_FLAG) {
header += MQTT_QOS2_FLAG;
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
}
if (_flags & MQTT_RETAIN_FLAG)
header += MQTT_RETAIN_FLAG;
if (_retries) {
header += MQTT_DUP_FLAG;
}
mqttOut.Publish(header, _topic, _message, _messageId);
out.clear();
out.add(mqttOut);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
}

void IROM
MqttPublisher::sendPubRel() {
mqttOut.PubRel(_messageId);
out.clear();
out.add(mqttOut);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
}

void MqttPublisher::onReceive(Header hdr, Cbor& cbor) {
PT_BEGIN()
PT_WAIT_UNTIL(hdr.is(INIT));

DISCONNECTED: {
while (true) {
	PT_YIELD()
	;
	if (hdr.is(REPLY(CONNECT)))
		goto READY;
}
}
READY: {
PT_YIELD()
;
_messageId++;
if ((_flags & MQTT_QOS_MASK) == MQTT_QOS0_FLAG) {
	sendPublish();
//	PT_YIELD_UNTIL(msg.is(_mqtt._framer, SIG_TXD));
//	Msg::publish(this, SIG_ERC, 0);
	goto READY;
} else if ((_flags & MQTT_QOS_MASK) == MQTT_QOS1_FLAG) {
	goto QOS1_ACK;
} else if ((_flags & MQTT_QOS_MASK) == MQTT_QOS2_FLAG) {
	goto QOS2_REC;
}
goto READY;
}

QOS1_ACK: {
// INFO("QOS1_ACK");
for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
	sendPublish();
	WAIT_REPLY2: setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
	PT_YIELD_UNTIL(hdr.is(RXD,MQTT_MSG_PUBACK) || hdr.is(TIMEOUT));
	if (hdr.is(RXD, MQTT_MSG_PUBACK)) {
		int mqttType, id;
		cbor.get(mqttType); // skip type in  <src><signal><type><msgId><qos><topic><value>
		cbor.get(id);
//		INFO(" messageId compare %d : %d ", id, _messageId);
		if (id == _messageId) {
//			Msg::publish(this, SIG_ERC, 0);
			goto READY;
		} else {
			goto WAIT_REPLY2;
		}
	}
}
//Msg::publish(this, SIG_ERC, ETIMEDOUT);
goto READY;
}

QOS2_REC: {
// INFO("QOS2_REC");
for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
	sendPublish();
	WAIT_REPLY: setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
	PT_YIELD_UNTIL(hdr.is(RXD,MQTT_MSG_PUBREC) || hdr.is(TIMEOUT));
	if (hdr.is(RXD, MQTT_MSG_PUBREC)) {
		int mqttType, id;
		cbor.get(mqttType);
//				INFO(" messageId compare %d : %d ",id,_messageId);
		cbor.get(id);
//		INFO(" messageId compare %d : %d ", id, _messageId);
		if (id == _messageId) {
			goto QOS2_COMP;
		} else {	// wrong PUBREC, don't resend
			goto WAIT_REPLY;
		}
	}
}
//Msg::publish(this, SIG_ERC, ETIMEDOUT);
goto READY;
}

QOS2_COMP: {
// INFO("QOS2_COMP");
for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
	sendPubRel();
	WAIT_REPLY1: setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
	PT_YIELD_UNTIL(hdr.is(RXD,MQTT_MSG_PUBCOMP) || hdr.is(TIMEOUT));
	if (hdr.is(RXD, MQTT_MSG_PUBCOMP)) {
		int mqttType, id;
		cbor.get(mqttType);
		if (cbor.get(id) && id == _messageId) {
//			Msg::publish(this, SIG_ERC, 0);
			goto READY;
		} else {	// wrong pubcomp, don't resend PUBREL
			goto WAIT_REPLY1;
		}
	}
}
//Msg::publish(this, SIG_ERC, ETIMEDOUT);
goto READY;
}
PT_END()
}
//________________________________________________________________________________________________________
//
MqttSubscriber::MqttSubscriber(ActorRef mqtt) :
Actor("MqttPublisher") {
_mqtt = mqtt;
}

ActorRef MqttSubscriber::create(ActorRef mqtt) {
return ActorRef(new MqttSubscriber(mqtt));
}

void MqttSubscriber::onReceive(Header hdr, Cbor& cbor) {
PT_BEGIN()
PT_WAIT_UNTIL(hdr.is(INIT));
DISCONNECTED: {
while (true) {
PT_YIELD()
;
}
}
PT_END()
}

void MqttSubscriber::sendPubRec() {
mqttOut.PubRec(_messageId);
out.clear();
out.add(mqttOut);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
}

void MqttSubscriber::sendPubComp() {
mqttOut.PubComp(_messageId);
out.clear();
out.add(mqttOut);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
}

void MqttSubscriber::sendPubAck() {
mqttOut.PubAck(_messageId);
out.clear();
out.add(mqttOut);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), out);
}

