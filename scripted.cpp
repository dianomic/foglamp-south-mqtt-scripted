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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

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
 * Callaback when an SSL error occurs
 */
int  sslErrorCallback(const char *str, size_t len, void *context)
{
	MQTTScripted *mqtt = (MQTTScripted *)context;
	mqtt->sslError(str, len);
	return 0;
}

/**
 * Construct an MQTT Scripted south plugin
 *
 * @param config	The configuration category
 */
MQTTScripted::MQTTScripted(ConfigCategory *config) : m_python(NULL), m_restart(false), m_state(mFailed)
{
	m_name = config->getName();
	m_logger = Logger::getLogger();
	m_asset = config->getValue("asset");
	m_broker = config->getValue("broker");
	m_topic = config->getValue("topic");
	m_clientCert = config->getValue("clientCert");
	m_key = config->getValue("key");
	m_keyPass = config->getValue("keyPass");
	m_serverCert = config->getValue("serverCert");
	m_username = config->getValue("username");
	m_password = config->getValue("password");
	string policy = config->getValue("policy");
	processPolicy(policy);
	m_timestamp = config->getValue("timestamp");
	m_timeFormat = config->getValue("format");
	string timezone = config->getValue("timezone");
	m_offset = strtol(timezone.c_str(), NULL, 10);
	m_offset *= 60 * 60;
	long num;
	auto res = timezone.find_first_of(':');
	if (res |= string::npos)
	{
		string mins = timezone.substr(res + 1);
		num = strtol(mins.c_str(), NULL, 10);
		num *= 60;
		m_offset += num;
	}
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
	lock_guard<mutex> guard(m_mutex);

	if (m_python)
	{
		delete m_python;
	}
}

/**
 * Process the policy string
 *
 * @param policy	The required policy
 */
void MQTTScripted::processPolicy(const string& policy)
{
	if (policy.compare("Single reading from root level") == 0)
	{
		m_policy = mPolicyFirstLevel;
		m_nest = false;
	}
	else if (policy.compare("Single reading & collapse") == 0)
	{
		m_policy = mPolicyCollapse;
		m_nest = false;
	}
	else if (policy.compare("Single reading & nest") == 0)
	{
		m_policy = mPolicyCollapse;
		m_nest = true;
	}
	else if (policy.compare("Multiple readings & collapse") == 0)
	{
		m_policy = mPolicyMultiple;
		m_nest = false;
	}
	else if (policy.compare("Multiple readings & nest") == 0)
	{
		m_policy = mPolicyMultiple;
		m_nest = true;
	}
	else
	{
		Logger::getLogger()->error("Unsupported value for policy configuration '%s'", policy.c_str());
	}
}

