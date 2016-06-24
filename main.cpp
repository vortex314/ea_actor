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
#include <DWM1000Anchor.h>
#include <DWM1000Tag.h>
#include <System.h>
//#include <PubSubClient.h>

ActorRef ledBlinker;
ActorRef tcpClient;
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
	wifi = Wifi::create("Merckx", "LievenMarletteEwoutRonald");
	tcpClient = TcpClient::create("iot.eclipse.org", 1883);
	mqttFramer = MqttFramer::create();
	config = Config::create();

#ifdef TAG
#define MQTT_NAME "tag_1"
#else
#define MQTT_NAME "anchor_1"
#endif
	mqtt = Mqtt::create(mqttFramer,MQTT_NAME);
#ifdef TAG
	dwm1000 = DWM1000_Tag::create(mqtt);
#endif
#ifdef ANCHOR1
	dwm1000 = DWM1000_Anchor::create(mqtt);
#endif
	System::create(mqtt);

	wifi >> tcpClient >> mqttFramer >> mqtt >> ledBlinker;



	Actor::broadcast(ActorRef(0), INIT, 0);

}

extern "C" void loop(void) {
	Actor::eventLoop();
}
