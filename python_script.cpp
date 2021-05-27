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
						  RTLD_LAZY | RTLD_GLOBAL);
			m_logger->info("Pre-loading of library '%s' "
						  "is needed on this system",
						  openLibrary.c_str());
		}
#endif
		Py_Initialize();
		PyEval_InitThreads(); // Initialize and acquire the global interpreter lock (GIL)
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

	//# FIXME_I
	Logger::getLogger()->setMinLevel("debug");
	Logger::getLogger()->debug("xxx2 %s - m_script :%s:", __FUNCTION__, m_script.c_str());
	Logger::getLogger()->setMinLevel("warning");


	PyObject *pName = PyUnicode_FromString((char *)m_script.c_str());
	if (m_pModule)
		m_pModule = PyImport_ReloadModule(m_pModule);
	else
		m_pModule = PyImport_Import(pName);
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
	// FIXME_I:
	char *strBuffer;

	// FIXME_I:
	strBuffer = (char *) malloc(1000);

	//# FIXME_I
	Logger::getLogger()->setMinLevel("debug");
	Logger::getLogger()->debug("xxx2 %s - v2", __FUNCTION__);
	Logger::getLogger()->setMinLevel("warning");

	PyGILState_STATE state = PyGILState_Ensure();
	if (m_pFunc)
	{

		//# FIXME_I
		Logger::getLogger()->setMinLevel("debug");
		Logger::getLogger()->debug("xxx2 %s - BRK 1 v2 ", __FUNCTION__);
		Logger::getLogger()->setMinLevel("warning");

		if (PyCallable_Check(m_pFunc))
		{
			//# FIXME_I
			Logger::getLogger()->setMinLevel("debug");
			Logger::getLogger()->debug("xxx2 %s - BRK 1.1 ", __FUNCTION__);
			Logger::getLogger()->setMinLevel("warning");


			// FIXME_I:
			PyObject *dict;
			PyObject *pValue;
			PyObject *pReturn = PyObject_CallFunction(m_pFunc, "ss", message.c_str(), topic.c_str());

			//# FIXME_I
			Logger::getLogger()->setMinLevel("debug");
			Logger::getLogger()->debug("xxx2 %s - BRK 1.2  %s %s", __FUNCTION__,  message.c_str(), topic.c_str());
			Logger::getLogger()->setMinLevel("warning");

			if (!pReturn)
			{
				m_logger->error("xxx2 Python convert function failed to return data");
				return NULL;
			}
			else
			{
				//# FIXME_I
				Logger::getLogger()->setMinLevel("debug");
				Logger::getLogger()->debug("xxx2 %s - bk2 v2", __FUNCTION__);
				Logger::getLogger()->setMinLevel("warning");

				//# FIXME_I
				Logger::getLogger()->setMinLevel("debug");
				Logger::getLogger()->debug("xxx2 %s - bk0.1 ", __FUNCTION__);
				Logger::getLogger()->setMinLevel("warning");

				// FIXME_I:
				if (PyTuple_Check(pReturn)) {

					//# FIXME_I
					Logger::getLogger()->setMinLevel("debug");
					Logger::getLogger()->debug("xxx2 %s - bk0.2 ", __FUNCTION__);
					Logger::getLogger()->setMinLevel("warning");


					//# FIXME_I
					Logger::getLogger()->setMinLevel("debug");
					Logger::getLogger()->debug("xxx2 %s - bk2.2 ", __FUNCTION__);
					Logger::getLogger()->setMinLevel("warning");

					try {

						if (PyArg_ParseTuple(pReturn, "so", &strBuffer, &dict) == false) {

							//# FIXME_I
							Logger::getLogger()->setMinLevel("debug");
							Logger::getLogger()->error("xxx2 %s - bk3 PyArg_ParseTuple :%s: ", __FUNCTION__, strBuffer);
							Logger::getLogger()->setMinLevel("warning");


							// FIXME_I:
							if (PyDict_Check(dict)){

								//# FIXME_I
								Logger::getLogger()->setMinLevel("debug");
								Logger::getLogger()->debug("xxx2 %s - bk3.01 - dict ", __FUNCTION__);
								Logger::getLogger()->setMinLevel("warning");

							} else {
								//# FIXME_I
								Logger::getLogger()->setMinLevel("debug");
								Logger::getLogger()->debug("xxx2 %s - bk3.01 - NO dict ", __FUNCTION__);
								Logger::getLogger()->setMinLevel("warning");

							}

							return NULL;
						}
					} catch (const std::exception &e)
					{
						Logger::getLogger()->error("xxx %s - exception PyArg_ParseTuple :%s:", __FUNCTION__, e.what());
					}


					// FIXME_I:
					if (PyDict_Check(dict)){

						//# FIXME_I
						Logger::getLogger()->setMinLevel("debug");
						Logger::getLogger()->debug("xxx2 %s - bk3.01 - dict ", __FUNCTION__);
						Logger::getLogger()->setMinLevel("warning");

					} else {
						//# FIXME_I
						Logger::getLogger()->setMinLevel("debug");
						Logger::getLogger()->debug("xxx2 %s - bk3.01 - NO dict ", __FUNCTION__);
						Logger::getLogger()->setMinLevel("warning");

					}


					asset= strBuffer;

					//# FIXME_I
					Logger::getLogger()->setMinLevel("debug");
					Logger::getLogger()->debug("xxx2 %s - bk3 :%s: ", __FUNCTION__, asset.c_str());
					Logger::getLogger()->setMinLevel("warning");


					// FIXME_I:
					pValue = dict;

//					if (!PyDict_Check(dict)){
//
//						// FIXME_I:
//						m_logger->error("xxx2 Return from Python convert function is not a DICT object");
//						return NULL;
//					} else {
//						//# FIXME_I
//						Logger::getLogger()->setMinLevel("debug");
//						Logger::getLogger()->debug("xxx2 %s - bk4 ", __FUNCTION__);
//						Logger::getLogger()->setMinLevel("warning");
//
//						pValue = dict;
//					}

					//# FIXME_I
					Logger::getLogger()->setMinLevel("debug");
					Logger::getLogger()->debug("xxx2 %s - bk3.1 :%s: ", __FUNCTION__, asset.c_str());
					Logger::getLogger()->setMinLevel("warning");


				} else {
					//# FIXME_I
					Logger::getLogger()->setMinLevel("debug");
					Logger::getLogger()->debug("xxx2 %s - bk0.4.1 ", __FUNCTION__);
					Logger::getLogger()->setMinLevel("warning");


					pValue = pReturn;
				}

				//# FIXME_I
				Logger::getLogger()->setMinLevel("debug");
				Logger::getLogger()->debug("xxx2 %s - bk0.4.2 ", __FUNCTION__);
				Logger::getLogger()->setMinLevel("warning");


			}

			if (!PyDict_Check(dict)){

				//# FIXME_I
				Logger::getLogger()->setMinLevel("debug");
				Logger::getLogger()->debug("xxx2 %s - bk0.3 ", __FUNCTION__);
				Logger::getLogger()->setMinLevel("warning");


				// FIXME_I:
				m_logger->error("xxx2 Return from Python convert function is neither a DICT object or a TUPLE");
				return NULL;
			}

			//# FIXME_I
			Logger::getLogger()->setMinLevel("debug");
			Logger::getLogger()->debug("xxx2 %s - BRK 1.3 ", __FUNCTION__);
			Logger::getLogger()->setMinLevel("warning");

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
			//Py_CLEAR(pReturn);
			// FIXME_I:
			//Py_CLEAR(dict);

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


	//# FIXME_I
	Logger::getLogger()->setMinLevel("debug");
	Logger::getLogger()->debug("xxx2 %s - BRK 2 ", __FUNCTION__);
	Logger::getLogger()->setMinLevel("warning");


	PyGILState_Release(state);
	return doc;
}
