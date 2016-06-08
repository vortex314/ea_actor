/*
 * Tcp.cpp
 *
 *  Created on: Oct 24, 2015
 *      Author: lieven
 */

#include "Tcp.h"
#include <string.h>
//#include <Logger.h>

//uint32_t Tcp::_maxConnections = 5;
Tcp* Tcp::_first = 0;

Tcp::Tcp() :
		Actor("Tcp"), _txd(1024), _buffer(1024) {
	LOGF("this : %X ", this);
	_type = LIVE;
	_next = 0;
	_conn = 0;
	strcpy(_host, "");
	_remote_port = 80;
	_remote_ip.addr = 0;
	_connected = false;
	_bytesTxd = 0;
	_bytesRxd = 0;
	_overflowTxd = 0;
	_connections = 0;
	_local_port = 0;
	_lastRxd = Sys::millis();
	reg();
	_sendState = READY;
}

Tcp::~Tcp() {
	if (_type == SERVER || _type == CLIENT) {
		if (_conn->proto.tcp)
			free(_conn->proto.tcp);
		free(_conn);
	}
	unreg();
}

void Tcp::reg() {
	Tcp* cursor;
	if (_first == 0) {
		_first = this;
	} else {
		for (cursor = _first; cursor->_next != 0; cursor = cursor->_next)
			;
		cursor->_next = this;
	}
}

void Tcp::unreg() {
	Tcp* cursor;
	if (_first == this) {
		_first = _next;
	} else {
		for (cursor = _first; cursor->_next != 0; cursor = cursor->_next)
			if (cursor->_next == this)
				cursor->_next = _next;
	}
}
/*
 uint32_t Tcp::used() {
 uint32_t count = 0;
 Tcp* cursor;
 for (cursor = _first; cursor != 0; cursor = cursor->_next) {
 if (cursor->isConnected())
 count++;
 }
 return count;
 }

 uint32_t Tcp::count() {
 uint32_t count = 0;
 Tcp* cursor;
 for (cursor = _first; cursor != 0; cursor = cursor->_next)
 count++;
 return count;
 }*/

bool Tcp::match(struct espconn* pconn, Tcp* pTcp) {
	/*	LOGF(" compare %X:%d : %X:%d ", *((uint32_t* )pTcp->_remote_ip.b),
	 pTcp->_remote_port, *((uint32_t* )pconn->proto.tcp->remote_ip),
	 pconn->proto.tcp->remote_port);*/

	if (pTcp->getType() == LIVE || pTcp->getType() == CLIENT)
		if (pTcp->_remote_port == pconn->proto.tcp->remote_port)
			if (memcmp(pTcp->_remote_ip.b, pconn->proto.tcp->remote_ip, 4) == 0)
				return true;
	if (pTcp->getType() == CLIENT && pTcp->_conn == pconn)
		return true;

	return false;
}

void Tcp::listTcp() {
	Tcp* cursor;
	char line[120];
	line[0] = 0;
	for (cursor = _first; cursor != 0; cursor = cursor->_next) {
		os_sprintf(line + strlen(line), "/ %X:%X %d %d", (uint32_t) cursor,
				cursor->_conn, cursor->getType(), cursor->isConnected());
	}
	LOGF(line);
}

Tcp* Tcp::findTcp(struct espconn* pconn) {
	Tcp* cursor;
	for (cursor = _first; cursor != 0; cursor = cursor->_next) {
		if (Tcp::match(pconn, cursor))
			return cursor;
	}
	return 0;
}

Tcp* Tcp::findFreeTcp(struct espconn* pconn) {
	Tcp* cursor;
	for (cursor = _first; cursor != 0; cursor = cursor->_next)
		if ((cursor->isConnected() == false) && (cursor->getType() == LIVE))
			return cursor;
	return 0;
}

