/*
 * TcpClient.cpp
 *
 *  Created on: May 16, 2016
 *      Author: lieven
 */

#include "TcpClient.h"



TcpClient::TcpClient(const char* host, uint16_t port) :
		Actor("TcpClient") {
	_client = new WiFiClient();
	_host = host;
	_port = port;
}

TcpClient::~TcpClient() {
}

void TcpClient::init() {

	_client->connect("test.mosquitto.org", 1883);
}

void TcpClient::onReceive(Header header,Cbor& data) {
	if (header.event == INIT) {
		init();
	}
}

ActorRef TcpClient::create(const char* host, uint16_t port) {
	return ActorRef(new TcpClient(host, port));
}
