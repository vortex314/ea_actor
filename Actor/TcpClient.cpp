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
	delete _client;
}

ActorRef TcpClient::create(const char* host, uint16_t port) {
	return ActorRef(new TcpClient(host, port));
}

void TcpClient::init() {

}

void TcpClient::onReceive(Header header, Cbor& data) {
	if (header.event == INIT) {
		init();
	} else if (header.event == CONNECTED) {
		_client->connect("test.mosquitto.org", 1883);
		right().tell(self(), CONNECTED, 0);
//		      client.println("GET /search?q=arduino HTTP/1.0");
//		      client.println();
	}
}

