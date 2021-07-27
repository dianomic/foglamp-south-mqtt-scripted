/*      
 * FogLAMP python script runner
 *                             
 * Copyright (c) 2021 Dianomic Systems
 *                             
 * Released under the Apache 2.0 Licence
 *                                      
 * Author: Mark Riddoch
 */
#include <python_script.h>
#include <utils.h>
#include <dlfcn.h>
#include "plugin_api.h"

using namespace std;
using namespace rapidjson;

/**
 * Constructor for the PythonScript class that is used to
 * convert the message payload
 *
 * @param name	The name of the south service
 */
PythonScript::PythonScript(const string& name) : m_init(false), m_pFunc(NULL), m_libpythonHandle(NULL), m_pModule(NULL)
{
	m_logger = Logger::getLogger();
	wchar_t *programName = Py_DecodeLocale(name.c_str(), NULL);
	Py_SetProgramName(programName);
	PyMem_RawFree(programName);

	m_logger->info("PythonScript c'tor: Py_IsInitialized()=%s", Py_IsInitialized()?"true":"false");
	if (!Py_IsInitialized())
	{
#ifdef PLUGIN_PYTHON_SHARED_LIBRARY
		string openLibrary = TO_STRING(PLUGIN_PYTHON_SHARED_LIBRARY);
		if (!openLibrary.empty())
		{
			m_libpythonHandle = dlopen(openLibrary.c_str(),
						  RTLD_LAZY | RTLD_LOCAL);
			m_logger->info("Pre-loading of library '%s' "
						  "is needed on this system",
						  openLibrary.c_str());
		}
#endif
		m_logger->info("PythonScript c'tor: line %d", __LINE__);
		Py_Initialize();
		m_logger->info("PythonScript c'tor: line %d", __LINE__);
		//PyEval_InitThreads(); // Initialize and acquire the global interpreter lock (GIL)
		//m_logger->info("PythonScript c'tor: line %d", __LINE__);
		PyThreadState* save = PyEval_SaveThread(); // release GIL
		m_logger->info("PythonScript c'tor: line %d", __LINE__);
		m_init = true;
	}

	PyGILState_STATE state = PyGILState_Ensure(); // acquire GIL

	// Set Python path for embedded Python 3.5
	// Get current sys.path. borrowed reference
	PyObject* sysPath = PySys_GetObject((char *)string("path").c_str());
	// Add FogLAMP python filters path
	string path = getDataDir() + "/scripts";
	PyObject* pPath = PyUnicode_DecodeFSDefault((char *)path.c_str());
	PyList_Insert(sysPath, 0, pPath);
	m_logger->info("PythonScript c'tor: Set sysPath=%s", path.c_str());
	// Remove temp object
	Py_CLEAR(pPath);
	PyGILState_Release(state);
}

/**
 * Destructor for the Python script class
 */
PythonScript::~PythonScript()
{
	if (m_init)
	{
		if (Py_IsInitialized())
		{
			PyGILState_STATE state = PyGILState_Ensure();
			Py_DECREF(m_pFunc);
			Py_DECREF(m_pModule);
			Py_Finalize();
			m_logger->info("~PythonScript(): Py_Finalize done");
		}
		if (m_libpythonHandle)
		{
			dlclose(m_libpythonHandle);
			m_logger->info("~PythonScript(): dl_close done");
		}
	}
}

/**
 * Called to set the script to run. Imports the Python module
 * and sets up the environment in order to facilitate the calling
 * of the Python function.
 *
 * @param name	The name of the Python script
 */
bool PythonScript::setScript(const string& name)
{
	m_logger->info("Script to execute is '%s'", name.c_str());

	size_t start = name.find_last_of("/");
	if (start != std::string::npos)
	{
		start++;
	}
	else
	{
		start = 0;
	}
	m_script = name.substr(start);
	size_t end = m_script.rfind(".py");
	if (end != std::string::npos)
	{
		m_script = m_script.substr(0, end);
	}
	PyGILState_STATE state = PyGILState_Ensure();

	PyObject *pName = PyUnicode_FromString((char *)m_script.c_str());
	m_logger->info("PythonScript::setScript: m_script=%s", m_script.c_str());
	if (m_pModule)
	{
		m_logger->info("PythonScript::setScript: before ReloadModule");
		PyObject *new_module = PyImport_ReloadModule(m_pModule);
		m_logger->info("PythonScript::setScript: line %d", __LINE__);
		Py_DECREF(m_pModule);
		m_logger->info("PythonScript::setScript: line %d", __LINE__);
        	m_pModule = new_module;
		m_logger->info("PythonScript::setScript: after ReloadModule");
	}
	else
	{
		m_logger->info("PythonScript::setScript: before Import");
		m_pModule = PyImport_Import(pName);
		m_logger->info("PythonScript::setScript: after Import");
	}
	if (!m_pModule)
	{
		m_logger->error("Failed to import script %s", m_script.c_str());
		return false;
	}
	PyObject *pDict = PyModule_GetDict(m_pModule);
	if (pDict)
	{
		m_pFunc = PyDict_GetItemString(pDict, (char*)"convert");
		Py_DECREF(pDict);
	}
	else
	{
		m_logger->error("Unable to extract dictionary from imported Python module");
	}
	PyGILState_Release(state);
	return true;
}

void PythonScript::freeMemObj(PyObject *obj1)
{
	if (obj1 != NULL)
		Py_CLEAR (obj1);
}

