/*
 * Fledge "MQTTScripted" scripted south plugin.
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch           
 */
#include "scripted.h"
#include <logger.h>
#include <rapidjson/document.h>
#include "MQTTClient.h"

#define TIMEOUT     10000L

using namespace std;
using namespace rapidjson;

/**
 * Callback when an MQTT message arrives for the topic to which we are subscribed
 */
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
int i;
char *payloadptr;

	payloadptr = (char *)message->payload;
	char *buf = (char *)malloc(message->payloadlen + 1);
	for(i = 0; i < message->payloadlen; i++)
	{
        	buf[i] = (*payloadptr++);
	}
	buf[message->payloadlen] = 0;
	MQTTClient_freeMessage(&message);
	MQTTScripted *mqtt = (MQTTScripted *)context;
	mqtt->processMessage(topicName, buf);
	MQTTClient_free(topicName);
	free(buf);
	return 1;
}

/**
 * Callback for when the connection to the MQTT server fails
 */
void connlost(void *context, char *cause)
{
	MQTTScripted *mqtt = (MQTTScripted *)context;
	mqtt->reconnect();
}

/**
 * Construct an MQTT Scripted south plugin
 *
 * @param config	The configuration category
 */
MQTTScripted::MQTTScripted(ConfigCategory *config) : m_python(NULL), m_restart(false)
{
	m_name = config->getName();
	m_logger = Logger::getLogger();
	m_asset = config->getValue("asset");
	m_broker = config->getValue("broker");
	m_topic = config->getValue("topic");
	m_script = config->getItemAttribute("script", ConfigCategory::FILE_ATTR);
	m_content = config->getValue("script");
	m_clientID = config->getName();
	m_qos = 1;
	m_python = new PythonScript(m_name);
	if (m_python && m_script.empty() == false && m_content.empty() == false)
	{
		m_python->setScript(m_script);
	}
}

/**
 * The destructure for the MQTTScripted plugin
 */
MQTTScripted::~MQTTScripted()
{
	if (m_python)
	{
		delete m_python;
	}
}

/**
 * Called when the plugin is started
 * 
 * This will connect to the MQTT broker and subscribe to the
 * requested topic.
 */