void Tcp::loadEspconn(struct espconn* pconn) {
	_conn = pconn;
	if (pconn) {
		ets_sprintf(_host, "%d.%d.%d.%d", pconn->proto.tcp->remote_ip[0],
				pconn->proto.tcp->remote_ip[1], pconn->proto.tcp->remote_ip[2],
				pconn->proto.tcp->remote_ip[3]);
		_remote_port = pconn->proto.tcp->remote_port;
		os_memcpy(&_remote_ip, pconn->proto.tcp->remote_ip, 4);
	}
}

void Tcp::logConn(const char* s, void* arg) {
	struct espconn* pconn = (struct espconn*) arg;
	return;
	if (pconn) {
//	LOGF(" %X:%X %X", this, pconn, pconn->reverse);
		LOGF(" %s - tcp :  %x  , ip : %d.%d.%d.%d:%d  ",
				s, pconn->reverse, pconn->proto.tcp->remote_ip[0], pconn->proto.tcp->remote_ip[1], pconn->proto.tcp->remote_ip[2], pconn->proto.tcp->remote_ip[3], pconn->proto.tcp->remote_port);
	}
}

void Tcp::registerCb(struct espconn* pconn) {

	pconn->reverse = this;
	logConn(__FUNCTION__, pconn);
//	espconn_regist_time(pconn, 10, 1);		// disconnect after 1000 sec
	espconn_regist_connectcb(pconn, connectCb);
	espconn_regist_disconcb(pconn, disconnectCb);
	espconn_regist_recvcb(pconn, recvCb);
	espconn_regist_sentcb(pconn, sendCb);
	espconn_regist_write_finish(pconn, writeFinishCb);
}
/*
 Tcp* getInstance(void *arg) {
 struct espconn *pconn = (struct 	_wifi = wifi;espconn *) arg;
 Tcp* pTcp = (Tcp*) pconn->reverse;
 if (!Tcp::match(pconn, pTcp)) {
 LOGF(
 "================================== espconn and pTcp don't match ");
 pTcp = Tcp::findTcp(pconn);
 }
 //	ll.first();_wifi = wifi;
 return pTcp;
 }*/

uint8_t StrToIP(const char* str, void *ip);

Erc Tcp::write(Bytes& bytes) {
	return write(bytes.data(), bytes.length());
}

Erc Tcp::write(uint8_t* pb, uint32_t length) {
//	LOGF(" sending : %d bytes", length);
	logConn(__FUNCTION__, _conn);
	uint32_t i = 0;
	while (_txd.hasSpace() && (i < length)) {
		_txd.write(pb[i++]);
	};
	if (i < length) {
		_overflowTxd++;
		LOGF("TCP BUFFER OVERFLOW");
	}
	send();
	return E_OK;
}

Erc Tcp::write(uint8_t data) {
	if (_txd.hasSpace())
		_txd.write(data);
	else {
		_overflowTxd++;
	}
	send();
	return E_OK;
}

bool Tcp::hasData() { // not in  as it will be called in interrupt
	return false;
}

bool Tcp::hasSpace() {
	return _txd.hasSpace();
}

//	callkback when tcp connection is established
//_________________________________________________________

