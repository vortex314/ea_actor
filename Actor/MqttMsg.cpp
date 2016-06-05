/*
 * MqttMsg.cpp
 *
 *  Created on: 22-jun.-2013
 *      Author: lieven2
 */

#include "MqttMsg.h"
#include <string.h>
#include <cstring>

const char* const MqttNames[] = { "UNKNOWN", "CONNECT", "CONNACK", "PUBLISH",
		"PUBACK", "PUBREC", "PUBREL", "PUBCOMP", "SUBSCRIBE", "SUBACK",
		"UNSUBSCRIBE", "UNSUBACK", "PINGREQ", "PINGRESP", "DISCONNECT" };
const char* const QosNames[] = { "QOS0", "QOS1", "QOS2" };

// #define LOG(x) std::cout << Sys::upTime() << " | MQTT OUT " << x << std::endl
#define LOG(x) INFO(x)

Str logLine(300);

 MqttMsg::MqttMsg(uint32_t size) :
		Bytes(size), _prefix(30), _message(0), _topic(0), _user(0), _password(
				0), _clientId(0) {
}

void  MqttMsg::prefix(Str& prefix) {
	_prefix.clear();
	_prefix << prefix;
}

void  MqttMsg::addHeader(int hdr) {
	clear();
	write(hdr);
}

void  MqttMsg::addRemainingLength(uint32_t length) {
	do {
		uint8_t digit = length & 0x7F;
		length /= 128;
		if (length > 0) {
			digit |= 0x80;
		}
		write(digit);
	} while (length > 0);
}

void  MqttMsg::addUint16(uint16_t value) {
	write(value >> 8);
	write(value & 0xFF);
}

void  MqttMsg::addString(const char *str) {
	addUint16(strlen(str));
	addBytes((uint8_t*) str, strlen(str));
}

void  MqttMsg::addStr(Str& str) {
	addUint16(str.length());
	addBytes(str);
}

void  MqttMsg::addComposedString(Str& prefix, Str &str) {
	addUint16(prefix.length() + str.length());
	addBytes(prefix);
	addBytes(str);
}

void  MqttMsg::addMessage(uint8_t* src, uint32_t length) {
	addUint16(length);
	addBytes(src, length);
}

void  MqttMsg::addBytes(Bytes& bytes) {
	bytes.offset(0);
	for (uint32_t i = 0; i < bytes.length(); i++)
		write(bytes.read());
}

void  MqttMsg::addBytes(uint8_t* bytes, uint32_t length) {
	for (uint32_t i = 0; i < length; i++)
		write(*bytes++);
}
#define MQTT_VERSION 0x04

void  MqttMsg::Connect(uint8_t hdr, const char *clientId,
		uint8_t connectFlag, const char *willTopic, Bytes& willMsg,
		const char *username, const char* password, uint16_t keepAlive) {

	uint8_t connectFlags = connectFlag;
	uint16_t remainLen = 0;

	uint16_t clientidlen = strlen(clientId);
	uint16_t usernamelen = strlen(username);
	uint16_t passwordlen = strlen(password);
	uint16_t willTopicLen = strlen(willTopic) + _prefix.length();
	uint16_t willMsgLen = willMsg.length();

	remainLen = 10 + clientidlen + 2; // header MUST length + client id
	// Preparing the connectFlags
	if (usernamelen) {
		remainLen += usernamelen + 2;
		connectFlags |= MQTT_USERNAME_FLAG;
	}
	if (passwordlen) {
		remainLen += passwordlen + 2;
		connectFlags |= MQTT_PASSWORD_FLAG;
	}
	if (willTopicLen) {
		remainLen += willTopicLen + 2;
		remainLen += willMsgLen + 2;
		connectFlags |= MQTT_WILL_FLAG;
	}

	addHeader(MQTT_MSG_CONNECT);
	addRemainingLength(remainLen);
	addString("MQTT");
	write(MQTT_VERSION);
	write(connectFlags);
	addUint16(keepAlive);
	addString(clientId);

	if (willTopicLen) {
		Str wt(willTopic);
		addComposedString(_prefix, wt);
		addUint16(willMsg.length());
		addBytes(willMsg);
	}
	if (usernamelen) { // Username - UTF encoded
		addString(username);
	}
	if (passwordlen) { // Password - UTF encoded
		addString(password);
	}
}