bool MQTTScripted::start()
{
	lock_guard<mutex> guard(m_mutex);


	// Connect to the MQTT broker
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_deliveryToken token;
	int rc;

	if ((rc = MQTTClient_create(&m_client, m_broker.c_str(), m_clientID.c_str(),
		MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to create client, return code %d\n", rc);
		return false;
	}

	MQTTClient_setCallbacks(m_client, this, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to connect, return code %d\n", rc);
		return false;
	}

	// Subscribe to the topic
	if ((rc = MQTTClient_subscribe(m_client, m_topic.c_str(), m_qos)) != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to subscribe to topic, return code %d\n", rc);
		return false;
	}
	Logger::getLogger()->info("Subscribed to topic '%s'", m_topic.c_str());
	return true;
}

/**
 * Called on shutdown of the south service. Cleans up the Python runtime
 * and closes the connection to the MQTT broker.
 */
void MQTTScripted::stop()
{
int rc;

	if ((rc = MQTTClient_disconnect(m_client, 10000)) != MQTTCLIENT_SUCCESS)
		Logger::getLogger()->error("Failed to disconnect, return code %d\n", rc);
	MQTTClient_destroy(&m_client);
	return;
}

/**
 * Reconnect to the MQTT broker on connection failure
 */
void MQTTScripted::reconnect()
{
int rc;

	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to connect, return code %d\n", rc);
		return;
	}

	// Subscribe to the topic
	if ((rc = MQTTClient_subscribe(m_client, m_topic.c_str(), m_qos)) != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to subscribe to topic, return code %d\n", rc);
		return;
	}
}

/**
 * Reconfigure the MQTTScripted delivery plugin
 *
 * @param newConfig	The new configuration
 */
void MQTTScripted::reconfigure(const ConfigCategory& category)
{
	lock_guard<mutex> guard(m_mutex);
	m_asset = category.getValue("asset");
	string broker = category.getValue("broker");
	bool resubscribe = false;
	if (broker.compare(m_broker))
	{
		resubscribe = true;
	}
	m_broker = broker;
	string topic = category.getValue("topic");
	if (topic.compare(m_topic))
	{
		resubscribe = true;
	}
	m_topic = topic;

	if (resubscribe)
	{
		Logger::getLogger()->info("Resubscribing to MQTT broker followign reconfiguration");
		// The MQTT broker has changed
		(void)MQTTClient_disconnect(m_client, 10000);
		MQTTClient_destroy(&m_client);


		// Connect to the new MQTT broker
		MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
		MQTTClient_deliveryToken token;
		int rc;

		if ((rc = MQTTClient_create(&m_client, m_broker.c_str(), m_clientID.c_str(),
			MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
		{
			Logger::getLogger()->error("Failed to create client, return code %d\n", rc);
		}
		else
		{
			MQTTClient_setCallbacks(m_client, this, connlost, msgarrvd, NULL);
			reconnect();
		}
	}

	m_script = category.getItemAttribute("script", ConfigCategory::FILE_ATTR);
	string content = category.getValue("script");
	if (m_content.compare(content))	// Script content has changed
	{
		Logger::getLogger()->info("Reconfiguration has changed the Python script");
		m_restart = true;
		m_content = content;
	}
}

/**
 * Called when a message is delivered from the MQTT broker
 *
 * @param topic	The MQTT topic
 * @param message	The MQTT message
 */
void MQTTScripted::processMessage(const string& topic,const  string& message)
{
Document doc;

	Logger::getLogger()->debug("Processing MQTT message: %s with script %s", message.c_str(), m_script.c_str());
	if (m_script.empty() || m_script.compare("\"\"") == 0)
	{
		// Message should be JSON
		doc.Parse(message.c_str());
		if (doc.HasParseError() == false && doc.IsObject())
		{
			Logger::getLogger()->debug("Message is JSON");
			vector<Datapoint *> points;
			// Iterate the document
			for (auto& m : doc.GetObject())
			{
				if (m.value.IsInt64())
				{
					long v = m.value.GetInt64();
					DatapointValue dpv(v);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
				else if (m.value.IsDouble())
				{
					double d = m.value.GetDouble();
					DatapointValue dpv(d);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
				else if (m.value.IsString())
				{
					const char *s = m.value.GetString();
					DatapointValue dpv(s);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
			}
			Reading reading(m_asset, points);
			(*m_ingest)(m_data, reading);
		}
		else
		{
			Logger::getLogger()->debug("Message is assumed to be simple value");
			// Maybe it is a simple value
			int i;
			bool nonNumeric = false;
			for (i = 0; i < message.length(); i++)
				if (message[i] != '.' && (message[i] < '0' || message[i] > '9') && message[i] != '-')
					nonNumeric = true;
			if (!nonNumeric)
			{
				double d = strtod(message.c_str(), NULL);
				DatapointValue dpv(d);
				Reading reading(m_asset, new Datapoint(m_topic, dpv));
				(*m_ingest)(m_data, reading);
			}
			else
			{
				Logger::getLogger()->warn("Unable to process simple value: '%s'",
						message.c_str());
			}
		}
	}
	else
	{
		string asset;

		if (m_restart)
		{
			Logger::getLogger()->info("Script content has changed, reloading");
			delete m_python;
			m_python = new PythonScript(m_name);
			if (m_python && m_script.empty() == false)
			{
				m_python->setScript(m_script);
			}
			m_restart = false;
		}
		// Give the message to the script to process
		Document *d = m_python->execute(message, topic, asset);

		if (d)
		{
			vector<Datapoint *> points;
			// Iterate the document
			for (auto& m : d->GetObject())
			{
				if (m.value.IsInt64())
				{
					long v = m.value.GetInt64();
					DatapointValue dpv(v);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
				else if (m.value.IsDouble())
				{
					double d = m.value.GetDouble();
					DatapointValue dpv(d);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
				else if (m.value.IsString())
				{
					const char *s = m.value.GetString();
					DatapointValue dpv(s);
					points.push_back(new Datapoint( m.name.GetString(), dpv));
				}
			}
			if (asset.empty()) {

				asset = m_asset;
			}

			Logger::getLogger()->debug("%s - message :%s: topic :%s: asset :%s: ", __FUNCTION__ , message.c_str(), topic.c_str(), asset.c_str() );
			Logger::getLogger()->debug("");
			Reading reading(asset, points);
			(*m_ingest)(m_data, reading);

			delete d;
		}
	}
}
