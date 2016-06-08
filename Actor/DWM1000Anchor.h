/*
 * DWM1000_Anchor_Tag.h
 *
 *  Created on: Feb 12, 2016
 *      Author: lieven
 */

#ifndef DWM1000_Anchor_H_
#define DWM1000_Anchor_H_

#include <Actor.h>

class DWM1000_Anchor: public Actor {
	uint32_t _count;
	static uint32_t _status_reg;
	ActorRef _mqtt;
	bool _mqttConnected;
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

	bool subscribed(Header);
	void onReceive(Header ,Cbor&);
};

#endif /* DWM1000_Anchor_Tag_H_ */
