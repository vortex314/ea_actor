/*
 * File:   Mqtt.h
 * Author: lieven2
 *
 * Created on 13 september 2013, 20:24
 */

#ifndef MQTTCONSTANTS_H
#define	MQTTCONSTANTS_H
#include "Str.h"


#define MQTT_RETRY_COUNT				3
#define MQTT_PUBLISH_TIMEOUT			1000
#define MQTT_KEEP_ALIVE			5000
// MQTT MESSAGE TYPES
#define MQTT_TYPE_MASK			0xF0
#define MQTT_MSG_CONNECT       	(1<<4)
#define MQTT_MSG_CONNACK       	(2<<4)
#define MQTT_MSG_PUBLISH      	(3<<4)
#define MQTT_MSG_PUBACK        	(4<<4)
#define MQTT_MSG_PUBREC			(5<<4)
#define MQTT_MSG_PUBREL        	(6<<4)
#define MQTT_MSG_PUBCOMP       	(7<<4)
#define MQTT_MSG_SUBSCRIBE    	(8<<4)
#define MQTT_MSG_SUBACK       	(9<<4)
#define MQTT_MSG_UNSUBSCRIBE  	(10<<4)
#define MQTT_MSG_UNSUBACK     	(11<<4)
#define MQTT_MSG_PINGREQ      	(12<<4)
#define MQTT_MSG_PINGRESP   	(13<<4)
#define MQTT_MSG_DISCONNECT   	(14<<4)

#define MQTT_DUP_FLAG     		(1<<3)
// MQTT QOS values
#define MQTT_QOS_MASK			(3<<1)
#define MQTT_QOS0_FLAG    		(0<<1)
#define MQTT_QOS1_FLAG    		(1<<1)
#define MQTT_QOS2_FLAG    		(1<<2)

#define MQTT_RETAIN_FLAG  		1
#define MQTT_RETAIN_MASK  		1

// CONNECT FLAGS
#define MQTT_CLEAN_SESSION  	(1<<1)
#define MQTT_WILL_FLAG     		(1<<2)
#define MQTT_WILL_RETAIN    	(1<<5)
#define MQTT_WILL_QOS0			(0<<3)
#define MQTT_WILL_QOS1			(1<<3)
#define MQTT_WILL_QOS2			(2<<3)
#define MQTT_USERNAME_FLAG  	(1<<7)
#define MQTT_PASSWORD_FLAG  	(1<<6)
// CONNACk RETURN CODES
#define MQTT_RTC_CONN_ACC  0x00 // Connection Accepted
#define MQTT_RTC_BAD_VERSION 0x01  // Connection Refused: unacceptable protocol version
#define MQTT_RTC_BAD_ID 0x02 // Connection Refused: identifier rejected
#define MQTT_RTC_SRV_UNAVAIL 0x03 // Connection Refused: server unavailable
#define MQTT_RTC_BAD_AUTH 0x04 // Connection Refused: bad user name or password
#define MQTT_RTC_ACC_DENIED 0x05 // Connection Refused: not authorized

#define TOPIC_MAX_LENGTH 100
#define MESSAGE_MAX_LENGTH 256
#define MQTT_MAX_SIZE   256


#endif	/* MQTTCONSTANTS_H */

