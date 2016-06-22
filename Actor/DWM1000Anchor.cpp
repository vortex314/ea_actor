/*
 * DWM1000_Anchor.cpp
 *
 *  Created on: Feb 12, 2016
 *      Author: lieven
 */

#include <DWM1000Anchor.h>
#include <Json.h>

extern "C" {
#include <Spi.h>
#include <gpio_c.h>
#include <espmissingincludes.h>
#include <osapi.h>
#include "deca_device_api.h"
#include "deca_regs.h"
#include "deca_sleep.h"
}
;
/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 1000

/* Default communication configuration. We use here EVK1000's default mode (mode 3). */
static dwt_config_t config = { //
		2, // Channel number.
				DWT_PRF_64M, // Pulse repetition frequency.
				DWT_PLEN_1024, /* Preamble length. */
				DWT_PAC32, /* Preamble acquisition chunk size. Used in RX only. */
				9, /* TX preamble code. Used in TX only. */
				9, /* RX preamble code. Used in RX only. */
				1, /* Use non-standard SFD (Boolean) */
				DWT_BR_110K, /* Data rate. */
				DWT_PHRMODE_STD, /* PHY header mode. */
				(1025 + 64 - 32) /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
		};

/* Default antenna delay values for 64 MHz PRF. See NOTE 1 below. */
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436

///

/* Frames used in the ranging process. See NOTE 2 below. */

static uint8 rx_poll_msg[] = { //
		0x41, 0x88, // byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
				0, // byte 2: sequence number, incremented for each new frame.
				0xCA, 0xDE, // byte 3/4 : PAN ID (0xDECA)
				'W', 'A', // byte 5/6: destination address, see NOTE 3 below.
				'V', 'E', // byte 7/8: source address, see NOTE 3 below.
				0x21, // byte 9: function code (specific values to indicate which message it is in the ranging process).
				0, 0 }; // All messages end with a 2-byte checksum automatically set by DW1000.

static uint8 tx_resp_msg[] = { //
		0x41, 0x88, // byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
				0, // byte 2: sequence number, incremented for each new frame.
				0xCA, 0xDE, // byte 3/4 : PAN ID (0xDECA)
				'V', 'E', // byte 5/6: destination address, see NOTE 3 below.
				'W', 'A', // byte 7/8: source address, see NOTE 3 below.
				0x10, // byte 9: function code (specific values to indicate which message it is in the ranging process). [Response message]
				0x02, // byte 10: activity code (0x02 to tell the initiator to go on with the ranging exchange).
				0, 0, // byte 11/12: activity parameter, not used for activity code 0x02.
				0, 0 }; // All messages end with a 2-byte checksum automatically set by DW1000.

static uint8 rx_final_msg[] = { 0x41, 0x88, // byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
		0, // byte 2: sequence number, incremented for each new frame.
		0xCA, 0xDE, // byte 3/4 : PAN ID (0xDECA)
		'W', 'A', // byte 5/6: destination address, see NOTE 3 below.
		'V', 'E', // byte 7/8: source address, see NOTE 3 below.
		0x23, // byte 9: function code (specific values to indicate which message it is in the ranging process). [Final message]
		0, 0, 0, 0, // byte 10 -> 13: poll message transmission timestamp
		0, 0, 0, 0, // byte 14 -> 17: response message reception timestamp
		0, 0, 0, 0, // byte 18 -> 21: final message transmission timestamp.
		0, 0 }; // All messages end with a 2-byte checksum automatically set by DW1000.

/* Length of the common part of the message (up to and including the function code, see NOTE 2 below). */
#define ALL_MSG_COMMON_LEN 10
/* Indexes to access some of the fields in the frames defined above. */
#define ALL_MSG_SN_IDX 2
#define FINAL_MSG_POLL_TX_TS_IDX 10
#define FINAL_MSG_RESP_RX_TS_IDX 14
#define FINAL_MSG_FINAL_TX_TS_IDX 18
#define FINAL_MSG_TS_LEN 4
/* Frame sequence number, incremented after each transmission. */
static uint8 frame_seq_nb = 0;

