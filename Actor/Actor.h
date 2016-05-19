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
	ANY = 0xFF
} Event;
#define MAX_ACTORS 10
//#define ANY 0xFF
/*
 typedef union {
 struct {
 uint8_t dst;
 uint8_t src;
 uint8_t event;
 uint8_t detail;
 };
 uint32_t word;


 } Header;

 */
class ActorRef;
class Header {
public:
	union {
		struct {
			uint8_t _dst;
			uint8_t _src;
			uint8_t _event;
			uint8_t _detail;
		};
		uint32_t _word;
	};
	Header(ActorRef dst, ActorRef src, Event event, uint8_t detail);
	Header(int dst, int src, Event event, uint8_t detail);
	bool matches(ActorRef dst, ActorRef src, Event event, uint8_t detail);
	bool matches(int dst, int src, Event event, uint8_t detail);
};

//#define LOGF(fmt,...) PrintHeader(__FILE__,__LINE__,__FUNCTION__);Serial.printf(fmt,##__VA_ARGS__);Serial.println();
//extern void PrintHeader(const char* file, uint32_t line, const char *function);

#define PT_BEGIN() bool ptYielded = true; switch (_ptLine) { case 0: // Declare start of protothread (use at start of Run() implementation).
#define PT_END() default: ; } ; return ; // Stop protothread and end it (use at end of Run() implementation).
// Cause protothread to wait until given condition is true.
#define PT_WAIT_UNTIL(condition) \
    do { _ptLine = __LINE__; case __LINE__: \
    if (!(condition)) return ; } while (0)

#define PT_WAIT_WHILE(condition) PT_WAIT_UNTIL(!(condition)) // Cause protothread to wait while given condition is true.
#define PT_WAIT_THREAD(child) PT_WAIT_WHILE((child).dispatch(msg)) // Cause protothread to wait until given child protothread completes.
// Restart and spawn given child protothread and wait until it completes.
#define PT_SPAWN(child) \
    do { (child).restart(); PT_WAIT_THREAD(child); } while (0)

// Restart protothread's execution at its PT_BEGIN.
#define PT_RESTART() do { restart(); return ; } while (0)

// Stop and exit from protothread.
#define PT_EXIT() do { stop(); return ; } while (0)

// Yield protothread till next call to its Run().
#define PT_YIELD() \
    do { ptYielded = false; _ptLine = __LINE__; case __LINE__: \
    if (!ptYielded) return ; } while (0)

// Yield protothread until given condition is true.
#define PT_YIELD_UNTIL(condition) \
    do { ptYielded = false; _ptLine = __LINE__; case __LINE__: \
    if (!ptYielded || !(condition)) return ; } while (0)
// Used to store a protothread's position (what Dunkels calls a
// "local continuation").
typedef unsigned short LineNumber;
// An invalid line number, used to mark the protothread has ended.
static const LineNumber LineNumberInvalid = (LineNumber) (-1);
// Stores the protothread's position (by storing the line number of
// the last PT_WAIT, which is then switched on at the next Run).

class Actor;

class ActorRef {
	uint8_t _idx;
protected:

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
	LineNumber _ptLine;
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

	static bool isHeader(ActorRef dst, ActorRef src, Event event,
			uint8_t detail);
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
