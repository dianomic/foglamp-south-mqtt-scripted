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
		Py_Initialize();
		//PyEval_InitThreads(); // Initialize and acquire the global interpreter lock (GIL)
		PyThreadState* save = PyEval_SaveThread(); // release GIL
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
			Py_CLEAR(m_pFunc);
			Py_CLEAR(m_pModule);
			Py_Finalize();
		}
		if (m_libpythonHandle)
		{
			dlclose(m_libpythonHandle);
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
	if (m_pModule)
	{
		PyObject *new_module = PyImport_ReloadModule(m_pModule);
		Py_CLEAR(m_pModule);
        	m_pModule = new_module;
	}
	else
	{
		m_pModule = PyImport_Import(pName);
	}
	Py_CLEAR(pName);
	if (!m_pModule)
	{
		m_logger->error("Failed to import script %s", m_script.c_str());
		return false;
	}
	PyObject *pDict = PyModule_GetDict(m_pModule);
	if (pDict)
	{
		m_pFunc = PyDict_GetItemString(pDict, (char*)"convert");
		Py_CLEAR(pDict);
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
			PyObject *dict = NULL;
			PyObject *assetObject = NULL;
			PyObject *pValue = NULL;
			PyObject *pReturn = NULL;
		       
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
					if (PyArg_ParseTuple(pReturn, "O|O", &assetObject, &dict) == false) {

						m_logger->error("a STRING and a DICT are expected as return values from the Python convert function");
						freeMemObj(pReturn);
						freeMemObj(assetObject);
						freeMemObj(dict);
						return NULL;
					}

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

					const char *name = PyUnicode_Check(assetObject) ?
						PyUnicode_AsUTF8(assetObject) : PyBytes_AsString(assetObject);
					asset = name;
					pValue = dict;

				} else {
					if (!PyDict_Check(pReturn))
					{
						m_logger->error("Return from Python convert function is not a DICT object");
						freeMemObj(pReturn);
						freeMemObj(assetObject);
						freeMemObj(dict);
						return NULL;
					}
					pValue = pReturn;
				}

			}

			doc = new Document();
			auto& alloc = doc->GetAllocator();
			doc->SetObject();
			PyObject *key, *value;
			Py_ssize_t pos = 0;

			while (PyDict_Next(pValue, &pos, &key, &value))
			{
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
			freeMemObj(pValue);
			freeMemObj(assetObject);
			freeMemObj(dict);
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
