/*
 * Actor.cpp
 *
 *  Created on: May 14, 2016
 *      Author: lieven
 */

#include "Actor.h"
#include <Logger.h>
#include <CborQueue.h>
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFL
#endif
//#define LOGF(fmt,...) printf("%s:%s ",__FILE__,__FUNCTION__ );vprintf(fmt,##__VA_ARGS__);printf("\n");

const char*strEvent[] = { "INIT", "TIMEOUT", "STOP", "RESTART", "CONFIG", "TXD",
		"RXD", "CONNECT", "DISCONNECT", "CONNECTED", "DISCONNECTED" };

char sEvent[20];
Cbor Actor::_cborOut(256);

const char* Actor::eventToString(uint8_t event) {
	if (event & 0x80) {
		strcpy(sEvent, "REPLY(");
		event &= 0x7F;
		strcat(sEvent, eventToString(event));
		strcat(sEvent, ")");
		return sEvent;
	}
	if (event > sizeof(strEvent)) {
		return "UNKNOWN";
	} else
		return strEvent[event];
}

const char* Actor::idxToPath(uint8_t idx) {
	if (idx < _count) {
		return _actors[idx]->_path;
	} else if (idx == ANY) {
		return "ANY";
	} else
		return "UNKNOWN";
}

Header::Header(ActorRef dst, ActorRef src, Event event, uint8_t detail) {
	_dst = dst.idx();
	_src = src.idx();
	_event = event;
	_detail = detail;
}
Header::Header(int dst, int src, Event event, uint8_t detail) {
	_dst = dst;
	_src = src;
	_event = event;
	_detail = detail;
}
bool Header::matches(int dst, int src, Event event, uint8_t detail) {
	if (dst == ANY || dst == _dst) {
		if (src == ANY || src == _src) {
			if (event == ANY || event == _event) {
				return true;
			}
		}
	}
	return false;
}

bool Header::is(uint8_t event, uint8_t detail) {

	if (event == ANY || event == _event) {
		if (detail == ANY || detail == _detail) {
			return true;
		}
	}
	return false;
}
bool Header::matches(ActorRef dst, ActorRef src, Event event, uint8_t detail) {
	if (dst.idx() == ANY || dst.idx() == _dst) {
		if (src.idx() == ANY || src.idx() == _src) {
			if (event == ANY || _event == event) {
				return true;
			}
		}
	}
	return false;
}

CborQueue* Actor::_cborQueue;
Actor* Actor::_actors[MAX_ACTORS];
uint32_t Actor::_count = 0;
Actor* Actor::_dummy = 0;

void Actor::setup() {
	_dummy = new Actor("Dummy");
	_cborQueue = new CborQueue(1024);
}

Actor& Actor::dummy() {
	return *_dummy;
}

Actor::Actor(const char* path) {
	_idx = _count++;
	_actors[_idx] = this;
	_path = path;
//	LOGF("ctor %s %d ",_path,_idx);
	_timeout = UINT64_MAX;
	_left = _right = _self = ActorRef(this);
	_ptLine = 0;
}

Actor& Actor::byIndex(uint8_t idx) {
	if (idx < _count)
		return *_actors[idx];
	else
		return dummy();
}

Actor::~Actor() {
	_actors[_idx] = &dummy();
}

ActorRef Actor::self() {
	return _self;
}

Header Actor::header(Actor& actor, Event event, uint8_t detail) {
	Header header;
	header._dst = actor.idx();
	header._src = idx();
	header._event = event;
	header._detail = detail;
	return header;
}

void Actor::onReceive(Header hdr, Cbor& data) {
	LOGF(" Default handler event %s from %s to %s", strEvent[hdr._event],
			ActorRef(hdr._src).path(), ActorRef(hdr._dst).path());
}

void Actor::unhandled(Header hdr) {
	LOGF(" unhandled event %s from %s to %s", strEvent[hdr._event],
			ActorRef(hdr._src).path(), ActorRef(hdr._dst).path());
}

//______________________________________________________
//
//______________________________________________________
//

const char* Actor::path() {
	return _path;
}

void Actor::setReceiveTimeout(uint32_t msec) {
	_timeout = millis() + msec;
}

bool Actor::timeout() {
	return millis() > _timeout;
}

uint8_t Actor::idx() {
	return _idx;
}

void Actor::tellf(Header hdr, const char* fmt, ...) {
	Cbor msg(1000);
	msg.add(hdr._word);
	va_list args;
	va_start(args, fmt);
	msg.vaddf(fmt, args);
	va_end(args);
	_cborQueue->put(msg);
}

void Actor::tell(Header header, Cbor& bytes) {
//	logHeader(header);
//	LOGF(" %s >>  ( %s,%d )  >> %s", Actor::byIndex(header._src).path(),
//			strEvent[header._event], header._detail,
//			Actor::byIndex(header._dst).path());
	header._dst = self().idx();
	Erc erc = _cborQueue->putf("uB", header._word, &bytes);
	if (erc) {
		LOGF("  >> erc : %d ", erc);
	};
}

void Actor::tell(ActorRef src, Event event, uint8_t detail,Cbor& cbor) {
	Header w(self(), src, event, detail);
	tell(w, cbor);
}

void Actor::tell(ActorRef src, Event event, uint8_t detail) {
	Header w(self(), src, event, detail);
	Cbor cbor(0);
	tell(w, cbor);
}