/* Buffer to store received response message.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
#define RX_BUF_LEN 24
static uint8 rx_buffer[RX_BUF_LEN];

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 µs and 1 µs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Delay between frames, in UWB microseconds. See NOTE 4 below. */
/* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 150
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.66 ms with above configuration. */
#define RESP_RX_TO_FINAL_TX_DLY_UUS 3100
/* Receive response timeout. See NOTE 5 below. */
#define RESP_RX_TIMEOUT_UUS 2700

/* Time-stamps of frames transmission/reception, expressed in device time units.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef unsigned long long uint64;

/* Delay between frames, in UWB microseconds. See NOTE 4 below. */
/* This is the delay from Frame RX timestamp to TX reply timestamp used for calculating/setting the DW1000's delayed TX function. This includes the
 * frame length of approximately 2.46 ms with above configuration. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 2600
#define RESP_TX_TO_FINAL_RX_DLY_UUS 500 /* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
#define FINAL_RX_TIMEOUT_UUS 3300	/* Receive final timeout. See NOTE 5 below. */

/* Timestamps of frames transmission/reception.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef signed long long int64;

static uint64 poll_rx_ts;
static uint64 resp_tx_ts;
static uint64 final_rx_ts;

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

/* Hold copies of computed time of flight and distance here for reference, so reader can examine it at a breakpoint. */
static double tof;
static double distance;

/* Declaration of static functions. */
static uint64 get_tx_timestamp_u64(void);
static uint64 get_rx_timestamp_u64(void);
static void final_msg_get_ts(const uint8 *ts_field, uint32 *ts);

static DWM1000_Anchor* gAnchor = 0;

DWM1000_Anchor::DWM1000_Anchor(ActorRef mqtt) :
		Actor("DWM1000_Anchor") {
	_count = 0;
	_mqtt = mqtt;
	_mqttConnected = false;
	gAnchor = this;
	_pollsReceived = 0;
	_pollsForAnchor = 0;
	_replySend = 0;
	_finalReceived = 0;
	_interrupts = 0;
	state(S_START);
}

ActorRef DWM1000_Anchor::create(ActorRef mqtt) {
	return ActorRef(new DWM1000_Anchor(mqtt));
}

DWM1000_Anchor::~DWM1000_Anchor() {
}
//_________________________________________________ HARd RESEST DWM1000_Anchor via PIN
//
void DWM1000_Anchor::resetChip() {
	LOGF( " Reset DWM1000_Anchor ");
	int pin = D1; // RESET PIN == D1 == GPIO5
	pinMode(pin, 1); // OUTPUT
	delay(10);
	digitalWrite(pin, 0); // PULL LOW
	delay(10); // 10ms
	digitalWrite(pin, 1); // PUT HIGH
}
//_________________________________________________ IRQ handler
//

ICACHE_RAM_ATTR void DWM1000_Anchor::my_dwt_isr() {
	if (gAnchor) {
		gAnchor->_interrupted = true;
		gAnchor->_interrupts++;
	}
}

bool DWM1000_Anchor::isInterruptDetected() {
	return _interrupted;
}

void DWM1000_Anchor::clearInterrupt() {
	_interrupted = false;
}

