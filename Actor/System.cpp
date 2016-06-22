/*
 * Dwm1000.cpp
 *
 *  Created on: 30-mei-2016
 *      Author: 600104947
 */
#include <Arduino.h>
#include <System.h>
#include <Json.h>
#include <Mqtt.h>
extern "C" {
#include "user_interface.h"
}

static Cbor out(100);

System::System(ActorRef mqtt) :
		Actor("System") {
	_mqtt = mqtt;
	_mqttConnected = false;
}

System::~System() {

}
/*#include <SPI.h>

union {
	uint32_t forAlignment;
	uint8_t data[20];
} input, output;
uint8_t writeEuid[] = { 0x81, 0x01, 0x03, 0x05, 0x07, 0x11, 0x13, 0x17, 0x23 };
uint8_t readEuid[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#include <Arduino.h>
#include <gpio_c.h>
// D8 == GPIO PIN 15
ICACHE_RAM_ATTR void cs_select() {
//	digitalWrite(D8, 0);
}

ICACHE_RAM_ATTR void cs_deselect() {
//	digitalWrite(D8, 1);
}

void testMode(int);

#include <spi_register.h>
#define HSPI 1

void config(){

		WRITE_PERI_REG(SPI_CTRL2(HSPI), 0xFFFFFFFF);

		WRITE_PERI_REG(SPI_CTRL2(HSPI),
				(( 0xF & SPI_CS_DELAY_NUM ) << SPI_CS_DELAY_NUM_S) | //
				(( 0x1 & SPI_CS_DELAY_MODE) << SPI_CS_DELAY_MODE_S) |//
				(( 0xF & SPI_SETUP_TIME )<< SPI_SETUP_TIME_S ) |//
				(( 0xF & SPI_HOLD_TIME )<< SPI_HOLD_TIME_S ) |//
				(( 0xF & SPI_CK_OUT_LOW_MODE )<< SPI_CK_OUT_LOW_MODE_S ) |//
				(( 0xF & SPI_CK_OUT_HIGH_MODE )<< SPI_CK_OUT_HIGH_MODE_S ) |//
				(( 0x7 & SPI_MOSI_DELAY_NUM ) << SPI_MOSI_DELAY_NUM_S) |//
				(( 0x7 & SPI_MISO_DELAY_NUM ) << SPI_MISO_DELAY_NUM_S) |//
				(( 0x1 & SPI_MOSI_DELAY_MODE )<< SPI_MOSI_DELAY_MODE_S ) |//
				(( 0x1 & SPI_MISO_DELAY_MODE )<< SPI_MISO_DELAY_MODE_S ) |//
				0);
	}


void testSPI() {
	int freqs[] = { 100000, 1000000, 4000000, 10000000 };
	config();
	for (int i = 0; i < 4; i++) {
		testMode(freqs[i]);
	}
}

void testMode(int freq) {
	SPIClass spi;
	pinMode(D8, OUTPUT);
	spi.begin();
	spi.setHwCs(true);
	spi.setFrequency(freq);
	spi.setDataMode(SPI_MODE0);
	spi.setBitOrder(MSBFIRST);

	memcpy(input.data, writeEuid, sizeof(writeEuid));
	cs_select();
	spi.transferBytes(input.data, output.data, sizeof(writeEuid));
	cs_deselect();

	Serial.print(" write ");
	for (int i = 0; i < sizeof(writeEuid); i++)
		Serial.printf("%X,", output.data[i]);
	Serial.println();

	memcpy(input.data, readEuid, sizeof(readEuid));
	cs_select();
	spi.transferBytes(input.data, output.data, sizeof(readEuid));
	cs_deselect();

	Serial.print(" read ");
	for (int i = 0; i < sizeof(readEuid); i++)
		Serial.printf("%X,", output.data[i]);
	Serial.println();

	spi.end();
}
*/
//___________________________________________________
//
bool System::subscribed(Header hdr) {
	return Actor::subscribed(hdr) || hdr.is(_mqtt, REPLY(CONNECT))
			|| hdr.is(_mqtt, REPLY(DISCONNECT));
}

void System::init() {

}

void System::publish(uint8_t qos, const char* key, Str& value) {
	if (_mqttConnected)
		_mqtt.tell(self(), PUBLISH, qos, _cborOut.putf("sB", key, &value));
}

void System::onReceive(Header hdr, Cbor& data) {
	Json json(20);
	if (hdr.is(_mqtt, REPLY(CONNECT))) {
		_mqttConnected = true;
		return;
	} else if (hdr.is(_mqtt, REPLY(DISCONNECT))) {
		_mqttConnected = false;
		return;
	} else if (hdr.is(INIT)) {
		setReceiveTimeout(10000);
		init();
		return;
	};
	switch (hdr._event) {
	case TIMEOUT: {
		setReceiveTimeout(2000);
		json.clear();
		json.add(true);
		publish(0, "system/online", json);
		json.clear();
		json.add((uint64_t) millis());
		publish(0, "system/up_time", json);
		json.clear();
		json.add((uint64_t) system_get_free_heap_size());
		publish(0, "system/heap_size", json);
		break;
	}
	default: {
		unhandled(hdr);
		break;
	}
	}
}
