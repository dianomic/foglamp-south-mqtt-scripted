/*
 * Fledge south plugin.
 *
 * Copyright (c) 2020 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <scripted.h>
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>

typedef void (*INGEST_CB)(void *, Reading);

using namespace std;

#define PLUGIN_NAME	"mqtt-scripted"

/**
 * Default configuration
 */
const char *default_config = QUOTE({
	"plugin" : {
       		"description" : "An MQTT plugin that supports Python script to convert the message payload",
		"type" : "string",
	       	"default" : PLUGIN_NAME,
		"readonly" : "true"
		},
	"asset" : {
       		"description" : "Asset name",
		"type" : "string",
	       	"default" : "mqtt",
		"displayName" : "Asset Name",
	       	"order" : "1",
	       	"mandatory": "true"
	       	},
	"broker" : {
       		"description" : "The address of the MQTT broker",
		"type" : "string",
	       	"default" : "localhost",
		"displayName" : "MQTT Broker",
	       	"order" : "2",
	       	"mandatory": "true"
	       	},
	"topic" : {
		"description" : "The MQTT topic to which we subscribe to receive sensor messages",
		"type" : "string",
		"default" : "sensor",
		"order": "3",
		"displayName": "Topic",
		"mandatory": "true"
		},
	"script" : {
		"description" : "MQTT message translation Python script",
		"type" : "script",
		"default" : "",
		"order": "4",
		"displayName": "Script"
		} 
	});

/**
 * The MQTTScripted plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
	PLUGIN_NAME,              // Name
	VERSION,                  // Version
	SP_ASYNC, 		  // Flags
	PLUGIN_TYPE_SOUTH,        // Type
	"1.0.0",                  // Interface version
	default_config		  // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	Logger::getLogger()->info("MQTTScripted Config is %s", info.config);
	return &info;
}

/**
 * Initialise the plugin, called to get the plugin handle
 */
PLUGIN_HANDLE plugin_init(ConfigCategory *config)
{
MQTTScripted	*mqtt;


	mqtt = new MQTTScripted(config);
	return (PLUGIN_HANDLE)mqtt;
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle)
{
MQTTScripted *mqtt = (MQTTScripted *)handle;


	if (!handle)
		return;
	mqtt->start();
}

/**
 * Register ingest callback
 */
void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data)
{
MQTTScripted *mqtt = (MQTTScripted *)handle;

	if (!handle)
		throw new exception();
	mqtt->registerIngest(data, cb);
}

/**
 * Poll for a plugin reading
 */
Reading plugin_poll(PLUGIN_HANDLE *handle)
{
MQTTScripted *mqtt = (MQTTScripted *)handle;

	throw runtime_error("MQTTScripted is an async plugin, poll should not be called");
}

/**
 * Reconfigure the plugin
 *
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string& newConfig)
{
MQTTScripted		*mqtt = (MQTTScripted *)*handle;
ConfigCategory	config(mqtt->getName(), newConfig);

	mqtt->reconfigure(config);
}

/**
 * Shutdown the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
MQTTScripted *mqtt = (MQTTScripted *)handle;

	mqtt->stop();
	delete mqtt;
}
};
