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

MqttMsg _mqttIn(MQTT_MSG_MAX_LENGTH);
MqttMsg _mqttOut(MQTT_MSG_MAX_LENGTH);


//________________________________________________________________________________________________________
//
Mqtt::Mqtt(ActorRef framer,const char* prefix) :
		Actor("Mqtt") {
	_prefix = prefix;
	_framer=framer;
	_connector = MqttConnector::create(framer,self(), prefix);
	_publisher = MqttPublisher::create(framer,self(), prefix);
	_subscriber = MqttSubscriber::create(framer,self(), prefix);
	_pinger = MqttPinger::create(framer,self());
	_connected = false;
}
//________________________________________________________________________________________________________
//
Mqtt::~Mqtt() {
}
//________________________________________________________________________________________________________
//
ActorRef Mqtt::create(ActorRef framer,const char* prefix) {
	return ActorRef(new Mqtt(framer,prefix));
}
//________________________________________________________________________________________________________
//
bool Mqtt::subscribed(Header hdr) {
	return Actor::subscribed(hdr) ;
}
//________________________________________________________________________________________________________
//
void Mqtt::onReceive(Header hdr, Cbor& cbor) {

	if (hdr.is(SUBSCRIBE)) {
		_subscriber.delegate(hdr, cbor);
	} else if (hdr.is(PUBLISH)) {
		if (_connected)
			_publisher.delegate(hdr, cbor);
		else
			LOGF("disconnected");
	} else if (hdr.is(RXD, ANY)) {
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
			break;
		}
		}
	} else if (hdr.is(self(), REPLY(CONNECT))) { // CONNACK received, broadcast
		_connected = true;
	} else if (hdr.is(_framer, REPLY(CONNECT))) { // TCP connected
		_connector.delegate(hdr, cbor);
	} else if (hdr.is( _framer, REPLY(DISCONNECT))) { // TCP disconnected
		_connected = false;
		_connector.delegate(hdr, cbor);
	} else if (hdr.is(DISCONNECT)) { // pinger asks to disconnect after ping failures
		_framer.tell(self(), DISCONNECT, 0);
	} else if (hdr.is(TXD, ANY)) { // child wants to transmit, send to framer
		hdr.src(self());
		_framer.tell(hdr, cbor);
	}
}
#define MQTT_TIME_RECONNECT 5000
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttConnector::MqttConnector(ActorRef framer,ActorRef mqtt, const char* prefix) :
		Actor("MqttConnector"), _clientId("nocl"), _prefix(20) {
	_mqtt = mqtt;
	_prefix = prefix;
	_framer=framer;

}

void MqttConnector::init() {
/*	ActorRef::byPath("Config").tell(self(), CONFIG, RXD,
			_cborOut.putf("s", "mqtt/clientId"));*/
	sprintf((char*) _clientId, "ESP%X", system_get_chip_id());
}

ActorRef MqttConnector::create(ActorRef framer,ActorRef mqtt, const char* prefix) {
	return ActorRef(new MqttConnector( framer,mqtt, prefix));
}

bool MqttConnector::subscribed(Header hdr) {
	return Actor::subscribed(hdr) ;
}

void MqttConnector::onReceive(Header hdr, Cbor& cbor) {
	Str onlineFalse("false");
	Str lwtTopic(60);
	lwtTopic.append(_prefix).append("system/online");
	PT_BEGIN()
		PT_WAIT_UNTIL(hdr.is(INIT, E_OK));
		init();
	DISCONNECTED: {
		PT_WAIT_UNTIL(hdr.is(_framer,REPLY(CONNECT)));
		}
	TCP_CONNECTED: {
		while(true) {
			setReceiveTimeout(MQTT_TIME_RECONNECT);

			_mqttOut.Connect(MQTT_QOS2_FLAG, _clientId, MQTT_CLEAN_SESSION,
					lwtTopic.c_str(), onlineFalse, "", "",
					TIME_KEEP_ALIVE / 1000);
			_framer.tell(Header(_mqtt, self(), TXD, 0),
					_cborOut.putf("B", &_mqttOut));
			PT_YIELD();
			if (hdr.is(RXD, MQTT_MSG_CONNACK)) {
				broadcast(_mqtt,REPLY(CONNECT),0);
				goto MQTT_CONNECTED;
			} else if (hdr.is(REPLY(DISCONNECT))) {
				broadcast(_mqtt,REPLY(DISCONNECT),0);
				goto DISCONNECTED;
			} else if ( hdr.is(TIMEOUT)){
				_framer.tell(_mqtt,DISCONNECT,0);
				broadcast(_mqtt,REPLY(DISCONNECT),0);
				}
			}
		}
	MQTT_CONNECTED: {
		setReceiveTimeout(UINT32_MAX);
		PT_WAIT_UNTIL(hdr.is(_framer,REPLY(DISCONNECT)));
		broadcast(_mqtt,REPLY(DISCONNECT),0);
		goto DISCONNECTED;
	}
PT_END()
}
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttPinger::MqttPinger(ActorRef framer,ActorRef mqtt) :
	Actor("MqttPinger"), _retries(0) {
_mqtt = mqtt;
_framer=framer;

}