void  MqttMsg::Publish(uint8_t hdr, Str& topic, Bytes& msg,
		uint16_t messageId) {
//	LOG("PUBLISH");
	addHeader(MQTT_MSG_PUBLISH + hdr);
	bool addMessageId = (hdr & MQTT_QOS_MASK) ? true : false;
	int remLen = topic.length() + _prefix.length() + 2 + msg.length();
	if (addMessageId)
		remLen += 2;
	addRemainingLength(remLen);
	addComposedString(_prefix, topic);
	/*    addUint16(strlen(_prefix) + strlen(topic));
	 addBytes((uint8_t*) _prefix, strlen(_prefix));
	 addBytes((uint8_t*) topic, strlen(topic));*/
	if (addMessageId)
		addUint16(messageId);
	addBytes(msg);
}

void  MqttMsg::ConnAck(uint8_t erc) {
//   LOG("CONNACK");
	addHeader(MQTT_MSG_CONNACK);
	addRemainingLength(2);
	write((uint8_t) 0);
	write(erc);
}

void  MqttMsg::Disconnect() {
//    LOG("DISCONNECT");
	addHeader(MQTT_MSG_DISCONNECT);
	addRemainingLength(0);
}

void  MqttMsg::PubRel(uint16_t messageId) {
//    LOG("PUBREL");
	addHeader(MQTT_MSG_PUBREL | MQTT_QOS1_FLAG);
	addRemainingLength(2);
	addUint16(messageId);
}

void  MqttMsg::PubAck(uint16_t messageId) {
//    LOG("PUBACK");
	addHeader(MQTT_MSG_PUBACK | MQTT_QOS1_FLAG);
	addRemainingLength(2);
	addUint16(messageId);
}

void  MqttMsg::PubRec(uint16_t messageId) {
//    LOG("PUBREC");
	addHeader(MQTT_MSG_PUBREC | MQTT_QOS1_FLAG);
	addRemainingLength(2);
	addUint16(messageId);
}

void  MqttMsg::PubComp(uint16_t messageId) {
//    LOG("PUBCOMP");
	addHeader(MQTT_MSG_PUBCOMP | MQTT_QOS1_FLAG);
	addRemainingLength(2);
	addUint16(messageId);
}

void  MqttMsg::Subscribe(Str& topic, uint16_t messageId,
		uint8_t requestedQos) {
//	LOG("SUBSCRIBE");
	addHeader(MQTT_QOS1_FLAG | MQTT_MSG_SUBSCRIBE);
	addRemainingLength(topic.length() + 2 + 2 + 1);
	addUint16(messageId);
	addStr(topic);
	write(requestedQos);
}

void  MqttMsg::PingReq() {
//    LOG("PINGREQ");
	addHeader(MQTT_MSG_PINGREQ); // Message Type, DUP flag, QoS level, Retain
	addRemainingLength(0); // Remaining length
}

void  MqttMsg::PingResp() {
// LOG("PINGRESP");
	addHeader(MQTT_MSG_PINGRESP); // Message Type, DUP flag, QoS level, Retain
	addRemainingLength(0); // Remaining length
}

void  MqttMsg::reset() {
	_recvState = ST_HEADER;
	clear();
	_remainingLength = 0;
	_header = 0;
	_recvState = ST_HEADER;
	_returnCode = 0;
	_lengthToRead = 0;
	_offsetVarHeader = 0;
	_messageId = 1;
}

uint8_t  MqttMsg::type() {
	return _header & MQTT_TYPE_MASK;
}

uint8_t  MqttMsg::header() {
	return _header ;
}

uint8_t  MqttMsg::qos() {
	return _header & MQTT_QOS_MASK;
}

uint16_t  MqttMsg::messageId() {
	return _messageId;
}

Str*  MqttMsg::topic() {
	return &_topic;
}

Bytes*  MqttMsg::message() {
	return &_message;
}

bool  MqttMsg::feed(uint8_t data) {
	write(data);
	if (_recvState == ST_HEADER) {
		_header = data;
		_recvState = ST_LENGTH;
		_remainingLength = 0;
	} else if (_recvState == ST_LENGTH) {
		if (calcLength(data) == false) // last byte read for length
				{
			_recvState = ST_PAYLOAD;
			_lengthToRead = _remainingLength;
			if (_remainingLength == 0) {
				_recvState = ST_COMPLETE;
			}
		}
	} else if (_recvState == ST_PAYLOAD) {
		_lengthToRead--;
		if (_lengthToRead == 0) {
			_recvState = ST_COMPLETE;
		}
	} else if (_recvState == ST_COMPLETE) {
		LOGF("invalid state");
	}
// INFO(" state/data/remainingLength %d/%X/%d",_recvState,data,_remainingLength);
	return _recvState == ST_COMPLETE;
}

bool  MqttMsg::complete() {
	return (_recvState == ST_COMPLETE);
}

