/*
 * DWM1000_Anchor_Tag.h
 *
 *  Created on: Feb 12, 2016
 *      Author: lieven
 */

#ifndef DWM1000_Anchor_H_
#define DWM1000_Anchor_H_

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

class DWM1000_Anchor: public Actor {
	uint32_t _count;
//	static uint32_t _status_reg;
	ActorRef _mqtt;
	bool _mqttConnected;
	const char* _status;
	uint32_t _pollsReceived;
	uint32_t _pollsForAnchor;
	uint32_t _replySend;
	uint32_t _finalReceived;
	uint32_t _finalForAnchor;
	volatile uint32_t _interrupts;
	volatile bool _interrupted;
	enum State {
		S_START,S_POLL_WAIT,S_REPLY_SEND,S_FINAL_WAIT,S_FINAL_SEND
	};
public:
	DWM1000_Anchor(ActorRef mqtt);
	static ActorRef create(ActorRef mqtt);
	virtual ~DWM1000_Anchor();
	void mode(uint32_t m);
	void init();
	void resetChip();
	void initSpi();
	void enableIsr();
	static bool interrupt_detected ;
	static void my_dwt_isr();
	bool isInterruptDetected();
	void clearInterrupt();
	static void rxcallback(const dwt_callback_data_t * data);
	static void txcallback(const dwt_callback_data_t * data);
	void publish();
	bool receivePollForAnchor();
	bool receiveFinalForAnchor();
	void calcDistance();
	void sendReply();

	bool subscribed(Header);
	void onReceive(Header ,Cbor&);
};

#endif /* DWM1000_Anchor_Tag_H_ */