void Tcp::connectCb(void* arg) {

	struct espconn* pconn = (struct espconn*) arg;

	Tcp* pTcp = findTcp(pconn); // if found it's TcpClient

	if (pTcp == 0) {
		pTcp = findFreeTcp(pconn);
	}

	if (pTcp) {

		pTcp->loadEspconn(pconn);
		pTcp->logConn(__FUNCTION__, arg);
		pTcp->registerCb(pconn);
		pTcp->_sendState = READY;

		/*		uint32_t keeplive;

		 espconn_set_opt(pesp_conn, ESPCONN_KEEPALIVE); // enable TCP keep alive

		 //set keepalive: 75s = 60 + 5*3
		 keeplive = 60;
		 espconn_set_keepalive(pesp_conn, ESPCONN_KEEPIDLE, &keeplive);
		 keeplive = 5;
		 espconn_set_keepalive(pesp_conn, ESPCONN_KEEPINTVL, &keeplive);
		 keeplive = 3; //try times
		 espconn_set_keepalive(pesp_conn, ESPCONN_KEEPCNT, &keeplive);  */
//		espconn_set_opt(pconn, ESPCONN_NODELAY);
//		espconn_set_opt(pconn, ESPCONN_COPY);
//		espconn_set_keepalive(pconn, ESPCONN_KEEPALIVE, (void*) 1);
//		espconn_set_keepalive(pconn, ESPCONN_KEEPIDLE, (void*) 1);
//		espconn_regist_time(pconn, 100, 0);
		pTcp->self().tell(pTcp->self(), REPLY(CONNECT), 0);
		pTcp->_buffer.clear();
		pTcp->_txd.clear();
		pTcp->_connected = true;
		pTcp->_lastRxd = Sys::millis();
	} else {
		LOGF(" no free TCP : disconnecting %X ", pconn);
		LOGF("  tcp :  %x  , ip : %d.%d.%d.%d:%d  ",
				pconn->reverse, pconn->proto.tcp->remote_ip[0], pconn->proto.tcp->remote_ip[1], pconn->proto.tcp->remote_ip[2], pconn->proto.tcp->remote_ip[3], pconn->proto.tcp->remote_port);
		espconn_disconnect(pconn);
	}
	return;
}

void Tcp::reconnectCb(void* arg, int8_t err) {
	struct espconn* pconn = (struct espconn*) arg;
	Tcp *pTcp = findTcp(pconn);
	pTcp->logConn(__FUNCTION__, arg);
	pTcp->_sendState = READY;
	pTcp->_buffer.clear();
	pTcp->_txd.clear();
	return;

	LOGF("TCP: Reconnect %s:%d err : %d", pTcp->_host, pTcp->_remote_port, err);
	pTcp->right().tell(pTcp->self(), REPLY(DISCONNECT), 0);
	pTcp->_connected = false;
}

void Tcp::disconnectCb(void* arg) {
	struct espconn* pconn = (struct espconn*) arg;

	Tcp *pTcp = findTcp(pconn);
	if (pTcp) {
		pTcp->logConn(__FUNCTION__, arg);
		pTcp->_connected = false;
		if (pTcp->getType() == LIVE) {
			pTcp->loadEspconn(0);
		}
		pTcp->self().tell(pTcp->self(), REPLY(DISCONNECT), 0);
	} else
		LOGF("connection not found");
	return;
}

void Tcp::send() { // send buffered data, max 100 bytes

	if (_sendState == READY) {
		if (_buffer.length()) {
			// retry send same data
		} else {
			while (_txd.hasData() && _buffer.hasSpace(1)) {
				_buffer.write(_txd.read());
			}
		}
		if (_buffer.length() == 0)
			return;
		int8_t erc = espconn_sent(_conn, _buffer.data(), _buffer.length());
		if (erc) {
			LOGF(" _conn : %X espconn_sent() =  %d , length =%d",
					_conn, erc, _buffer.length());
			return;
		} else {
			_sendState = SENDING;
			_bytesTxd += _buffer.length();
			_buffer.clear();
		}
	}
}

void Tcp::writeFinishCb(void* arg) {
	struct espconn* pconn = (struct espconn*) arg;
	Tcp *pTcp = findTcp(pconn);
	pTcp->logConn(__FUNCTION__, arg);

	return;
}

void Tcp::sendCb(void *arg) {
	struct espconn* pconn = (struct espconn*) arg;
	Tcp *pTcp = findTcp(pconn);
	pTcp->logConn(__FUNCTION__, arg);
	pTcp->_sendState = READY;
	pTcp->send();
	pTcp->right().tell(pTcp->self(), REPLY(TXD), 0);
	return;
}