void  MqttMsg::complete(bool b) {
	if (b)
		_recvState = ST_COMPLETE;
}

void  MqttMsg::readUint16(uint16_t* pi) {
	*pi = read() << 8;
	*pi += read();
}

void  MqttMsg::mapUtfStr(Str& str) {
	uint16_t l;
	readUint16(&l);
	str.map(data() + offset(), l);
	move(l);
}

void  MqttMsg::readBytes(Bytes* b, int length) {
	int i;
	b->clear();
	for (i = 0; i < length; i++) {
		b->write(read());
	}
}

bool  MqttMsg::calcLength(uint8_t data) {
	_remainingLength <<= 7;
	_remainingLength += (data & 0x7F);
	return (data & 0x80);
}

const char*  MqttMsg::toString(Str& str) {
	parse();
	str.clear() << '{' << MqttNames[type() >> 4] << "+"
			<< QosNames[(_header & MQTT_QOS_MASK) >> 1];
	if (_header & 0x1)
		str.append("+RETAIN");
	if (_header & MQTT_DUP_FLAG)
		str.append("+DUP");
	str << ", messageId : " << _messageId;

	if (type() == MQTT_MSG_PUBLISH) {
		str << (const char*) ", topic : " << _topic;
		str << ", message : ";
		_message.toHex(str);
	} else if (type() == MQTT_MSG_SUBSCRIBE) {
		str << ", topic : " << _topic;
	} else if (type() == MQTT_MSG_CONNECT) {
		str << ", clientId : " << _clientId;
		str << ", willTopic : " << _topic;
		str << ", willMessage : " << _message;
		str << ", user : " << _user;
		str << ", password : " << _password;
	}
	str.append(" }");
	return str.c_str();
}

bool  MqttMsg::parse() {
	if (length() < 2) {
		LOGF("too short, length :%d", length());
		return false;
	}
	offset(0);
	_header = read();
	_remainingLength = 0;
	_messageId = 0;
	while (calcLength(read()))
		;
	switch (_header & 0xF0) {
	case MQTT_MSG_CONNECT: {
		Str protocol(0);
		mapUtfStr(protocol);
		read(); // version
		uint8_t connectFlags = read();
//		INFO(" connectFlags :  0x%x", connectFlags);
		uint16_t keepAliveTimer;
		readUint16(&keepAliveTimer);
//		INFO(" keepALiveTimer :  %d", keepAliveTimer);
		mapUtfStr(_clientId);
		if (connectFlags & MQTT_WILL_FLAG) {
			mapUtfStr(_topic);
			mapUtfStr(_message);
		}
		if (connectFlags & MQTT_USERNAME_FLAG)
			mapUtfStr(_user);
		if (connectFlags & MQTT_PASSWORD_FLAG)
			mapUtfStr(_password);

		break;
	}
	case MQTT_MSG_CONNACK: {
		read();
		_returnCode = read();
		break;
	}
	case MQTT_MSG_PUBLISH: {
		mapUtfStr(_topic);
		int rest = _remainingLength - _topic.length() - 2;
		if (_header & MQTT_QOS_MASK) // if QOS > 0 load messageID from payload
		{
			readUint16(&_messageId);
			rest -= 2;
		} else {
			_messageId = 0;
		}
		_message.map(data() + offset(), rest); // map message to rest of payload
		break;
	}
	case MQTT_MSG_SUBSCRIBE: {
		readUint16(&_messageId);
		mapUtfStr(_topic);
		break;
	}
	case MQTT_MSG_SUBACK: {
		readUint16(&_messageId);
		break;
	}
	case MQTT_MSG_PUBACK: {
		readUint16(&_messageId);
		break;
	}
	case MQTT_MSG_PUBREC: {
		readUint16(&_messageId);
		break;
	}
	case MQTT_MSG_PUBREL: {
		readUint16(&_messageId);
		break;
	}
	case MQTT_MSG_PUBCOMP: {
		readUint16(&_messageId);
		break;
	}
	case MQTT_MSG_PINGREQ:
	case MQTT_MSG_PINGRESP: {
		break;
	}
	default: {
		LOGF(" bad Mqtt msg type 0x%x: erc : %d", _header & 0xF0, EINVAL);
		break; // ignore bad package
	}
	}
	return true;
}

// put Active Objects global
// check malloc used after init ?
// stack or global ?
// test MqttPub
// all Stellaris dependent in one file
// publish CPU,FLASH,RAM
// Usb recv can be Bytes instead of MqttMsg / drop parse
// try Usb::disconnect() => UsbCDCTerm()
//

