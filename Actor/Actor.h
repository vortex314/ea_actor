/*
 * Actor.h
 *
 *  Created on: May 14, 2016
 *      Author: lieven
 */

#ifndef ACTOR_H_
#define ACTOR_H_

#include <Sys.h>
#include <Cbor.h>
#include <CborQueue.h>

typedef enum {
	INIT,
	TIMEOUT,
	STOP,
	RESTART,
	CONFIG,
	TXD,
	RXD,
	CONNECT,
	DISCONNECT,
	CONNECTED,
	DISCONNECTED,
} Event;



typedef union {
	struct {
		uint8_t dst;
		uint8_t src;
		uint8_t event;
		uint8_t detail;
	};
	uint32_t word;
} Header;

//#define LOGF(fmt,...) PrintHeader(__FILE__,__LINE__,__FUNCTION__);Serial.printf(fmt,##__VA_ARGS__);Serial.println();
//extern void PrintHeader(const char* file, uint32_t line, const char *function);

class Actor;
class ActorRef {
	uint8_t _idx;
private:

public:
	ActorRef();
	ActorRef(Actor* actor);
	ActorRef(int idx);
	Actor& actor();
	~ActorRef();
	void tell(Header header, Cbor& data);
	void forward(Header header, Cbor& data);
	void tell(ActorRef src, Event event, uint8_t detail);
	ActorRef operator>>(ActorRef ref);
	uint8_t idx() {
		return _idx;
	}
	bool equal(ActorRef ref);
	const char* path();
};

#define MAX_ACTORS 10
#define BROADCAST 0xFF
class Actor {
private:
	uint8_t _idx;
	const char* _path;
	uint64_t _timeout;
	ActorRef _self;
	ActorRef _left;
	ActorRef _right;
	static Actor* _actors[MAX_ACTORS];
	static uint32_t _count;
	static Actor* _dummy;
	static CborQueue* _cborQueue;
protected:

public:
	Actor(const char* name);
	virtual ~Actor();
	ActorRef self();
	static void link(ActorRef left, ActorRef right);
	void tell(Header header, Cbor& data);
	void tell(ActorRef src, Event event, uint8_t detail);
	static Actor& byIndex(uint8_t idx);
	static Actor& dummy();
	static void setup();
	static void eventLoop();
	Header header(Actor& actor, Event event, uint8_t detail);

	uint8_t idx();
	const char* path();
	static void broadcast(Actor& src, Event event, uint8_t detail);
	virtual void onReceive(Header header, Cbor& data);
	virtual void poll();
	void unhandled(Header header);

	ActorRef left() {
		return _left;
	}
	ActorRef right() {
		return _right;
	}
	bool timeout();
	void setReceiveTimeout(uint32_t time);
};

#endif /* ACTOR_H_ */