void Tcp::recvCb(void* arg, char *pdata, unsigned short len) {

	struct espconn* pconn = (struct espconn*) arg;
	Tcp *pTcp = findTcp(pconn);
	if (pTcp) {
		pTcp->_bytesRxd += len;
		pTcp->_lastRxd = Sys::millis();
		Bytes bytes;
		bytes.map((uint8_t*) pdata, (uint32_t) len);
//		LOGF(" received : %d bytes",len);
//		Str str(400);
//		bytes.toHex(str);
//		LOGF(" received : %s ",str.c_str());
		Erc erc;
		pTcp->right().tell(Header(pTcp->right(), pTcp->self(), RXD, 0),
				_cborOut.putf("B", &bytes));
//		if (erc = Msg::queue().putf("uuB", pTcp, SIG_RXD, &bytes)) {
//			pTcp->_overflowRxd++;
//		}
//		LOGF("erc %d bytes.length %d pTcp %X ", erc, bytes.length(), pTcp);
		pTcp->logConn(__FUNCTION__, arg);
		return;
	} else {
		LOGF(" Tcp not found ");
	}
}

void Tcp::dnsFoundCb(const char *name, ip_addr_t *ipaddr, void *arg) {
//	Tcp *pTcp = getInstance(pconn);
//	Tcp *pTcp = findTcp((struct espconn*) arg);
	struct espconn* pconn = (struct espconn*) arg;
	Tcp *pTcp = (Tcp*) (pconn->reverse);
	pTcp->logConn(__FUNCTION__, arg);

	if (ipaddr == NULL) {
		LOGF("DNS: Found, but got no ip, try to reconnect");
		return;
	};
	LOGF("DNS: found ip %d.%d.%d.%d",
			*((uint8 * ) &ipaddr->addr), *((uint8 * ) &ipaddr->addr + 1), *((uint8 * ) &ipaddr->addr + 2), *((uint8 * ) &ipaddr->addr + 3));

//	if (pTcp->_remote_ip.addr == 0 && ipaddr->addr != 0) {
	memcpy(pTcp->_conn->proto.tcp->remote_ip, &ipaddr->addr, 4);
	memcpy(pTcp->_remote_ip.b, &ipaddr->addr, 4);
	espconn_connect((pTcp->_conn));
	LOGF("TCP: connecting...");
//	}
}

void Tcp::disconnect() {
	espconn_disconnect(_conn);
	_connected = false;
}
/*
 *
 */bool Tcp::isConnected() {
	return _connected;
}
//____________________________________________________________
//
//
//____________________________________________________________
//
void Tcp::onReceive(Header hdr, Cbor& cbor) {
	PT_BEGIN()
	;
	WIFI_DISCONNECTED: {
		while (true) {
			PT_YIELD_UNTIL(hdr.is(left(), self(), REPLY(CONNECT), ANY));
//			LOGF("Tcp started. %x", this);
			goto CONNECTING;
		}
	}
	CONNECTING: {
		LOGF("CONNECTING");
		while (true) {
			PT_YIELD();
			if (hdr.is(self(), self(), REPLY(CONNECT), 0)) {
				right().tell(self(), REPLY(CONNECT), 0);
				_sendState=READY;
				goto CONNECTED;
			} else if (hdr.is(self(), self(), REPLY(DISCONNECT), 0)) {
				right().tell(self(), REPLY(DISCONNECT), 0);
				goto WIFI_DISCONNECTED;
			}
		}
	}
	CONNECTED: {
		LOGF("CONNECTED");
		while (true) {
			setReceiveTimeout(5000);
			PT_YIELD();
			if (hdr.is(self(), REPLY(DISCONNECT))) { // tcp link gone, callback was called
				right().tell(self(), REPLY(DISCONNECT), 0);
				goto CONNECTING;
			} else if (hdr.is(left(), REPLY(DISCONNECT))) { // wifi link gone
				right().tell(self(), REPLY(DISCONNECT), 0);
				goto WIFI_DISCONNECTED;
			} else if (hdr.is(right(), DISCONNECT)) { // wifi link gone
				disconnect();
				goto CONNECTING;
			} else if (timeout()) {
				LOGF("%d:%d %d", _lastRxd + 5000, Sys::millis());
				if ((_lastRxd + 5000) < Sys::millis()) {
					LOGF(" timeout - disconnect %X:%X", this, _conn);
					disconnect();
				}
			}
		}
	}
	;
PT_END()
}

