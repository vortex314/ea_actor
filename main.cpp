#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Actor.h>
#include <Wifi.h>
#include <TcpClient.h>
#include <pins_arduino.h>
//#include <PubSubClient.h>

const int led = 16;


class LedBlinker: public Actor {
	uint8_t _pin;
	bool _isOn;
	uint32_t _interval;
public:

	LedBlinker(uint8_t pin) :
			Actor("Led") {
		_isOn=true;
		_pin=pin;
		_interval = 100;
	}
	static ActorRef create(int pin) {
		return ActorRef(new LedBlinker(pin));
	}
	void init() {
		pinMode(_pin, OUTPUT);
		digitalWrite(_pin, 1);
	}
	void blink() {
		if (_isOn) {
			_isOn = false;
			digitalWrite(_pin, 1);
		} else {
			_isOn = true;
			digitalWrite(_pin, 0);
		}
	}
	void onReceive(Header hdr, Cbor& data) {
		if (hdr.event == INIT) {
			init();
			setReceiveTimeout(_interval);
		} else if ( hdr.event == TIMEOUT ){
			blink();
			setReceiveTimeout(_interval);
		} else if ( hdr.event == CONNECTED ) {
			_interval = 500;
		} else if ( hdr.event == DISCONNECTED ) {
			_interval = 100;
		} else {
			unhandled(hdr);
		}
	}
};

ActorRef ledBlinker;
ActorRef tcpClient;
ActorRef wifi;


extern "C" void setup(void) {
	Serial.begin(115200);
//	Serial.setDebugOutput(true);
	Serial.println("Started ....");

	Actor::setup();

	ledBlinker = LedBlinker::create(16);
	wifi = Wifi::create("Merckx","LievenMarletteEwoutRonald");
	tcpClient = TcpClient::create("test.mosquitto.org",1883);
	wifi >> tcpClient >> ledBlinker;

	Actor::broadcast(Actor::dummy(),INIT,0);

}

extern "C" void loop(void) {
	Actor::eventLoop();
}
