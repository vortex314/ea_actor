/*
 * Actor.h
 *
 *  Created on: May 14, 2016
 *      Author: lieven
 */

#ifndef ACTOR_H_
#define ACTOR_H_
#include <stdint.h>
#include <Sys.h>
#include <Cbor.h>
#include <CborQueue.h>

typedef enum {
	INIT = 0,
	TIMEOUT,
	STOP,
	RESTART,
	CONFIG,	// detail =0, initial params in payload, =1 , load config from flash again
	TXD,
	RXD,
	CONNECT,
	DISCONNECT,
	ADD_LISTENER,
	PUBLISH,
	SUBSCRIBE,
	RESPONSE = 0x80,
	ANY = 0xFF
} Event;
#define REPLY(xxx) (Event)(xxx + RESPONSE)
#define MAX_ACTORS 20

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
	Header() {
		_word = 0;
	}
	Header(ActorRef dst, ActorRef src, Event event, uint8_t detail);
	Header(int dst, int src, Event event, uint8_t detail); //
	bool matches(ActorRef dst, ActorRef src, Event event, uint8_t detail); //
	bool matches(int dst, int src, Event event, uint8_t detail); //
	bool is(uint8_t event, uint8_t detail); //
	inline bool is(uint8_t event) {
		return _event == event;
	}
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
	void forward(Header header, Cbor& data); // handle by another onReceive, keep header
	void delegate(Header header, Cbor& data); // handle by another onReceive, keep header
	void route(Header, Cbor&); // change destination and put back on queue
	void tell(ActorRef src, Event event, uint8_t detail);
	void tell(ActorRef src, Event event, uint8_t detail,Cbor& cbor);
	void reply(Header hdr,Cbor& cbor);
	ActorRef operator>>(ActorRef ref);
	uint8_t idx() {
		return _idx;
	}
	bool equal(ActorRef ref);
	const char* path();
	static ActorRef dummy() {
		return ActorRef(0);
	}
	static ActorRef byPath(const char* path);
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
	static const char* eventToString(uint8_t event);
	static const char* idxToPath(uint8_t idx);
	static void logHeader(Header);
public:
	static Cbor _cborOut;
	static Cbor _cborIn;
	Actor(const char* name);
	virtual ~Actor();
	ActorRef self();
	static void link(ActorRef left, ActorRef right);
	void tellf(Header hdr, const char* fmt, ...);
	void tell(Header header, Cbor& data);
	void tell(ActorRef src, Event event, uint8_t detail);
	static void send(const char* fmt,...);
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
