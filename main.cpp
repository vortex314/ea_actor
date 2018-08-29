#include <Arduino.h>
//#include <ESP8266WiFi.h>
#include <Actor.h>
#include <Wifi.h>
#include <Tcp.h>
#include <Mqtt.h>
#include <MqttFramer.h>
#include <LedBlinker.h>
#include <Config.h>
#include <pins_arduino.h>

#include <System.h>
//#include <PubSubClient.h>

ActorRef ledBlinker;
ActorRef tcpServer;
ActorRef wifi;
ActorRef mqtt;
ActorRef mqttFramer;
ActorRef dwm1000;
ActorRef config;



extern "C" void setup(void) {
	Serial.begin(115200);
//	Serial.setDebugOutput(true);
	Serial.println("\n\n Started ....");

	Actor::setup();

	ledBlinker = LedBlinker::create(16);
	wifi = Wifi::create(SSID, PSWD);
	tcpServer = TcpServer::create(wifi,23);
	mqttFramer = MqttFramer::create();
	config = Config::create();


	mqtt = Mqtt::create(mqttFramer,"stm32_1/");
	System::create(mqtt);

	Actor::broadcast(ActorRef(0), INIT, 0);

}

extern "C" void loop(void) {
	Actor::eventLoop();
}
