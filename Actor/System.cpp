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
#include <Spi.h>

#include <Arduino.h>
#include <gpio_c.h>
// D8 == GPIO PIN 15
void cs_select() {
//	digitalWrite(D8, 0);
}

void cs_deselect() {
//	digitalWrite(D8, 1);
}

void testMode(int);

#include <spi_register.h>
#define HSPI 1
/* Register 0x14  SPI_CTRL2

 spi_cs_delay_num  [31:28]   4'h0  R/W
 spi_cs signal is delayed by 80MHz clock cycles

 spi_cs_delay_mode [27:26]   2'h0  R/W
 spi_cs signal is delayed by spi_clk. 0: zero; 1: half cycle; 2: one cycle

 spi_mosi_delay_num  [25:23] 3'h0  R/W
 MOSI signals are delayed by 80MHz clock cycles

 spi_mosi_delay_mode [22:21] 2'h0  R/W
 MOSI signals are delayed by spi_clk. 0: zero; 1: half cycle; 2: one cycle

 spi_miso_delay_num  [20:18] 3'h0  R/W
 MISO signals are delayed by 80MHz clock cycles

 spi_miso_delay_mode [17:16] 2'h0  R/W
 MISO signals are delayed by spi_clk. 0: zero; 1: half cycle; 2: one cycle

 SPI_CK_OUT_HIGH_MODE [15:12] 4'h0 R/W
 CPOL = 1 => 0xF

 SPI_CK_OUT_LOW_MODE [11:8] 4'h0 R/W
 CPOL =0 => 0xF

 SPI_HOLD_TIME [7:4]
 SPI_CS_HOLD ( in SPI_USER ) : As above, except it holds the CS line low for a few cpu cycles after the SPI clock stops.
 Again, good idea to enable this by default unless your SPI slave devices has specific requirements
 about when the CS line goes high.

 SPI_SETUP_TIME [3:0]

 SPI_CS_SETUP: Enabling this ensures that your chip select (CS) line is pulled low a couple of cpu cycles
 before the SPI clock starts, giving your SPI slave device some time to get ready if required.
 I don’t see any harm in having this on by default.

 SPI_CS_HOLD: As above, except it holds the CS line low for a few cpu cycles after the SPI clock stops.
 Again, good idea to enable this by default unless your SPI slave devices has specific requirements
 about when the CS line goes high.

 if (mSPIMode & 0x2) {// CPOL
 SET_PERI_REG_MASK(SPI_CTRL2, SPI_CK_OUT_HIGH_MODE << SPI_CK_OUT_HIGH_MODE_S);
 CLEAR_PERI_REG_MASK(SPI_CTRL2, SPI_CK_OUT_LOW_MODE << SPI_CK_OUT_LOW_MODE_S);
 } else {
 SET_PERI_REG_MASK(SPI_CTRL2, SPI_CK_OUT_LOW_MODE << SPI_CK_OUT_LOW_MODE_S);
 CLEAR_PERI_REG_MASK(SPI_CTRL2, SPI_CK_OUT_HIGH_MODE << SPI_CK_OUT_LOW_MODE_S);
 }

 [COMMAND]+[ADDRESS]+[DataOUT]+[DUMMYBITS]+[DataIN]

 */




union {
	uint32_t forAlignment;
	uint8_t data[20];
} input, output;
uint8_t writeEuid[] = { 0x81, 0x01, 0x03, 0x05, 0x07, 0x11, 0x13, 0x17, 0x23 };
uint8_t readEuid[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void testSPI() {
	Spi spi(HSPI);
//	pinMode(D8, OUTPUT);
	spi.dwmInit();

	memcpy(output.data, writeEuid, sizeof(writeEuid));

	spi.txf(output.data,sizeof(writeEuid),input.data,0);
	Serial.print(" write ");
	for (int i = 0; i < sizeof(writeEuid); i++)
		Serial.printf("%X,", input.data[i]);
	Serial.println();

	memcpy(output.data, readEuid, sizeof(readEuid));
	spi.txf(output.data,1,input.data,sizeof(readEuid)-1);

	Serial.printf("  read ");
	for (int i = 0; i < sizeof(readEuid); i++)
		Serial.printf("%X,", output.data[i]);
	Serial.println();


}

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
		testSPI();
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