ActorRef MqttPinger::create(ActorRef framer,ActorRef mqtt) {
return ActorRef(new MqttPinger( framer,mqtt));
}

bool MqttPinger::subscribed(Header hdr) {
	return Actor::subscribed(hdr) ;
}

void MqttPinger::onReceive(Header hdr, Cbor& cbor) {

PT_BEGIN()
PT_WAIT_UNTIL(hdr.is(INIT));
DISCONNECTED:
PT_WAIT_UNTIL(hdr.is(REPLY(CONNECT)));
goto PINGING;
WAITING: { // wait between PING's
	setReceiveTimeout((TIME_KEEP_ALIVE / 3));
	PT_WAIT_UNTIL(hdr.is(_mqtt,REPLY(DISCONNECT)) || hdr.is(TIMEOUT));
	if (hdr.is(_mqtt,REPLY(DISCONNECT)))
		goto DISCONNECTED;
}
PINGING: {	// retyr 3 ping's max
	_retries = 1;
	while (_retries < 3) {

		_mqttOut.PingReq();
		_framer.tell(self(), TXD, 0, _cborOut.putf("B", (Bytes*) &_mqttOut));
		setReceiveTimeout(TIME_PING);

		PT_YIELD_UNTIL(
				hdr.is(RXD, MQTT_MSG_PINGRESP) || hdr.is(TIMEOUT, ANY) || hdr.is(DISCONNECT));

		if (hdr.is(RXD, MQTT_MSG_PINGRESP)) {
			goto WAITING;
		} else if (hdr.is(TIMEOUT)) {
			_retries++;
		} else if (hdr.is(DISCONNECT))
			goto DISCONNECTED;
	}
	_framer.tell(self(), DISCONNECT, 0);
	broadcast(_mqtt,REPLY(DISCONNECT),0);
	LOGF(" ping timeouts retries reached, disconnecting ... ");
	goto DISCONNECTED;
}

PT_END()
}
//________________________________________________________________________________________________________
//
//________________________________________________________________________________________________________
//
MqttPublisher::MqttPublisher(ActorRef framer,ActorRef mqtt, const char* prefix) :
Actor("MqttPublisher"), _prefix(20), _topic(MQTT_TOPIC_MAX_LENGTH), _message(
		MQTT_VALUE_MAX_LENGTH) {
_mqtt = mqtt;
_qos = 0;
_messageId = 0;
_retries = 0;
_qos = 0;
_prefix = prefix;
_framer=framer;

}

ActorRef MqttPublisher::create(ActorRef framer,ActorRef mqtt, const char* prefix) {
return ActorRef(new MqttPublisher(framer,mqtt, prefix));
}

void MqttPublisher::sendPublish() {
	uint8_t header = 0;
	 if (_qos  == MQTT_QOS1_FLAG) {

		header += MQTT_QOS1_FLAG;
		setReceiveTimeout(MQTT_TIME_WAIT_REPLY);

	} else if ((_qos & MQTT_QOS_MASK) == MQTT_QOS2_FLAG) {

		header += MQTT_QOS2_FLAG;
		setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
	}

	if (_qos & MQTT_RETAIN_FLAG)

		header += MQTT_RETAIN_FLAG;

	if (_retries) {

		header += MQTT_DUP_FLAG;
	}

	Str _fullTopic(60);
	_fullTopic = _prefix;
	_fullTopic.append(_topic);

	_mqttOut.Publish(header, _fullTopic, _message, _messageId);
	_framer.tell(Header(_framer, _mqtt, TXD, 0), _cborOut.putf("B", &_mqttOut));
//	LOGF(" TOPIC %s ",_fullTopic.c_str());
}

