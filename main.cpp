#include <Arduino.h>
//#include <ESP8266WiFi.h>
#include <Actor.h>
#include <Wifi.h>
#include <Tcp.h>
#include <Mqtt.h>
#include <LedBlinker.h>
#include <pins_arduino.h>
//#include <PubSubClient.h>

ActorRef ledBlinker;
ActorRef tcpClient;
ActorRef wifi;
ActorRef mqtt;

extern "C" void setup(void) {
	Serial.begin(115200);
//	Serial.setDebugOutput(true);
	Serial.println("Started ....");

	Actor::setup();

	ledBlinker = LedBlinker::create(16);
	wifi = Wifi::create("Merckx", "LievenMarletteEwoutRonald");
	tcpClient = TcpClient::create("test.mosquitto.org",1883);
	mqtt = Mqtt::create("test.mosquitto.org",1883);
//	wifi >> ledBlinker;

	wifi >> tcpClient >> mqtt >> ledBlinker;

	Actor::broadcast(Actor::dummy(), INIT, 0);

}

extern "C" void loop(void) {
	Actor::eventLoop();
}