void PythonScript::freeMemAll(PyObject *obj1, char *str, PyObject *obj3)
{
	freeMemObj (obj1);

	if (str != NULL)
		Py_CLEAR (str);

	freeMemObj (obj3);
}

/**
 * Execute the mapping function. This function is always called
 * convert and is passed the MQTT message as a string. It must return
 * a Python DICT which is a set of key/value pairs that make up the data
 * points for this reading.
 *
 * @param message	The MQTT message string
 */
Document *PythonScript::execute(const string& message, const string& topic, string& asset)
{
Document *doc = NULL;

	PyGILState_STATE state = PyGILState_Ensure();
	if (m_pFunc)
	{
		if (PyCallable_Check(m_pFunc))
		{
			m_logger->info("topic=%s, message=%s", topic.c_str(), message.c_str());
			PyObject *dict;
			PyObject *assetObject;
			PyObject *pValue;
			PyObject *pReturn;
		       
			try {
				pReturn = PyObject_CallFunction(m_pFunc, "ss", message.c_str(), topic.c_str());
			} catch (exception& e) {
				m_logger->error("Python script execution failed: %s", e.what());
				return NULL;
			}

			if (!pReturn)
			{
				m_logger->error("Python convert function failed to return data");
				return NULL;
			}
			else
			{
				if (PyTuple_Check(pReturn)) {
					m_logger->info("%s : %d", __FUNCTION__, __LINE__);
					if (PyArg_ParseTuple(pReturn, "O|O", &assetObject, &dict) == false) {

						m_logger->error("a STRING and a DICT are expected as return values from the Python convert function");
						freeMemObj(pReturn);
						freeMemObj(assetObject);
						freeMemObj(dict);
						return NULL;
					}
					else
						m_logger->info("%s : %d", __FUNCTION__, __LINE__);

					if (assetObject == NULL){

						m_logger->error("a STRING is expected as the first value returned by the Python convert function");
						freeMemObj(pReturn);
						freeMemObj(dict);
						return NULL;

					} else if (dict == NULL){

						m_logger->error("a DICT is expected as the second value returned by the Python convert function");
						freeMemObj(pReturn);
						freeMemObj(assetObject);
						return NULL;

					} else {
						if (! PyDict_Check(dict)){

							m_logger->error("a DICT is expected as the second value returned by the Python convert function");
							freeMemObj(pReturn);
							freeMemObj(assetObject);
							freeMemObj(dict);
							return NULL;
						}
					}
					m_logger->info("%s : %d", __FUNCTION__, __LINE__);

					const char *name = PyUnicode_Check(assetObject) ?
						PyUnicode_AsUTF8(assetObject) : PyBytes_AsString(assetObject);
					m_logger->info("asset = name = %s", name);
					asset = name;
					pValue = dict;
					//m_logger->info("asset=%s, pValue=%s", asset,  PyUnicode_AsUTF8(PyObject_Repr(pValue)));
					m_logger->info("%s : %d", __FUNCTION__, __LINE__);
					// freeMemObj(pReturn);
					// m_logger->info("%s : %d", __FUNCTION__, __LINE__);

				} else {
					m_logger->info("%s : %d", __FUNCTION__, __LINE__);
					if (!PyDict_Check(pReturn))
					{
						m_logger->error("Return from Python convert function is not a DICT object");
						freeMemObj(pReturn);
						freeMemObj(assetObject);
						freeMemObj(dict);
						return NULL;
					}
					pValue = pReturn;
					m_logger->info("%s : %d", __FUNCTION__, __LINE__);
				}

			}

			doc = new Document();
			auto& alloc = doc->GetAllocator();
			doc->SetObject();
			PyObject *key, *value;
			Py_ssize_t pos = 0;

			while (PyDict_Next(pValue, &pos, &key, &value))
			{
				m_logger->info("%s : %d", __FUNCTION__, __LINE__);
				const char *name = PyUnicode_Check(key) ? 
					PyUnicode_AsUTF8(key)
					: PyBytes_AsString(key);
				if (PyLong_Check(value) || PyLong_Check(value))
				{
					doc->AddMember(Value(name, alloc), Value((int64_t)PyLong_AsLong(value)), alloc);
				}
				else if (PyFloat_Check(value))
				{
					doc->AddMember(Value(name, alloc),Value(PyFloat_AS_DOUBLE(value)), alloc);
				}
				else if (PyBytes_Check(value))
				{
					doc->AddMember(Value(name, alloc),Value(PyBytes_AsString(value), alloc), alloc);
				}
				else if (PyUnicode_Check(value))
				{
					doc->AddMember(Value(name, alloc),Value(PyUnicode_AsUTF8(value), alloc), alloc);
				}
				else
				{
					m_logger->error("Not adding data for '%s', unable to map type", name);
				}
			}
			m_logger->info("%s : %d", __FUNCTION__, __LINE__);
			freeMemObj(pValue);
			// m_logger->info("%s : %d", __FUNCTION__, __LINE__);
			// freeMemObj(pReturn);
			m_logger->info("%s : %d", __FUNCTION__, __LINE__);
			freeMemObj(assetObject);
			m_logger->info("%s : %d", __FUNCTION__, __LINE__);
			freeMemObj(dict);
			m_logger->info("%s : %d", __FUNCTION__, __LINE__);
		}
		else
		{
			m_logger->error("The convert function is not callable in the supplied Python script");
		}

	}
	else
	{
		m_logger->fatal("Unable to create Python reference to function \"convert\"");
	}

	PyGILState_Release(state);
	return doc;
}
