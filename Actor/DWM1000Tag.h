/*
 * DWM1000Tag.h
 *
 *  Created on: Jun 7, 2016
 *      Author: lieven
 */

#ifndef DWM1000TAG_H_
#define DWM1000TAG_H_
#include <Actor.h>
extern "C" {
#include <Spi.h>
#include <gpio_c.h>
#include <espmissingincludes.h>
#include <osapi.h>
#include "deca_device_api.h"
#include "deca_regs.h"
#include "deca_sleep.h"
};

class DWM1000_Tag: public Actor {
	uint32_t _count;
	ActorRef _mqtt;
	bool _mqttConnected;
	uint32_t _pollsSend;
	uint32_t _replyReceived;
	uint32_t _replyForTag;
	uint32_t _finalSend;
	uint32_t _interrupts;
	enum State {
		S_START,S_REPLY_WAIT
	};
	volatile bool _interrupted;
public:
	DWM1000_Tag(ActorRef mqtt);
	static ActorRef create(ActorRef mqtt);
	virtual ~DWM1000_Tag();
	void mode(uint32_t m);
	void init();
	void resetChip();
	void initSpi();
	void sendPoll();
	void sendFinal();
	void onReceive(Header, Cbor&);
	bool subscribed(Header hdr);
	void enableIsr();
	static void rxcallback(const dwt_callback_data_t * data);
	static void txcallback(const dwt_callback_data_t * data);
	static bool interrupt_detected;
	uint32_t _status_reg;
	static void my_dwt_isr();//
	bool isInterruptDetected();
	void clearInterrupt();
	void publish();
	bool receiveReplyForTag();
	void onRxCallback();
};

#endif /* DWM1000TAG_H_ */