void ActorRef::delegate(Header hdr, Cbor& data) {
	actor().onReceive(hdr, data);
}

void Actor::broadcast(Actor& src, Event event, uint8_t detail) {
	Header w(ANY, src.idx(), event, detail);
	Cbor cbor(0);
	LOGF("broacast %X", w._word);
	Erc erc = _cborQueue->putf("uB", w._word, &cbor);
	if (erc) {
		LOGF(" cborQueue put failed : %d ", erc);
	}
}
#define LOGHEADER(__hdr) LOGF("  %s => %s => %s ",Actor::idxToPath(__hdr._src),Actor::eventToString(__hdr._event),Actor::idxToPath(__hdr._dst))
void Actor::logHeader(Header hdr) {
	LOGF(" event %s => %s,%d => %s ", Actor::idxToPath(hdr._src),
			Actor::eventToString(hdr._event), hdr._detail,
			Actor::idxToPath(hdr._dst));
}

Cbor _data(256);

void Actor::eventLoop() {
	while (_cborQueue->hasData()) {
		Header header;
		_cborQueue->getf("uB", &header, &_data);
		_data.offset(0);
//		LOGF("cbor length : %d ",_data.length());
		if (header._dst == ANY) {
			for (uint32_t i = 0; i < _count; i++) {
				header._dst = i;
				LOGHEADER(header);
				Actor::byIndex(i).onReceive(header, _data);
			}
		} else if (header._dst < _count) {
			LOGHEADER(header);
			Actor::byIndex(header._dst).onReceive(header, _data);
		} else {
			LOGHEADER(header);
			LOGF(" invalid dst : %d", header._dst);
		}
	};
	for (uint32_t i = 0; i < _count; i++) {
		Actor* actor = _actors[i];
		if (actor == 0) {
			break;
		} else {
			if (actor->timeout()) {
				Cbor cbor(0);
				Header header(_actors[i]->_idx, _dummy->_idx, TIMEOUT, 0);
//				LOGHEADER(header);
				actor->setReceiveTimeout(UINT32_MAX);
				actor->onReceive(header, cbor);

			}
		}
	}
	for (uint32_t i = 0; i < _count; i++) {
		Actor* actor = Actor::_actors[i];
		actor->poll();
	}
}

void Actor::poll() {	// dummy return directly
}

ActorRef::ActorRef() {
	_idx = 0; // Actor::dummy().idx();
}
ActorRef::ActorRef(Actor* actor) {
	_idx = actor->idx();
}

ActorRef::ActorRef(int idx) {
	_idx = idx;
}

Actor& ActorRef::actor() {
	return Actor::byIndex(_idx);
}

ActorRef::~ActorRef() {
	if (_idx != 0) {
//		delete &Actor::byIndex(_idx);
		_idx = 0;
	}
}

bool ActorRef::equal(ActorRef ref) {
	return _idx == ref._idx;
}

void ActorRef::tell(Header header, Cbor& data) {
	actor().tell(header, data);
}
void ActorRef::forward(Header header, Cbor& data) {
	actor().onReceive(header, data);
}
void ActorRef::tell(ActorRef src, Event event, uint8_t detail) {
	actor().tell(src, event, detail);
}

ActorRef ActorRef::operator>>(ActorRef ref) {
	Actor::link(*this, ref);
	return ref;
}

void Actor::link(ActorRef left, ActorRef right) {
	left.actor()._right = right;
	right.actor()._left = left;
}

const char* ActorRef::path() {
	return actor().path();
}

//______________________________________________________
//
/*
 #define MAX_RIGHTS 4
 class Router: public Actor {
 Actor& _rights[MAX_RIGHTS];
 Actor& _left;
 Router() {
 _left = deadLetterActor;
 for (int i = 0; i < MAX_RIGHTS; i++)
 _rights[i] = deadLetterActor;reference
 }

 void onReceive(Event event, int detail, Cbor& cbor) {
 if (sender() == left()) {
 for (int i = 0; i < MAX_RIGHTS; i++)
 if (_rights[i] != 0) {
 _rights[i].sender(sender());
 _rights[i].onReceive(event, detail, cbor);
 }
 } else {
 for (int i = 0; i < MAX_RIGHTS; i++)
 if (sender() == right(i)) {
 _left.sender(sender());
 _left.onReceive(event, detail, cbor);
 return;
 }
 deadLetterActor.onReceive(event, detail, cbor);
 }
 }
 Actor& left() {
 return _left;
 }
 Actor& right(int i) {
 return _rights[i];
 }
 };



 class ChildActor: public Actor {
 Actor& _parent;
 public:
 ChildActor(Actor& parent) :
 Actor("child") {
 _parent = parent;
 }
 ~ChildActor() {

 }
 void onReceive(Event event, int detail, Cbor& cbor) {
 if (event == TXD) {
 _parent.tell(self(), TXD, 0, cbor)
 }
 }
 };
 class ParentActor: public Actor {
 public:
 ParentActor() :
 Actor("child") {

 }
 ~ParentActor() {

 }
 void onReceive(Event event, int detail, Cbor& cbor) {

 }
 };

 int main() {
 Actor& child = ChildActor();

 Cbor cbor;

 child.tell(child, TXD, 0, cbor);
 child >> child >> child;

 cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!

 return 0;
 }
 */
