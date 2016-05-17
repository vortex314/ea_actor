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

const char*strEvent[] = { "INIT", "TIMEOUT", "STOP", "RESTART", "CONFIG", "TXD",
		"RXD", "CONNECT", "DISCONNECT", "CONNECTED", "DISCONNECTED" };

const char* eventString(int event) {
	return strEvent[event];
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
//	LOGF("ctor %s",path);
	_idx = _count++;
	_actors[_idx] = this;
	_path = path;
	_timeout = UINT64_MAX;
	_left = _right = _self = ActorRef(this);
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
	header.dst = actor.idx();
	header.src = idx();
	header.event = event;
	header.detail = detail;
	return header;
}

void Actor::onReceive(Header hdr, Cbor& data) {
	LOGF(" Default handler event %s from %s to %s", strEvent[hdr.event],
			ActorRef(hdr.src).path(), ActorRef(hdr.dst).path());
}

void Actor::unhandled(Header hdr) {
	LOGF(" unhandled event %s from %s to %s", strEvent[hdr.event],
			ActorRef(hdr.src).path(), ActorRef(hdr.dst).path());
}

//______________________________________________________
//
//______________________________________________________
//

const char* Actor::path() {
	return _path;
}

void Actor::link(ActorRef left, ActorRef right) {
	left.actor()._right = right;
	right.actor()._left = left;
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

void Actor::tell(Header header, Cbor& bytes) {
	LOGF(" %s >>  ( %s,%d )  >> %s", Actor::byIndex(header.src).path(),
			strEvent[header.event], header.detail,
			Actor::byIndex(header.dst).path());

	Erc erc = _cborQueue->putf("uB", header.word, &bytes);
	if (erc)
		LOGF("  >> erc : %d ", erc);
}

void Actor::tell(ActorRef src, Event event, uint8_t detail) {
	Header w;
	Cbor cbor(0);
	w.dst = idx();
	w.src = src.idx();
	w.event = event;
	w.detail = detail;
	tell(w, cbor);
}

void Actor::broadcast(Actor& src, Event event, uint8_t detail) {
	Header w;
	Cbor cbor(0);
	w.dst = BROADCAST;
	w.src = src.idx();
	w.event = event;
	w.detail = detail;
	Erc erc = _cborQueue->putf("uB", w.word, &cbor);
}

void Actor::eventLoop() {
	while (_cborQueue->hasData()) {
		Cbor cbor(100);
		Header header;
		_cborQueue->getf("uB", &header, &cbor);
		if (header.dst == BROADCAST) {
			for (int i = 0; i < _count; i++) {
				Actor::byIndex(i).onReceive(header, cbor);
			}
		} else if (header.dst < _count) {
			Actor::byIndex(header.dst).onReceive(header, cbor);
		} else {
			LOGF(" invalid dst : %d", header.dst);
		}
	};
	for (int i = 0; i < _count; i++) {
		Actor* actor = _actors[i];
		if (actor == 0) {
			break;
		} else {
			if (actor->timeout()) {
				Cbor cbor(0);
				actor->onReceive(actor->header(dummy(), TIMEOUT, 0), cbor);
			}
		}
	}
	for (int i = 0; i < _count; i++) {
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
void ActorRef::tell(ActorRef src, Event event, uint8_t detail) {
	actor().tell(src, event, detail);
}

ActorRef ActorRef::operator>>(ActorRef ref) {
	Actor::link(*this, ref);
	return ref;
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
