#ifndef _SCRIPTED_H
#define _SCRIPTED_H
/*
 * FogLAMP south service plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <MQTTClient.h>
#include <python_script.h>
#include <reading.h>
#include <config_category.h>
#include <plugin_api.h>
#include <logger.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>

typedef void (*INGEST_CB)(void *, Reading);

#define	INITIAL_RECONNECT_WAIT	100	// Number of milliseconds before next attempt to reconnect
#define MAX_RECONNECT_WAIT	(10 * INITIAL_RECONNECT_WAIT)
#define CONNECT_ERROR_INTERVAL  60      // Interval between connection errors in seconds

/**
 * A scripted MQTT client plugin.
 *
 * The plugin connects to an MQTT broker and subscribes to a given
 * token. Any messages that are receioved may be either simple JSON
 * douments, a single value or a generic string message. This later
 * type will be passed to a Python script that is defined by the user.
 * This Python script must convert the message to a Python DICT that
 * consists of simple key/value pairs that will be used as the datapoints
 * in the return reading.
 */
class MQTTScripted {
	public:
				MQTTScripted(ConfigCategory *config);
				~MQTTScripted();
		void		reconfigure(const ConfigCategory& config);
		bool		start();
		void		stop();
		void		registerIngest(void *data, void (*cb)(void *, Reading))
				{
					m_ingest = cb;
					m_data = data;
				}
		void		processMessage(const std::string& topic, const std::string& payload);
		bool		reconnect();
		std::string	getName() { return m_name; };
		void		sslError(const char *str, int len) {
					m_logger->error("SSL Error: %s", str);
				};
		void		reconnection() {
					std::lock_guard<std::mutex> guard(m_mutex);
					backgroundReconnect();
				}
		void		reconnectRetry();
	private:
		void			(*m_ingest)(void *, Reading);
		std::string		privateKeyPath();
		std::string		serverCertPath();
		std::string		clientCertPath();
		std::string		pemPath();
		void			processDocument(rapidjson::Document& doc, const std::string &asset);
		void			getValues(const rapidjson::Value& object, std::vector<Datapoint *>& points, bool recurse, std::string& user_ts);
		void			processPolicy(const std::string& policy);
		void			convertTimestamp(std::string& ts);
		void			backgroundReconnect();

	private:
		std::string		m_asset;
		std::string		m_broker;
		std::string		m_topic;
		std::string		m_script;
		std::string		m_content;
		int			m_qos;
		std::string		m_clientID;
		Logger			*m_logger;
		std::mutex		m_mutex;
		MQTTClient		m_client;
		void			*m_data;
		PythonScript		*m_python;
		std::string		m_name;
		bool			m_restart;
		std::string		m_key;
		std::string		m_serverCert;
		std::string		m_clientCert;
		std::string		m_keyPath;
		std::string		m_keyPass;
		std::string		m_serverCertPath;
		std::string		m_clientCertPath;
		std::string		m_username;
		std::string		m_password;
		enum { mFailed, mCreated, mConnected }
					m_state;
		std::string		m_pemPath;
		enum { mPolicyFirstLevel, mPolicyCollapse, mPolicyMultiple }
					m_policy;
		bool			m_nest;
		std::string		m_timestamp;
		std::string		m_timeFormat;
		long			m_offset;
		std::thread		*m_reconnectThread;
		bool			m_reap;
		time_t			m_connectFailTime;
};
#endif