//____________________________________________________________
//			Tcp::connect
//____________________________________________________________
void TcpClient::connect() {

//LOGF(" Connecting ");
//	ets_memset(_conn, 0, sizeof(_conn));
_conn->type = ESPCONN_TCP;
_conn->state = ESPCONN_NONE;
//	_conn->proto.tcp = (esp_tcp *) malloc(sizeof(esp_tcp));
ets_memset(_conn->proto.tcp, 0, sizeof(esp_tcp));
_conn->proto.tcp->local_port = espconn_port();
_conn->proto.tcp->remote_port = _remote_port;
_conn->reverse = this;
registerCb(_conn);

if (StrToIP(_host, _conn->proto.tcp->remote_ip)) {
	LOGF("TCP: Connect to ip  %s:%d", _host, _remote_port);
	espconn_connect(_conn);
} else {
	LOGF("TCP: Connect to domain %s:%d", _host, _remote_port);
	espconn_gethostbyname(_conn, _host, &_remote_ip.ipAddr, Tcp::dnsFoundCb);
}

}

ActorRef TcpClient::create(const char* host, uint16_t port) {
return ActorRef(new TcpClient(host, port));
}

TcpClient::TcpClient(const char* host, uint16_t port) :
	Tcp() {
LOGF("this : %X ", this);
strncpy(_host, host, sizeof(_host));
_remote_port = port;
logConn(__FUNCTION__, _conn);
setType(CLIENT);
_conn = new (struct espconn);
_remote_ip.addr = 0; // should be zero for dns callback to work
_connected = false;
ets_memset(_conn, 0, sizeof(_conn));
_conn->type = ESPCONN_TCP;
_conn->state = ESPCONN_NONE;
_conn->proto.tcp = (esp_tcp *) malloc(sizeof(esp_tcp));
ets_memset(_conn->proto.tcp, 0, sizeof(esp_tcp));
_connected = true; // don't reallocate master client
}

void TcpClient::onReceive(Header hdr, Cbor& cbor) {
PT_BEGIN()
WIFI_DISCONNECTED: {
	while (true) {
		PT_YIELD();
		if (hdr.is(self(), left(), REPLY(CONNECT), 0)) {
			LOGF("TcpClient started. %x", this);
			goto CONNECTING;
		}
	}
}
CONNECTING: {
	while (true) {
		connect();
		PT_YIELD();
		if (hdr.is(self(), self(), REPLY(CONNECT), 0)) {
			right().tell(self(), REPLY(CONNECT), 0);
			goto CONNECTED;
		} else if (hdr.is(self(), self(), REPLY(DISCONNECT), 0)) {
			right().tell(self(), REPLY(DISCONNECT), 0);
			goto WIFI_DISCONNECTED;
		}

	}
}
CONNECTED: {
	while (true) {
		setReceiveTimeout(5000);
		PT_YIELD();
		if (hdr.is(self(), self(), REPLY(DISCONNECT), 0)) { // tcp link gone, callback was called
			right().tell(self(), REPLY(DISCONNECT), 0);
			goto CONNECTING;
		} else if (hdr.is(left(), self(), REPLY(DISCONNECT), 0)) { // wifi link gone
			right().tell(self(), REPLY(DISCONNECT), 0);
			goto WIFI_DISCONNECTED;
		} else if (hdr.is(TXD)) {
//			LOGF(" cbor length : %d offset : %d", cbor.length(), cbor.offset())
			Bytes data(256);
			cbor.get(data);
			write(data);
		} else if (hdr.is(TIMEOUT)) {
			//			LOGF("%d:%d %d", _lastRxd + 5000, Sys::millis());
			/*if ((_lastRxd + 5000) < Sys::millis()) {
			 LOGF(" timeout - disconnect %X:%X", this, _conn);
			 disconnect();
			 }
			 */
		} else if (hdr.is(DISCONNECT)) {
			disconnect();
		}
	}
}
PT_END()
}

