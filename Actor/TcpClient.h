/*
 * TcpClient.h
 *
 *  Created on: May 16, 2016
 *      Author: lieven
 */

#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include <Actor.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>



class TcpClient: public Actor {
	WiFiClient* _client;
	const char* _host;
	uint16_t _port;
public:
	TcpClient(const char* host,uint16_t port) ;
	virtual ~TcpClient();
	void init();
//	void onReceive(ActorRef src, Event event, uint8_t detail, Cbor& data) ;
	void loop();
	void onReceive(Header header, Cbor& data);
	static ActorRef create(const char* host,uint16_t port);
};

#endif /* TCPCLIENT_H_ */
