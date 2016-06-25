/*
 * Actor.cpp
 *
 *  Created on: May 14, 2016
 *      Author: lieven
 */

#include "Actor.h"
#include <Sys.h>
#include <CborQueue.h>
#include <Arduino.h>
#include <stdio.h>
#include <cstdarg>
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFL
#endif
//#define LOGF(fmt,...) printf("%s:%s ",__FILE__,__FUNCTION__ );vprintf(fmt,##__VA_ARGS__);printf("\n");

/*	INIT = 0,
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
 ANY = 0xFF*/
const char*strEvent[] = { "INIT", "TIMEOUT", "STOP", "RESTART", "CONFIG", "TXD",
		"RXD", "CONNECT", "DISCONNECT", "ADD_LISTENER", "PUBLISH", "SUBSCRIBE" };

char sEvent[20];
Cbor Actor::_cborOut(256);
Cbor Actor::_cborIn(256);

char* strcat(char *dst, const char* src) {
	while (*dst)
		dst++;
	while (*src) {
		*dst = *src;
		dst++;
		src++;
	}
	return dst;
}

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

Header& Header::src(ActorRef src) {
	_src = src.idx();
	return *this;
}

ActorRef Header::src() {
	return ActorRef(_src);
}

Header& Header::dst(ActorRef dst) {
	_dst = dst.idx();
	return *this;
}

ActorRef Header::dst() {
	return ActorRef(_dst);
}

bool Header::is(int dst, int src, Event event, uint8_t detail) {
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

bool Header::is(ActorRef src, uint8_t event) {
	return src.idx() == _src && event == _event;
}

bool Header::is(ActorRef dst, ActorRef src, Event event, uint8_t detail) {
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
	_cborQueue = new CborQueue(1024);
	_dummy = new Actor("Dummy");
	LOGF(" CborQueue %X:%d ", _cborQueue, _cborQueue->_size);
}

Actor& Actor::dummy() {
	return *_dummy;
}

Actor::Actor(const char* path) {
	_idx = _count++;
	ASSERT_LOG(_count < MAX_ACTORS);
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
	LOGF(" Default handler event ? %s => %s => %s",
			ActorRef(hdr._src).path(), eventToString(hdr._event), ActorRef(hdr._dst).path());
}

bool Actor::subscribed(Header hdr) {
	return hdr._dst == _idx || hdr._dst == ANY;
}

void Actor::unhandled(Header hdr) {
	LOGF(" unhandled event %s from %s to %s",
			eventToString(hdr._event), ActorRef(hdr._src).path(), ActorRef(hdr._dst).path());
}

//______________________________________________________
//
//______________________________________________________
//

const char* Actor::path() {
	if (_idx == ANY)
		return "ANY";
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
#define LOGHEADER(__hdr,__cbor) LOGF("  %s => %s:%d => %s ",Actor::idxToPath(__hdr._src),Actor::eventToString(__hdr._event),__cbor.length(),Actor::idxToPath(__hdr._dst))

void Actor::send(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	LOGF(" fmt : %s", fmt);
	LOGF(" cborQueue %X ", _cborQueue);
	Erc erc = _cborQueue->vputf(fmt, args);
	va_end(args);
	ASSERT_LOG(erc == E_OK);
}

void Actor::tell(Header header, Cbor& cbor) {
//	logHeader(header);
//	LOGF(" %s >>  ( %s,%d )  >> %s", Actor::byIndex(header._src).path(),
//			strEvent[header._event], header._detail,
//			Actor::byIndex(header._dst).path());
	header._dst = self().idx();
//	LOGHEADER(header, cbor);
	ASSERT_LOG(_cborQueue->putf("uB", header._word, (Bytes*)&cbor) == E_OK);
}

void ActorRef::tell(ActorRef src, Event event, uint8_t detail, Cbor& cbor) {
	Header hdr(*this, src, event, detail);
	tell(hdr, cbor);
}

void Actor::tell(ActorRef src, Event event, uint8_t detail) {
	Header w(self(), src, event, detail);
	Cbor cbor(0);
	tell(w, cbor);
}

void ActorRef::delegate(Header hdr, Cbor& data) {
	actor().onReceive(hdr, data);
}

void Actor::broadcast(ActorRef src, Event event, uint8_t detail) {
	_cborOut.clear();
	_cborQueue->putf("uB", Header(ANY, src.idx(), event, detail), &_cborOut);
}

void Actor::logHeader(Header hdr) {
	LOGF(" event %s => {%s,%d} => %s ",
			Actor::idxToPath(hdr._src), Actor::eventToString(hdr._event), hdr._detail, Actor::idxToPath(hdr._dst));
}

void Actor::eventLoop() {
	for (uint32_t i = 0; i < _count; i++) {
		Actor* actor = Actor::_actors[i];
		actor->poll();
	}

	while (_cborQueue->hasData()) {
		Header hdr;
		ASSERT_LOG(_cborQueue->getf("uB", &hdr, &_cborIn) == E_OK);
		_cborIn.offset(0);

		for (uint32_t i = 0; i < _count; i++) {
			if (Actor::_actors[i]->subscribed(hdr)) {
				_cborIn.offset(0);
				_cborOut.clear();
				/*				LOGF("event %s => {%s,%d,%d} => %s ",
				 Actor::idxToPath(hdr._src), //
				 Actor::eventToString(hdr._event),//
				 hdr._detail,//
				 _cborIn.length(),//
				 Actor::_actors[i]->path());*/
				Actor::byIndex(i).onReceive(hdr, _cborIn);
			}
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
				_cborOut.clear();
				actor->onReceive(header, cbor);

			}
		}
	}

}

void Actor::poll() { // dummy return directly
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

bool ActorRef::operator==(ActorRef ref) {
	return ref._idx == _idx;
}

bool ActorRef::operator!=(ActorRef ref) {
	return ref._idx != _idx;
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

