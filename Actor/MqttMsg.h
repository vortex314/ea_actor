/*
 * MqttMsg.h
 *
 *  Created on: Oct 25, 2015
 *      Author: lieven
 */

#ifndef MQTTMSG_H_
#define MQTTMSG_H_
#include "MqttConstants.h"

enum MqttField {
	KEY_HEADER, KEY_TOPIC, KEY_MESSAGE, KEY_CLIENT, KEY_USER, KEY_PASSWORD,
};

class MqttMsg: public Bytes {
private:
	Str _prefix;
	Str _message;
	Str _topic;
	Str _user;
	Str _password;
	Str _clientId;
	uint8_t _header;
	uint32_t _remainingLength;
	uint32_t _lengthToRead;
	uint32_t _offsetVarHeader;
	uint8_t _returnCode;
	uint16_t _messageId;

	enum RecvState {
		ST_HEADER, ST_LENGTH, ST_PAYLOAD, ST_COMPLETE
	} _recvState;
private:
	void  addHeader(int header);
	void  addRemainingLength(uint32_t length);
	void  addUint16(uint16_t value);
	void  addString(const char *string);
	void  addStr(Str& str);
	void   addComposedString(Str& prefix, Str& str);
	void  addMessage(uint8_t* src, uint32_t length);
	void  addBytes(Bytes& bytes);
	void  addBytes(uint8_t* bytes, uint32_t length);
public:
	 MqttMsg(uint32_t size);
	void  Connect(uint8_t hdr, const char *clientId, uint8_t connectFlag,
			const char* willTopic, Bytes& willMsg, const char *username,
			const char* password, uint16_t keepAlive);
	void  ConnAck(uint8_t erc);
	void  Disconnect();
	void  PingReq();
	void  PingResp();
	void  Publish(uint8_t hdr, Str& topic, Bytes& msg, uint16_t message_id);
	void  PubRel(uint16_t messageId);
	void  PubAck(uint16_t messageId);
	void  PubRec(uint16_t messageId);
	void  PubComp(uint16_t messageId);
	void  Subscribe(Str& topic, uint16_t messageId, uint8_t requestedQos);
	void  prefix(Str& pr);
public:

	bool  feed(uint8_t b);

	uint16_t  messageId(); // if < 0 then not found
	uint8_t  type();
	uint8_t  qos();
	bool  complete();
	void  complete(bool st);
	void  reset();
//	void add(uint8_t data);
	bool  calcLength(uint8_t data);
	bool  parse();
	void  readUint16(uint16_t * pi);
	void  mapUtfStr(Str& str);
	void  readBytes(Bytes* b, int length);
	Str*  topic();
	Bytes*  message();
	const char *  toString(Str& str);

};

#endif /* MQTTMSG_H_ */
