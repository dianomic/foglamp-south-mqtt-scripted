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
static const char *default_config = QUOTE({
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
	"username" : {
		"description" : "The username to use for the connection if any",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Username",
	       	"order" : "3"
		},
	"password" : {
		"description" : "The password for the user if using authentication",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Password",
	       	"order" : "4",
		"validity" : "username != \"\""
		},
	"serverCert" : {
		"description" : "The name of the server certificate to be trusted. This should correspond to a PEM file stored in the FogLAMP certificate store",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Trusted Certificate",
	       	"order" : "5"
		},
	"clientCert": {
		"description" : "The certificate that will be used by the plugin to connect to the broker. This should correspond to a PEM file stored in the FogLAMP certificate store",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Client Certificate",
	       	"order" : "6",
		"validity" : "serverCert != \"\""
		},
	"key" : {
		"description" : "The private key used by the client to create the client certificate. This may be left blank if it is included in the PEM file of the client certificate.",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Private Key",
	       	"order" : "7",
		"validity" : "clientCert != \"\""
		},
	"keyPass" : {
		"description" : "The password used to encrypte the private key. This may be left blank if the private key is not encrypted.",
		"type" : "password",
	       	"default" : "",
		"displayName" : "Key Password",
	       	"order" : "8",
		"validity" : "key != \"\""
		},
	"topic" : {
		"description" : "The MQTT topic to which we subscribe to receive sensor messages",
		"type" : "string",
		"default" : "sensor",
		"order" : "9",
		"displayName": "Topic",
		"mandatory": "true"
		},
	"policy" : {
		"description" : "The policy to choose when dealign with nested objects in the response payload or the response form the script.",
		"type" : "enumeration",
		"options" : [ "Single reading from root level", "Single reading & collapse",
				"Single reading & nest", "Multiple readings & collapse",
				"Multiple readings & nest" ],
		"default" : "Single reading from root level",
		"order" : "10",
		"displayName": "Object Policy",
		"mandatory": "true"
		},
	"timestamp" : {
		"description" : "The name of a property that should be used as a timestamp. If left blank then the payload is assumed not to have a tiemstamp and readings are timestamped with the current date and time",
		"type" : "string",
		"default" : "",
		"order" : "11",
		"displayName": "Timestamp"
		},
	"format" : {
       		"description" : "The format of timestamps to pass if using the time based data selection method and also the format of timestamps in the payload",
		"type" : "string",
	       	"default" : "",
		"displayName" : "Time Format",
	       	"order" : "13",
	       	"mandatory": "false",
		"validity": "timestamp != \"\""
		},
	"timezone" : {
       		"description" : "The default timezone to use if none is specific. Timezones should be expressed as time offsets",
		"type" : "string",
	       	"default" : "+00:00",
		"displayName" : "Timezone",
	       	"order" : "12",
	       	"mandatory": "false",
		"validity": "timestamp != \"\""
		},
	"script" : {
		"description" : "MQTT message translation Python script",
		"type" : "script",
		"default" : "",
		"order" : "14",
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