void MqttPublisher::sendPubRel() {
	_mqttOut.PubRel(_messageId);
	_framer.tell(Header(_framer, _mqtt, TXD, 0), _cborOut.putf("B", &_mqttOut));
}

bool MqttPublisher::subscribed(Header hdr) {
	return Actor::subscribed(hdr) ;
}


void MqttPublisher::onReceive(Header hdr, Cbor& cbor) {

	PT_BEGIN()

	PT_WAIT_UNTIL(hdr.is(INIT));

	DISCONNECTED:
		LOGF("DISCONNECTED");
		PT_WAIT_UNTIL(hdr.is(_mqtt,REPLY(CONNECT)));
	READY: {
//		LOGF("READY");
		PT_YIELD();
		if ( hdr.is(REPLY(DISCONNECT))) {
			goto DISCONNECTED;
		} else if ( hdr.is(PUBLISH)) {
			_messageId++;
			_qos = hdr._detail;
			ASSERT_LOG(cbor.scanf("SB",&_topic,&_message));

			if ((_qos & MQTT_QOS_MASK)   == MQTT_QOS0_FLAG) {
				sendPublish();
			} else if ((_qos & MQTT_QOS_MASK) == MQTT_QOS1_FLAG) {
				goto QOS1_ACK;
			} else if ((_qos & MQTT_QOS_MASK) == MQTT_QOS2_FLAG) {	// INFO("QOS2_COMP");
				goto QOS2_REC;
			}
		}
		goto READY;
	}

	QOS1_ACK: {
		LOGF("QOS1_ACK");
		for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
			sendPublish();
			setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
			while(true) {
				PT_YIELD();
				if (hdr.is(RXD, MQTT_MSG_PUBACK)) {
					ASSERT_LOG(cbor.scanf("B",&_mqttIn));
					if (_mqttIn.parse() &&  (_mqttIn.messageId() == _messageId)) {
						goto READY;
						}
					}
				 else if ( hdr.is(TIMEOUT)) {
					break;
				 	 }
				}
			}
		LOGF(" NO ACK ");
		goto READY;
		}


	QOS2_REC: {
		LOGF("QOS2_REC");
		for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
			sendPublish();
			setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
			PT_YIELD();
			if (hdr.is(RXD, MQTT_MSG_PUBREC)) {
				ASSERT_LOG(cbor.scanf("B",&_mqttIn));
				if (_mqttIn.parse() &&  (_mqttIn.messageId() == _messageId)) {
					goto READY;
					}
				} else if ( hdr.is(REPLY(DISCONNECT))) {
					goto DISCONNECTED;
				}
			}
		goto READY;
		}

	QOS2_COMP:  {
		LOGF("QOS2_REC");
		for (_retries = 0; _retries < MQTT_PUB_MAX_RETRIES; _retries++) {
			sendPubRel();
			setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
			PT_YIELD();
			if (hdr.is(RXD, MQTT_MSG_PUBCOMP)) {
				ASSERT_LOG(cbor.scanf("B",&_mqttIn));
				if (_mqttIn.parse() &&  (_mqttIn.messageId() == _messageId)) {
					goto READY;
					}
				} else if ( hdr.is(REPLY(DISCONNECT))) {
					goto DISCONNECTED;
				}
			}
		goto READY;
		}
PT_END()
}
//________________________________________________________________________________________________________
//
MqttSubscriber::MqttSubscriber(ActorRef framer,ActorRef mqtt, const char* prefix) :
Actor("MqttSubscriber"), _prefix(20), _topic(MQTT_TOPIC_MAX_LENGTH), _message(
	MQTT_VALUE_MAX_LENGTH) {
_qos = 0;
_messageId = 1;
_retries = 0;
_mqtt = mqtt;
_qos = 0;
_prefix = prefix;
_framer=framer;

}