//_________________________________________________ Configure IRQ pin
//
void DWM1000_Anchor::enableIsr() {
	LOGF( " IRQ SET ");
	int pin = D2; // IRQ PIN = D2 = GPIO4
	pinMode(pin, 0); // INPUT
	attachInterrupt(pin, my_dwt_isr, CHANGE);
}
//_________________________________________________ INITIALIZE SPI
//
void DWM1000_Anchor::initSpi() {
	LOGF( "Init SPI ");
	Spi spi(HSPI);
	spi.init();
	spi.mode(0, 0);
//	spi.clock(HSPI, SPI_CLK_PREDIV, SPI_CLK_CNTDIV);
	spi.clock(10, 20); //
//	spi.tx_byte_order( SPI_BYTE_ORDER_HIGH_TO_LOW);
//	spi.rx_byte_order( SPI_BYTE_ORDER_HIGH_TO_LOW);
	spi.tx_byte_order(SPI_BYTE_ORDER_LOW_TO_HIGH);
	spi.rx_byte_order(SPI_BYTE_ORDER_LOW_TO_HIGH);
	spi.set_bit_order(0);
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

void DWM1000_Anchor::txcallback(const dwt_callback_data_t * data) {
}

void DWM1000_Anchor::init() {
	LOGF("HSPI");
//_________________________________________________INIT SPI ESP8266




	resetChip();
	LOGF(" IRQ pin : %d ", digitalRead(D2));
	initSpi();
	enableIsr();

	while (true) {
		uint64_t eui = 0xF1F2F3F4F5F6F7F;
		dwt_seteui((uint8_t*) &eui);
		dwt_geteui((uint8_t*) &eui);
		LOGF( "EUID : %ld", eui);
		ASSERT_LOG( eui == 0xF1F2F3F4F5F6F7F);
		dwt_seteui((uint8_t*) &eui);
		dwt_geteui((uint8_t*) &eui);
		LOGF( "EUID : %lX", eui);
		delay(2000);
		if (eui == 0xF1F2F3F4F5F6F7F)
			break;
	}

//	dwt_softreset();
	deca_sleep(100);

	if (dwt_initialise(DWT_LOADUCODE)) {
		LOGF( " dwt_initialise failed ");
	} else
		LOGF( " dwt_initialise done.");
	if (dwt_configure(&config)) {
		LOGF( " dwt_configure failed ");
	} else
		LOGF( " dwt_configure done.");

	uint32_t device_id = dwt_readdevid();
	uint32_t part_id = dwt_getpartid();
	uint32_t lot_id = dwt_getlotid();

	LOGF( " device id : %X, part id :%X, lot_id :%X",
			device_id, part_id, lot_id);

	/* Apply default antenna delay value. See NOTE 1 below. */
	dwt_setrxantennadelay(RX_ANT_DLY);
	dwt_settxantennadelay(TX_ANT_DLY);


//	dwt_initialise(DWT_LOADUCODE);
// Configure the callbacks from the dwt library
	dwt_setinterrupt(DWT_INT_RFCG, 1); // enable interr
	dwt_setcallbacks(txcallback, rxcallback); // set interr callbacks

	_count = 0;
}

bool DWM1000_Anchor::subscribed(Header hdr) {
	return Actor::subscribed(hdr) || hdr.is(_mqtt, REPLY(CONNECT))
			|| hdr.is(_mqtt, REPLY(DISCONNECT));
}

//____________________________________________________ PUBLISH TO MQTT
//
void DWM1000_Anchor::publish() {
	static uint32_t count = 0;
	if (count++ % 100)
		return; // publish only each 100 calls
	if (_mqttConnected) {
		Json json(20);
		Bytes* js = &json;
		json.clear();
		json.add(_pollsReceived);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/pollsReceived", js));
		json.clear();
		json.add(_pollsForAnchor);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/pollsForAnchor", js));
		json.clear();
		json.add(_replySend);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/replySend", js));
		json.clear();
		json.add(_finalReceived);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/finalReceived", js));
		json.clear();
		json.add(_interrupts);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/interrupts", js));
		json.clear();
		json.add((float) distance);
		_mqtt.tell(self(), PUBLISH, 0,
				_cborOut.putf("sB", "dwm1000/distance", js));
	}
}

void DWM1000_Anchor::calcDistance() {
	_finalReceived++;
	uint32 poll_tx_ts, resp_rx_ts, final_tx_ts;
	uint32 poll_rx_ts_32, resp_tx_ts_32, final_rx_ts_32;
	double Ra, Rb, Da, Db;
	int64 tof_dtu;

	/* Retrieve response transmission and final reception timestamps. */
	resp_tx_ts = get_tx_timestamp_u64();
	final_rx_ts = get_rx_timestamp_u64();

	/* Get timestamps embedded in the final message. */
	final_msg_get_ts(&rx_buffer[FINAL_MSG_POLL_TX_TS_IDX], &poll_tx_ts);
	final_msg_get_ts(&rx_buffer[FINAL_MSG_RESP_RX_TS_IDX], &resp_rx_ts);
	final_msg_get_ts(&rx_buffer[FINAL_MSG_FINAL_TX_TS_IDX], &final_tx_ts);

	/* Compute time of flight. 32-bit subtractions give correct answers even if clock has wrapped. See NOTE 10 below. */
	poll_rx_ts_32 = (uint32) poll_rx_ts;
	resp_tx_ts_32 = (uint32) resp_tx_ts;
	final_rx_ts_32 = (uint32) final_rx_ts;
	Ra = (double) (resp_rx_ts - poll_tx_ts);
	Rb = (double) (final_rx_ts_32 - resp_tx_ts_32);
	Da = (double) (final_tx_ts - resp_rx_ts);
	Db = (double) (resp_tx_ts_32 - poll_rx_ts_32);
	tof_dtu = (int64) ((Ra * Rb - Da * Db) / (Ra + Rb + Da + Db));

	tof = tof_dtu * DWT_TIME_UNITS;
	distance = tof * SPEED_OF_LIGHT;

}
//______________________________________________________________________
//
bool DWM1000_Anchor::receivePollForAnchor() {
	uint32 frame_len;
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);
	/* Clear good RX frame event in the DW1000 status register. */
	frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023; /* A frame has been received, read it into the local buffer. */
	if (frame_len <= RX_BUFFER_LEN) {
		dwt_readrxdata(rx_buffer, frame_len, 0);
	}
//			LOGF(" frame_len : %d status_reg : %X",frame_len,_status_reg);

	/* Check that the frame is a poll sent by "DS TWR initiator" example.
	 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
	rx_buffer[ALL_MSG_SN_IDX] = 0;
	if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
		return true;
	}
	return false;
}
//______________________________________________________________________
//
bool DWM1000_Anchor::receiveFinalForAnchor() {
	uint32_t frame_len;
	/* Clear good RX frame event and TX frame sent in the DW1000 status register. */
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG | SYS_STATUS_TXFRS);

	/* A frame has been received, read it into the local buffer. */
	frame_len = dwt_read32bitreg(
			RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
	if (frame_len <= RX_BUF_LEN) {
		dwt_readrxdata(rx_buffer, frame_len, 0);
	}

	/* Check that the frame is a final message sent by "DS TWR initiator" example.
	 * As the sequence number field of the frame is not used in this example, it can be zeroed to ease the validation of the frame. */
	rx_buffer[ALL_MSG_SN_IDX] = 0;
	if (memcmp(rx_buffer, rx_final_msg, ALL_MSG_COMMON_LEN)) {
		_pollsForAnchor++;
		return true;
	}

	return false;
}
//______________________________________________________________________
//
void DWM1000_Anchor::sendReply() {

	uint32 resp_tx_time;

	poll_rx_ts = get_rx_timestamp_u64(); /* Retrieve poll reception timestamp. */

	/* Set send time for response. See NOTE 8 below. */
	resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME))
			>> 8;
	dwt_setdelayedtrxtime(resp_tx_time);

	/* Set expected delay and timeout for final message reception. */
	dwt_setrxaftertxdelay(RESP_TX_TO_FINAL_RX_DLY_UUS);
	dwt_setrxtimeout(FINAL_RX_TIMEOUT_UUS);

	/* Write and send the response message. See NOTE 9 below.*/
	tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
	dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
	dwt_writetxfctrl(sizeof(tx_resp_msg), 0);
	dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
	dwt_setinterrupt(DWT_INT_RFCG, 1); // enable

}
//______________________________________________________________________
//
void DWM1000_Anchor::rxcallback(const dwt_callback_data_t * data) {
	if (gAnchor) {
		DWM1000_Anchor& anchor = *gAnchor;
		anchor._interrupts++;
		switch (anchor.state()) {
		case S_POLL_WAIT: {
			anchor._pollsReceived++;
			if (anchor.receivePollForAnchor()) {
				anchor._pollsForAnchor++;
				anchor.sendReply();
				anchor._replySend++;
				anchor.setReceiveTimeout(10);
				anchor.state(S_FINAL_WAIT);
			}
			break;
		}
		case S_FINAL_WAIT: {
			anchor._finalReceived++;
			if (anchor.receiveFinalForAnchor()) {
				anchor._finalForAnchor++;
				anchor.calcDistance();
			}
			anchor.setReceiveTimeout(10);
			anchor.state(S_START);
			break;
		}

		}

	}
}
//______________________________________________________________________
//
void DWM1000_Anchor::onReceive(Header hdr, Cbor& cbor) {

	if (hdr.is(_mqtt, REPLY(CONNECT))) {
		_mqttConnected = true;
		return;
	} else if (hdr.is(_mqtt, REPLY(DISCONNECT))) {
		_mqttConnected = false;
		return;
	} else if (hdr.is(INIT)) {
		init();
		setReceiveTimeout(1000);
		return;
	};
	LOGF("");
	uint64_t end_time = millis() + 10;
	publish();
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXRFTO);
	// clear timeout
	dwt_setrxtimeout(0); /* Clear reception timeout to start next ranging process. */
	dwt_rxenable(0); /* Activate reception immediately. */
	clearInterrupt();
	while (end_time > millis()) {
		if (isInterruptDetected()) {
			_pollsReceived++;
			if (receivePollForAnchor()) {
				_pollsForAnchor++;
				sendReply();
				_replySend++;
				break;
			}

		}
	};
	if (isInterruptDetected()) {
		clearInterrupt();
		while (end_time > millis()) {
			if (isInterruptDetected()) {
				_finalReceived++;
				if (receiveFinalForAnchor()) {
					_finalForAnchor++;
					calcDistance();
				}
			}

		}
	}
	setReceiveTimeout(1);
	return;

	switch (state()) {
	case S_START: {
		publish();
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXRFTO);
		// clear timeout
		dwt_setrxtimeout(0); /* Clear reception timeout to start next ranging process. */
		dwt_rxenable(0); /* Activate reception immediately. */
		dwt_setinterrupt(DWT_INT_RFCG, 1); // enable RXD interrupt
		setReceiveTimeout(10);/* This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature. */
		state(S_POLL_WAIT);
		break;
	}
	case S_POLL_WAIT: {
		if (hdr.is(TIMEOUT)) {
			setReceiveTimeout(10);
			state(S_START);
		}
		break;
	}
	case S_FINAL_WAIT: {
		if (hdr.is(TIMEOUT)) {
			setReceiveTimeout(10);
			state(S_START);
		}
		break;
	}

	}
	/*
	 PT_BEGIN()
	 while (true) {

	 WAIT_POLL: {

	 while (true) { // Poll for reception of a frame or error/timeout. See NOTE 7 below.
	 publish();
	 dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXRFTO);
	 // clear timeout
	 dwt_setrxtimeout(0); // Clear reception timeout to start next ranging process.
	 dwt_rxenable(0); // Activate reception immediately.
	 dwt_setinterrupt(DWT_INT_RFCG, 1); // enable RXD interrupt
	 setReceiveTimeout(1000);// This is the delay from the end of the frame transmission to the enable of the receiver, as programmed for the DW1000's wait for response feature.

	 PT_YIELD_UNTIL(hdr.is(TIMEOUT) || hdr.is(RXD));
	 if (hdr.is(RXD)) {
	 _pollsReceived++;
	 _status_reg = dwt_read32bitreg(SYS_STATUS_ID);
	 break;
	 }
	 }
	 }
	 ///____________________________________________________________________________

	 if (hdr.is(RXD)) {
	 if (_mqttConnected)
	 _mqtt.tell(self(), PUBLISH, 0,
	 _cborOut.putf("ss", "dwm1000/status", "data received"));

	 uint32 frame_len;
	 dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);
	 // Clear good RX frame event in the DW1000 status register.
	 frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023; // A frame has been received, read it into the local buffer.
	 if (frame_len <= RX_BUFFER_LEN) {
	 dwt_readrxdata(rx_buffer, frame_len, 0);
	 }
	 //			LOGF(" frame_len : %d status_reg : %X",frame_len,_status_reg);

	 // Check that the frame is a poll sent by "DS TWR initiator" example.
	 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame.
	 rx_buffer[ALL_MSG_SN_IDX] = 0;
	 if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
	 _pollsForAnchor++;
	 uint32 resp_tx_time;

	 poll_rx_ts = get_rx_timestamp_u64(); // Retrieve poll reception timestamp.

	 // Set send time for response. See NOTE 8 below.
	 resp_tx_time = (poll_rx_ts
	 + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
	 dwt_setdelayedtrxtime(resp_tx_time);

	 // Set expected delay and timeout for final message reception.
	 dwt_setrxaftertxdelay(RESP_TX_TO_FINAL_RX_DLY_UUS);
	 dwt_setrxtimeout(FINAL_RX_TIMEOUT_UUS);

	 // Write and send the response message. See NOTE 9 below.
	 tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
	 dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
	 dwt_writetxfctrl(sizeof(tx_resp_msg), 0);
	 dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
	 _replySend++;

	 setReceiveTimeout(10);
	 dwt_setinterrupt(DWT_INT_RFCG, 1); // enable
	 PT_YIELD_UNTIL(hdr.is(TIMEOUT) || hdr.is(RXD));
	 // Increment frame sequence number after transmission of the response message (modulo 256).
	 frame_seq_nb++;
	 if (hdr.is(TIMEOUT)) {
	 LOGF(" timeout on final ")
	 goto WAIT_POLL;
	 } else if (hdr.is(RXD)) {
	 _mqtt.tell(self(), PUBLISH, 0,
	 _cborOut.putf("ss", "dwm1000/status", "replied"));
	 // Clear good RX frame event and TX frame sent in the DW1000 status register.
	 dwt_write32bitreg(SYS_STATUS_ID,
	 SYS_STATUS_RXFCG | SYS_STATUS_TXFRS);

	 // A frame has been received, read it into the local buffer.
	 frame_len = dwt_read32bitreg(
	 RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
	 if (frame_len <= RX_BUF_LEN) {
	 dwt_readrxdata(rx_buffer, frame_len, 0);
	 }

	 // Check that the frame is a final message sent by "DS TWR initiator" example.
	 // As the sequence number field of the frame is not used in this example, it can be zeroed to ease the validation of the frame.
	 rx_buffer[ALL_MSG_SN_IDX] = 0;
	 if (memcmp(rx_buffer, rx_final_msg, ALL_MSG_COMMON_LEN)
	 == 0) {
	 _finalReceived++;
	 uint32 poll_tx_ts, resp_rx_ts, final_tx_ts;
	 uint32 poll_rx_ts_32, resp_tx_ts_32, final_rx_ts_32;
	 double Ra, Rb, Da, Db;
	 int64 tof_dtu;

	 // Retrieve response transmission and final reception timestamps.
	 resp_tx_ts = get_tx_timestamp_u64();
	 final_rx_ts = get_rx_timestamp_u64();

	 // Get timestamps embedded in the final message.
	 final_msg_get_ts(&rx_buffer[FINAL_MSG_POLL_TX_TS_IDX],
	 &poll_tx_ts);
	 final_msg_get_ts(&rx_buffer[FINAL_MSG_RESP_RX_TS_IDX],
	 &resp_rx_ts);
	 final_msg_get_ts(&rx_buffer[FINAL_MSG_FINAL_TX_TS_IDX],
	 &final_tx_ts);

	 // Compute time of flight. 32-bit subtractions give correct answers even if clock has wrapped. See NOTE 10 below.
	 poll_rx_ts_32 = (uint32) poll_rx_ts;
	 resp_tx_ts_32 = (uint32) resp_tx_ts;
	 final_rx_ts_32 = (uint32) final_rx_ts;
	 Ra = (double) (resp_rx_ts - poll_tx_ts);
	 Rb = (double) (final_rx_ts_32 - resp_tx_ts_32);
	 Da = (double) (final_tx_ts - resp_rx_ts);
	 Db = (double) (resp_tx_ts_32 - poll_rx_ts_32);
	 tof_dtu = (int64) ((Ra * Rb - Da * Db)
	 / (Ra + Rb + Da + Db));

	 tof = tof_dtu * DWT_TIME_UNITS;
	 distance = tof * SPEED_OF_LIGHT;

	 // Display computed distance on LCD.
	 //						char dist_str[20];
	 //						sprintf(dist_str,"%3.2f", distance);
	 //                      lcd_display_str(dist_str);
	 LOGF( " distance : %d m", (float) distance);
	 }
	 }
	 } else {
	 LOGF(" it's no poll msg ");
	 }
	 } else {
	 LOGF(" it's no RXD ")
	 goto WAIT_POLL;
	 }

	 }

	 PT_END();
	 */
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_tx_timestamp_u64()
 *
 * @brief Get the TX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_tx_timestamp_u64(void) {
	uint8 ts_tab[5];
	uint64 ts = 0;
	int i;
	dwt_readtxtimestamp(ts_tab);
	for (i = 4; i >= 0; i--) {
		ts <<= 8;
		ts |= ts_tab[i];
	}
	return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_rx_timestamp_u64(void) {
	uint8 ts_tab[5];
	uint64 ts = 0;
	int i;
	dwt_readrxtimestamp(ts_tab);
	for (i = 4; i >= 0; i--) {
		ts <<= 8;
		ts |= ts_tab[i];
	}
	return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_get_ts()
 *
 * @brief Read a given timestamp value from the final message. In the timestamp fields of the final message, the least
 *        significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to read
 *         ts  timestamp value
 *
 * @return none
 */
static void final_msg_get_ts(const uint8 *ts_field, uint32 *ts) {
	int i;
	*ts = 0;
	for (i = 0; i < FINAL_MSG_TS_LEN; i++) {
		*ts += ts_field[i] << (i * 8);
	}
}

/*****************************************************************************************************************************************************
 * NOTES:
 *
 * 1. The sum of the values is the TX to RX antenna delay, experimentally determined by a calibration process. Here we use a hard coded typical value
 *    but, in a real application, each device should have its own antenna delay properly calibrated to get the best possible precision when performing
 *    range measurements.
 * 2. The messages here are similar to those used in the DecaRanging ARM application (shipped with EVK1000 kit). They comply with the IEEE
 *    802.15.4 standard MAC data frame encoding and they are following the ISO/IEC:24730-62:2013 standard. The messages used are:
 *     - a poll message sent by the initiator to trigger the ranging exchange.
 *     - a response message sent by the responder allowing the initiator to go on with the process
 *     - a final message sent by the initiator to complete the exchange and provide all information needed by the responder to compute the
 *       time-of-flight (distance) estimate.
 *    The first 10 bytes of those frame are common and are composed of the following fields:
 *     - byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
 *     - byte 2: sequence number, incremented for each new frame.
 *     - byte 3/4: PAN ID (0xDECA).
 *     - byte 5/6: destination address, see NOTE 3 below.
 *     - byte 7/8: source address, see NOTE 3 below.
 *     - byte 9: function code (specific values to indicate which message it is in the ranging process).
 *    The remaining bytes are specific to each message as follows:
 *    Poll message:
 *     - no more data
 *    Response message:
 *     - byte 10: activity code (0x02 to tell the initiator to go on with the ranging exchange).
 *     - byte 11/12: activity parameter, not used for activity code 0x02.
 *    Final message:
 *     - byte 10 -> 13: poll message transmission timestamp.
 *     - byte 14 -> 17: response message reception timestamp.
 *     - byte 18 -> 21: final message transmission timestamp.
 *    All messages end with a 2-byte checksum automatically set by DW1000.
 * 3. Source and destination addresses are hard coded constants in this example to keep it simple but for a real product every device should have a
 *    unique ID. Here, 16-bit addressing is used to keep the messages as short as possible but, in an actual application, this should be done only
 *    after an exchange of specific messages used to define those short addresses for each device participating to the ranging exchange.
 * 4. Delays between frames have been chosen here to ensure proper synchronisation of transmission and reception of the frames between the initiator
 *    and the responder and to ensure a correct accuracy of the computed distance. The user is referred to DecaRanging ARM Source Code Guide for more
 *    details about the timings involved in the ranging process.
 * 5. This timeout is for complete reception of a frame, i.e. timeout duration must take into account the length of the expected frame. Here the value
 *    is arbitrary but chosen large enough to make sure that there is enough time to receive the complete final frame sent by the responder at the
 *    110k data rate used (around 3.5 ms).
 * 6. In a real application, for optimum performance within regulatory limits, it may be necessary to set TX pulse bandwidth and TX power, (using
 *    the dwt_configuretxrf API call) to per device calibrated values saved in the target system or the DW1000 OTP memory.
 * 7. We use polled mode of operation here to keep the example as simple as possible but all status events can be used to generate interrupts. Please
 *    refer to DW1000 User Manual for more details on "interrupts". It is also to be noted that STATUS register is 5 bytes long but, as the event we
 *    use are all in the first bytes of the register, we can use the simple dwt_read32bitreg() API call to access it instead of reading the whole 5
 *    bytes.
 * 8. Timestamps and delayed transmission time are both expressed in device time units so we just have to add the desired response delay to poll RX
 *    timestamp to get response transmission time. The delayed transmission time resolution is 512 device time units which means that the lower 9 bits
 *    of the obtained value must be zeroed. This also allows to encode the 40-bit value in a 32-bit words by shifting the all-zero lower 8 bits.
 * 9. dwt_writetxdata() takes the full size of the message as a parameter but only copies (size - 2) bytes as the check-sum at the end of the frame is
 *    automatically appended by the DW1000. This means that our variable could be two bytes shorter without losing any data (but the sizeof would not
 *    work anymore then as we would still have to indicate the full length of the frame to dwt_writetxdata()). It is also to be noted that, when using
 *    delayed send, the time set for transmission must be far enough in the future so that the DW1000 IC has the time to process and start the
 *    transmission of the frame at the wanted time. If the transmission command is issued too late compared to when the frame is supposed to be sent,
 *    this is indicated by an error code returned by dwt_starttx() API call. Here it is not tested, as the values of the delays between frames have
 *    been carefully defined to avoid this situation.
 * 10. The high order byte of each 40-bit time-stamps is discarded here. This is acceptable as, on each device, those time-stamps are not separated by
 *     more than 2**32 device time units (which is around 67 ms) which means that the calculation of the round-trip delays can be handled by a 32-bit
 *     subtraction.
 * 11. The user is referred to DecaRanging ARM application (distributed with EVK1000 product) for additional practical example of usage, and to the
 *     DW1000 API Guide for more details on the DW1000 driver functions.
 ****************************************************************************************************************************************************/