void traceCallback(enum MQTTCLIENT_TRACE_LEVELS level, char* message)
{
	Logger::getLogger()->debug("Trace: %s", message);
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
		m_state = mFailed;
		return false;
	}
	m_state = mCreated;

	MQTTClient_setTraceCallback(traceCallback);
	MQTTClient_setTraceLevel(MQTTCLIENT_TRACE_MAXIMUM);

	MQTTClient_setCallbacks(m_client, this, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	if (m_username.length())
	{
		conn_opts.username = m_username.c_str();
		conn_opts.password = m_password.c_str();
	}

	// Do we need MQTTS support
	MQTTClient_SSLOptions sslopts = MQTTClient_SSLOptions_initializer;
	if (m_serverCert.length())
	{
		string serverCert = serverCertPath();
		sslopts.trustStore = strdup(serverCert.c_str());
		string clientCert = clientCertPath();
		sslopts.keyStore = strdup(clientCert.c_str());
		if (m_key.length())
		{
			string privateKey = privateKeyPath();
			sslopts.privateKey = strdup(privateKey.c_str());
		}
		if (m_keyPass.length())
		{
			sslopts.privateKeyPassword = m_keyPass.c_str();
		}

		sslopts.ssl_error_cb = sslErrorCallback;
		sslopts.ssl_error_context = this;

		sslopts.enableServerCertAuth = false;
		sslopts.verify = false;

		conn_opts.ssl = &sslopts;
		m_logger->info("Trust store: %s", sslopts.trustStore);
		m_logger->info("Key store: %s", sslopts.keyStore);
		m_logger->info("Private key: %s", sslopts.privateKey);
	}
	rc = MQTTClient_connect(m_client, &conn_opts);
	if (sslopts.trustStore)
		free((void *)sslopts.trustStore);
	if (sslopts.keyStore)
		free((void *)sslopts.keyStore);
	if (sslopts.privateKey)
		free((void *)sslopts.privateKey);
	if (rc != MQTTCLIENT_SUCCESS)
	{
		Logger::getLogger()->error("Failed to connect, return code %d\n", rc);
		return false;
	}
	m_state = mConnected;

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
	lock_guard<mutex> guard(m_mutex);
int rc;

	if (m_state == mConnected)
	{
		if ((rc = MQTTClient_disconnect(m_client, 10000)) != MQTTCLIENT_SUCCESS)
			Logger::getLogger()->error("Failed to disconnect, return code %d\n", rc);
	}
	if (m_state == mConnected || m_state == mCreated)
	{
		MQTTClient_destroy(&m_client);
	}
	m_state = mFailed;
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

	if (m_username.length())
	{
		conn_opts.username = m_username.c_str();
		conn_opts.password = m_password.c_str();
	}

	// Do we need MQTTS support
	MQTTClient_SSLOptions sslopts = MQTTClient_SSLOptions_initializer;
	if (m_serverCert.length())
	{
		string serverCert = serverCertPath();
		sslopts.trustStore = strdup(serverCert.c_str());
		string clientCert = clientCertPath();
		sslopts.keyStore = strdup(clientCert.c_str());
		if (m_key.length())
		{
			string privateKey = privateKeyPath();
			sslopts.privateKey = strdup(privateKey.c_str());
		}
		if (m_keyPass.length())
			sslopts.privateKeyPassword = m_keyPass.c_str();

		sslopts.ssl_error_cb = sslErrorCallback;
		sslopts.ssl_error_context = this;

		sslopts.enableServerCertAuth = true;
		sslopts.verify = true;

		m_logger->info("Trust store: %s", sslopts.trustStore);
		m_logger->info("Key store: %s", sslopts.keyStore);
		m_logger->info("Private key: %s", sslopts.privateKey);

		conn_opts.ssl = &sslopts;
	}
	rc = MQTTClient_connect(m_client, &conn_opts);
	if (sslopts.trustStore)
		free((void *)sslopts.trustStore);
	if (sslopts.keyStore)
		free((void *)sslopts.keyStore);
	if (sslopts.privateKey)
		free((void *)sslopts.privateKey);
	if (rc != MQTTCLIENT_SUCCESS)
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

	string key = category.getValue("key");
	if (key.compare(m_key))
	{
		resubscribe = true;
	}
	m_key = key;

	string clientCert = category.getValue("clientCert");
	if (clientCert.compare(m_clientCert))
	{
		resubscribe = true;
	}
	m_clientCert = clientCert;

	string keyPass = category.getValue("keyPass");
	if (keyPass.compare(m_keyPass))
	{
		resubscribe = true;
	}
	m_keyPass = keyPass;

	string serverCert = category.getValue("serverCert");
	if (serverCert.compare(m_serverCert))
	{
		resubscribe = true;
	}
	m_serverCert = serverCert;

	string username = category.getValue("username");
	if (username.compare(m_username))
	{
		resubscribe = true;
	}
	m_username = username;

	string password = category.getValue("password");
	if (password.compare(m_password))
	{
		resubscribe = true;
	}
	m_password = password;

	string policy = category.getValue("policy");
	processPolicy(policy);

	m_timestamp = category.getValue("timestamp");
	m_timeFormat = category.getValue("format");

	string timezone = category.getValue("timezone");
	m_offset = strtol(timezone.c_str(), NULL, 10);
	m_offset *= 60 * 60;
	long num;
	auto res = timezone.find_first_of(':');
	if (res |= string::npos)
	{
		string mins = timezone.substr(res + 1);
		num = strtol(mins.c_str(), NULL, 10);
		num *= 60;
		m_offset += num;
	}

	if (resubscribe)
	{
		Logger::getLogger()->info("Resubscribing to MQTT broker following reconfiguration");
		// The MQTT broker has changed
		(void)MQTTClient_disconnect(m_client, 10000);
		MQTTClient_destroy(&m_client);


		// Connect to the new MQTT broker
		MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
		MQTTClient_deliveryToken token;
		int rc;

		Logger::getLogger()->debug("Create MQTT Client '%s' with clientID '%s'", m_broker.c_str(), m_clientID.c_str());
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

	lock_guard<mutex> guard(m_mutex);

	Logger::getLogger()->debug("Processing MQTT message: %s with script %s", message.c_str(), m_script.c_str());
	if (m_script.empty() || m_script.compare("\"\"") == 0)
	{
		// Message should be JSON
		doc.Parse(message.c_str());
		if (doc.HasParseError() == false && doc.IsObject())
		{
			Logger::getLogger()->debug("Message is JSON");
			processDocument(doc, m_asset);
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
			if (asset.empty()) {

				asset = m_asset;
			}
			processDocument(*d, asset);

			Logger::getLogger()->debug("%s - message :%s: topic :%s: asset :%s: ", __FUNCTION__ , message.c_str(), topic.c_str(), asset.c_str() );

			delete d;
		}
	}
}

/**
 * Return the directory where pem fiels are stored
 */
string MQTTScripted::pemPath()
{
	if (getenv("FOGLAMP_DATA"))
	{
		m_pemPath = getenv("FOGLAMP_DATA");
		m_pemPath += "/etc/certs/";
	}
	else if (getenv("FOGLAMP_ROOT"))
	{
		m_pemPath = getenv("FOGLAMP_ROOT");
		m_pemPath += "/data/etc/certs/";
	}
	else
	{
		m_pemPath = "/usr/local/foglamp/data/etc/certs/"; 
	}

	string pemPath = m_pemPath;
	pemPath += "pem/";
	struct stat statb;
	if (stat(pemPath.c_str(), &statb) == 0 && (statb.st_mode & S_IFMT) == S_IFDIR)
		m_pemPath += "pem/";
	return m_pemPath;
}

/**
 * Return the path to the private ket for this connection
 */
string MQTTScripted::privateKeyPath()
{
	m_keyPath = pemPath();
	m_keyPath += m_key;
	m_keyPath += ".pem";

	if (access(m_keyPath.c_str(), R_OK))
	{
		m_logger->error("Unable to access certificate %s", m_keyPath.c_str());
	}

	return m_keyPath;
}

/**
 * Return the path to the server certificate
 */
string MQTTScripted::clientCertPath()
{
	m_clientCertPath = pemPath();
	m_clientCertPath += m_clientCert;
	m_clientCertPath += ".pem";

	if (access(m_clientCertPath.c_str(), R_OK))
	{
		m_logger->error("Unable to access certificate %s", m_clientCertPath.c_str());
	}

	return m_clientCertPath;
}

/**
 * Return the path to the server certificate
 */
string MQTTScripted::serverCertPath()
{
	m_serverCertPath = pemPath();
	m_serverCertPath += m_serverCert;
	m_serverCertPath += ".pem";

	if (access(m_serverCertPath.c_str(), R_OK))
	{
		m_logger->error("Unable to access certificate %s", m_serverCertPath.c_str());
	}

	return m_serverCertPath;
}

/**
 * Process the JSON document following the rules regarding collapsing and creating
 * multiple readings.
 *
 * @param doc	The JSON document to process into readings
 */
void MQTTScripted::processDocument(Document& doc, const string& asset)
{
	if (m_policy == mPolicyFirstLevel)
	{
		Logger::getLogger()->debug("Policy is to take data from the first level only");
		vector<Datapoint *> points;
		string ts;
		getValues(doc.GetObject(), points, false, ts);
		Reading reading(asset, points);
		if (!ts.empty())
			reading.setUserTimestamp(ts);
		(*m_ingest)(m_data, reading);
	}
	else if (m_policy == mPolicyCollapse)
	{
		Logger::getLogger()->debug("Policy is to collapse data into a single reading");
		vector<Datapoint *> points;
		string ts;
		getValues(doc.GetObject(), points, true, ts);
		Reading reading(asset, points);
		if (!ts.empty())
			reading.setUserTimestamp(ts);
		(*m_ingest)(m_data, reading);
	}
	else if (m_policy == mPolicyMultiple)
	{
		Logger::getLogger()->debug("Policy is to create multiple readings");
		string user_ts;
		vector<Datapoint *> points;
		for (auto& m : doc.GetObject())
		{
			if (!strcmp(m.name.GetString(), m_timestamp.c_str()))
			{
				if (m.value.IsString())
				{
					user_ts = m.value.GetString();
					convertTimestamp(user_ts);
				}
			}
			else if (m.value.IsInt64())
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
			else if (m.value.IsObject()) 
			{
				string ts;
				vector<Datapoint *> children;
				getValues(m.value, children, true, ts);
				Reading reading(m.name.GetString(), children);
				if (!ts.empty())
					reading.setUserTimestamp(ts);
				(*m_ingest)(m_data, reading);
			}
		}
		if (points.size() > 0)
		{
			Reading reading(asset, points);
			if (!user_ts.empty())
				reading.setUserTimestamp(user_ts);
			(*m_ingest)(m_data, reading);
		}
	}
}

/**
 * Get the values from the current level of the JSON document.
 * If the recuse flag is set we will recurse into child objects
 * and extract the data from those objects also.
 *
 * @param object	The object to iterate over
 * @param points	The datapoint array
 * @param recurse	Recusrse to nested objects
 */
void MQTTScripted::getValues(const Value& object, vector<Datapoint *>& points, bool recurse, string& user_ts)
{
	// Iterate the document
	for (auto& m : object.GetObject())
	{
		if (!strcmp(m.name.GetString(), m_timestamp.c_str()))
		{
			if (m.value.IsString())
			{
				user_ts = m.value.GetString();
				convertTimestamp(user_ts);
			}
		}
		else if (m.value.IsInt64())
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
		else if (m.value.IsObject() && recurse)
		{
			if (m_nest)
			{
				vector<Datapoint *> *children = new vector<Datapoint *>;
				string ts;
				getValues(m.value, *children, true, ts);
				DatapointValue	dpv(children, true);
				points.push_back(new Datapoint( m.name.GetString(), dpv));
			}
			else
			{
				string ts;
				getValues(m.value, points, true, ts);
			}
		}
	}
}



/**
 * Convert some common timestamp formats to formats required by FogLAMP
 *
 * @param ts	Timestamp to convert
 */
void MQTTScripted::convertTimestamp(string& ts)
{
struct tm tm;
char	buf[200];
double  fraction = 0;

	size_t pos = ts.find_first_of(".");
	if (pos != string::npos)
	{
		fraction = strtod(ts.substr(pos).c_str(), NULL);
	}
	strptime(ts.c_str(), m_timeFormat.c_str(), &tm);
	// Now adjust for the timezone
	time_t tim = mktime(&tm);
	tim += m_offset;
	gmtime_r(&tim, &tm);
	strftime(buf, sizeof(buf), DEFAULT_DATE_TIME_FORMAT, &tm);
	ts = buf;
	snprintf(buf, sizeof(buf), "%1.6f", fraction);
	ts += &buf[1];
}