TcpServer::TcpServer(uint16_t port) :
Tcp() {
LOGF("this : %X ", this);
_local_port = port;
_conn->proto.tcp->local_port = _local_port;
setType(SERVER);
_conn = new (struct espconn);
ets_memset(_conn, 0, sizeof(_conn));
_conn->type = ESPCONN_TCP;
_conn->state = ESPCONN_NONE;
_conn->proto.tcp = (esp_tcp *) malloc(sizeof(esp_tcp));
ets_memset(_conn->proto.tcp, 0, sizeof(esp_tcp));
_local_port = 23;
_conn->reverse = (Tcp*) this;
logConn(__FUNCTION__, _conn);
_connected = true; // to allocate TCP instance
}

//------------------------------------------------------------------------
void TcpServer::listen() {
if (espconn_accept(_conn) != ESPCONN_OK) {
LOGF("ERR : espconn_accept");
}
registerCb(_conn);
logConn(__FUNCTION__, _conn);
}
//------------------------------------------------------------------------
void TcpServer::onReceive(Header hdr, Cbor& cbor) {
PT_BEGIN()
;
WIFI_DISCONNECT: {
while (true) {
	PT_YIELD();
	if (hdr.is(self(), left(), REPLY(CONNECT), 0)) {
		LOGF("TcpServer started. %x", this);
		listen();
		goto CONNECTING;
	}
}
}
CONNECTING: {
while (true) {

	setReceiveTimeout(2000);
	PT_YIELD();
	if (hdr.is(self(), left(), REPLY(CONNECT), 0)) {
		listTcp();
		goto WIFI_DISCONNECT;
	}
}
}
PT_END()
}
//-----------------------------------------------------
/*
 TcpClient::TcpClient(const char* host, uint16_t port) :
 Tcp() {

 strncpy(_host, host, sizeof(_host));
 _remote_port = port;
 LOGF("this : %X ", this);
 setType(CLIENT);
 _conn = new (struct espconn);
 strcpy(_host, "");
 _remote_port = 80;
 _remote_ip.addr = 0; // should be zero for dns callback to work
 _connected = false;
 ets_memset(_conn, 0, sizeof(_conn));
 _conn->type = ESPCONN_TCP;
 _conn->state = ESPCONN_NONE;
 _conn->proto.tcp = (esp_tcp *) malloc(sizeof(esp_tcp));
 ets_memset(_conn->proto.tcp, 0, sizeof(esp_tcp));
 _connected = true; // don't reallocate master client
 }
 */

uint8_t StrToIP(const char* str, void *ip) {
LOGF(" convert  Ip address : %s", str);
int i; // The count of the number of bytes processed.
const char * start; // A pointer to the next digit to process.
start = str;

for (i = 0; i < 4; i++) {
char c; /* The digit being processed. */
int n = 0; /* The value of this byte. */
while (1) {
c = *start;
start++;
if (c >= '0' && c <= '9') {
	n *= 10;
	n += c - '0';
}
/* We insist on stopping at "." if we are still parsing
 the first, second, or third numbers. If we have reached
 the end of the numbers, we will allow any character. */
else if ((i < 3 && c == '.') || i == 3) {
	break;
} else {
	return 0;
}
}
if (n >= 256) {
return 0;
}
((uint8_t*) ip)[i] = n;
}
return 1;
}