ActorRef MqttSubscriber::create(ActorRef framer,ActorRef mqtt, const char* prefix) {
return ActorRef(new MqttSubscriber(framer,mqtt, prefix));
}

bool MqttSubscriber::subscribed(Header hdr) {
	return Actor::subscribed(hdr) ;
}

void MqttSubscriber::onReceive(Header hdr, Cbor& cbor) {

PT_BEGIN()
;
PT_WAIT_UNTIL(hdr.is(INIT));
DISCONNECTED:
PT_WAIT_UNTIL(hdr.is(REPLY(CONNECT)));
READY: {
PT_WAIT_UNTIL(
	hdr.is(SUBSCRIBE) || hdr.is(RXD, MQTT_MSG_PUBLISH) || hdr.is(REPLY(DISCONNECT)));
if (hdr.is(SUBSCRIBE))
goto SUBSCRIBING;
if (hdr.is(REPLY(DISCONNECT)))
goto DISCONNECTED;
if (hdr.is(RXD, MQTT_MSG_PUBLISH)) {
_topic.clear();
_message.clear();
cbor.offset(0);
cbor.scanf("B", &_mqttIn);
if (_mqttIn.parse()) {
	_topic = *_mqttIn.topic();
	_message = *_mqttIn.message();
	_qos = _mqttIn.qos();
} else
	goto READY;
if (_qos == MQTT_QOS0_FLAG) {
	right().tell(Header(right(), self(), REPLY(SUBSCRIBE), 0),
			_cborOut.putf("SB", &_topic, &_message));
	goto READY;
} else if (_qos == MQTT_QOS1_FLAG) {
	sendPubAck();
	right().tell(Header(right(), self(), REPLY(SUBSCRIBE), 0),
			_cborOut.putf("SB", &_topic, &_message));
	goto READY;
} else if (_qos == MQTT_QOS2_FLAG) {

	goto QOS2_REC;
}
}
}
QOS2_REC: {
_retries = 0;
while (_retries < MQTT_PUB_MAX_RETRIES) {
sendPubRec();
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
PT_WAIT_UNTIL(hdr.is(TIMEOUT) || hdr.is(RXD, MQTT_MSG_PUBREL));
if (hdr.is(TIMEOUT)) {
	_retries++;

} else if (hdr.is(RXD, MQTT_MSG_SUBACK)) {
	right().tell(Header(right(), self(), REPLY(SUBSCRIBE), 0),
			_cborOut.putf(""));
	goto READY;
}

}
right().tell(Header(right(), self(), REPLY(SUBSCRIBE), EAGAIN),
	_cborOut.putf(""));
}
SUBSCRIBING: {
if (cbor.scanf("uSB", &_qos, &_topic, &_message) == false) {
LOGF("invalid subscribe args");
goto READY;
}
_retries = 0;
while (_retries < MQTT_PUB_MAX_RETRIES) {
sendSubscribe();
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
PT_WAIT_UNTIL(hdr.is(TIMEOUT) || hdr.is(RXD, MQTT_MSG_SUBACK));
if (hdr.is(TIMEOUT)) {
	_retries++;
} else if (hdr.is(RXD, MQTT_MSG_SUBACK)) {
	right().tell(Header(right(), self(), REPLY(SUBSCRIBE), 0),
			_cborOut.putf(""));
	goto READY;
} else {
	unhandled(hdr);
}
}
right().tell(Header(right(), _mqtt, REPLY(SUBSCRIBE), E_TIMEOUT),
	_cborOut.putf(""));
}
PT_END()
}

void MqttSubscriber::sendPubRec() {
_mqttOut.PubRec(_messageId);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), _cborOut.putf("B", &_mqttOut));
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
}

void MqttSubscriber::sendPubComp() {
_mqttOut.PubComp(_messageId);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), _cborOut.putf("B", &_mqttOut));
setReceiveTimeout(MQTT_TIME_WAIT_REPLY);
}

void MqttSubscriber::sendPubAck() {
_mqttOut.PubAck(_messageId);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), _cborOut.putf("B", &_mqttOut));
}

void MqttSubscriber::sendSubscribe() {
_mqttOut.Subscribe(_topic, _messageId, MQTT_QOS1_FLAG);
_mqtt.tell(Header(_mqtt, self(), TXD, 0), _cborOut.putf("B", &_mqttOut));
}

