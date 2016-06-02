/*
 * Config.h
 *
 *  Created on: 1-jun.-2016
 *      Author: 600104947
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <Actor.h>
/*
 * Config waits on requests for config but can also send out config changes
 * CONFIG requests : send all params to sender which are known
 * CONFIG store and retrieval
 * CONFIG events for reconfig, find path by Actor names , example "mqtt/host":"test.mosquitto.org"
 * data in config can be stored in cbor format
 * CONFIG can also be published on a regular basis
 *
 * IN : CONFIG,RXD : REPLY(CONFIG),
 * IN :
 *
 */
class Config : public Actor {
public:
	Config() : Actor("Config"){
		;
	}
	static Config* _configServer;

public:
	virtual ~Config();
	static ActorRef create() {
		return ActorRef(new Config());
	}
	void init();
	void onReceive(Header hdr, Cbor& data);
	static ActorRef getConfigServer() {
		return ActorRef(_configServer);
	}
};

#endif /* CONFIG_H_ */
