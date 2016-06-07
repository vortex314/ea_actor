/*
 * DWM1000Tag.h
 *
 *  Created on: Jun 7, 2016
 *      Author: lieven
 */

#ifndef DWM1000TAG_H_
#define DWM1000TAG_H_
#include <Actor.h>

class DWM1000_Tag: public Actor {
	uint32_t _count;
	static uint32_t _status_reg;
	ActorRef _mqtt;
public:
	DWM1000_Tag(ActorRef mqtt);
	static ActorRef create(ActorRef mqtt);
	virtual ~DWM1000_Tag();
	void mode(uint32_t m);
	void init();
	void resetChip();
	void initSpi();
	void onReceive(Header ,Cbor&);
	void enableIsr();
	static bool interrupt_detected ;
	static void my_dwt_isr();
	bool isInterruptDetected();
	void clearInterrupt();
};

#endif /* DWM1000TAG_H_ */
