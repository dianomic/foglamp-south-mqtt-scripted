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

void logError();

/**
 * Constructor for the PythonScript class that is used to
 * convert the message payload
 *
 * @param name	The name of the south service
 */
PythonScript::PythonScript(const string& name) : m_init(false), m_pFunc(NULL), m_pModule(NULL)
{
	m_logger = Logger::getLogger();

	// Set python program name
	wchar_t *programName = Py_DecodeLocale(name.c_str(), NULL);
	Py_SetProgramName(programName);
	PyMem_RawFree(programName);

	// Inititialise embedded Python
	m_runtime = PythonRuntime::getPythonRuntime();

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

	m_init = true;
}

/**
 * Destructor for the Python script class
 */
PythonScript::~PythonScript()
{
	m_init = false;
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

	m_failedScript = false;
	m_execCount = 0;

	size_t start = name.find_last_of("/");
	if (start != std::string::npos)
	{
		start++;
	}
	else
	{
		start = 0;
	}
	PyGILState_STATE state = PyGILState_Ensure();

	string scriptName = name.substr(start);
	size_t end = scriptName.rfind(".py");
	if (end != std::string::npos)
	{
		scriptName = scriptName.substr(0, end);
	}

	// Load or reload script into Python module object
	if (m_script == scriptName && m_pModule)
	{
		m_logger->debug("Python reload module %s", m_script.c_str());

		PyObject *new_module = PyImport_ReloadModule(m_pModule);
		if (m_pModule)
		{
			Py_CLEAR(m_pModule);
			Py_CLEAR(m_pFunc);
		}
		m_pModule = new_module;
	}
	else
	{
		PyObject *pName = PyUnicode_FromString((char *)scriptName.c_str());
		if (m_pModule)
		{
			Py_CLEAR(m_pModule);
			Py_CLEAR(m_pFunc);
		}
		m_logger->debug("Python load module %s", scriptName.c_str());

		m_pModule = PyImport_Import(pName);

		Py_CLEAR(pName);
	}

	if (!m_pModule)
	{
		logError();

		PyGILState_Release(state);
		m_failedScript = true;
		return false;
	}

	// Set member variable
	m_script = scriptName;

	Py_CLEAR(m_pFunc);
	m_pFunc = PyObject_GetAttrString(m_pModule, (char*)"convert");
	if (!m_pFunc)
	{
		m_logger->error("The supplied script does not define a function called 'convert'");
		m_failedScript = true;
	}

	PyGILState_Release(state);

	return m_pFunc != NULL;;
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

	if (m_failedScript)
	{
		m_execCount++;
		if (m_execCount > 100)
		{
			m_logger->warn("The plugin is unable to process data without a valid 'convert' funtion in the script.");
			m_execCount = 0;
		}
		return doc;
	}

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
				m_logger->error("Execution of the convert Python function failed: %s", e.what());
				return NULL;
			}

			if (!pReturn)
			{
				logError();
				return NULL;
			}
			else if (pReturn == Py_None)
			{
				doc = new Document();
				auto& alloc = doc->GetAllocator();
				doc->SetObject();
				return doc;
			}
			else if (PyTuple_Check(pReturn))
			{
				if (PyArg_ParseTuple(pReturn, "O|O", &assetObject, &dict) == false)
				{

					m_logger->error("Return from Python convert function is of an incorrect type, it should be a Python DICT object or a string with the asset code and a DICT object with the reading data");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;
				}

				if (assetObject == NULL)
				{

					m_logger->error("When the return from the Python convert function is a pair of values the first of these must be a string containing the asset code");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;

				}
				else if (assetObject == Py_None)
				{

					m_logger->error("The returned asset name was None, either a valid string must be returned or the asset name may be omitted");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;

				}
				else if (dict == NULL)
				{

					m_logger->error("Return from Python convert function is of an incorrect type, it should be a Python DICT object or a string with the asset code and a DICT object with the reading data");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;

				}
				else if (dict == Py_None)
				{
					const char *name;
					if (PyUnicode_Check(assetObject))
					{
						name = PyUnicode_AsUTF8(assetObject);
					}
					else if (PyBytes_Check(assetObject))
					{
						PyBytes_AsString(assetObject);
					}
					else
					{
						m_logger->error("When the return from the Python convert function is a pair of values the first of these must be a string contianing the asset name");
						Py_CLEAR(pReturn);
						m_failedScript = true;
						m_execCount = 0;
						return NULL;
					}
					asset = name;
					doc = new Document();
					auto& alloc = doc->GetAllocator();
					doc->SetObject();
					return doc;
				}
				else  if (! PyDict_Check(dict))
				{

					m_logger->error("When the return from the Python convert function is a pair of values the second of these must be a Python DICT");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;
				}

				const char *name;
			       	if (PyUnicode_Check(assetObject))
				{
					name = PyUnicode_AsUTF8(assetObject);
				}
				else if (PyBytes_Check(assetObject))
				{
					PyBytes_AsString(assetObject);
				}
				else
				{
					m_logger->error("When the return from the Python convert function is a pair of values the first of these must be a string contianing the asset name");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;
				}
				if (! *name)
				{
					m_logger->error("An empty asset name has been returned by the sccript. Asset names can not be empty");
					Py_CLEAR(pReturn);
					m_failedScript = true;
					m_execCount = 0;
					return NULL;
				}
				asset = name;
				pValue = dict;

			}
			else if (!PyDict_Check(pReturn))
			{
				m_logger->error("Return from Python convert function is of an incorrect type, it should be a Python DICT object or a DICT object and a string");
				Py_CLEAR(pReturn);
				m_failedScript = true;
				m_execCount = 0;
				return NULL;
			}
			else
			{
				pValue = pReturn;
			}

			doc = new Document();
			Document::AllocatorType& alloc = doc->GetAllocator();
			doc->SetObject();
			PyObject *key = NULL;
			PyObject *value = NULL;
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
				else if (PyDict_Check(value))
				{
					Value child(kObjectType);
					createJSON(value, child, alloc);
					doc->AddMember(Value(name, alloc), child, alloc);
				}
				else
				{
					m_logger->error("Not adding data for '%s', unable to map type", name);
				}
			}
			Py_CLEAR(pReturn);
		}
		else
		{
			m_logger->error("The convert function is not callable in the supplied Python script");
		}
	}
	else
	{
		m_logger->fatal("The supplied Python script does not define a valid \"convert\" function");
	}

	PyGILState_Release(state);
	return doc;
}

/**
 * Log an error from the Python interpreter
 */
void PythonScript::logError()
{
PyObject *ptype, *pvalue, *ptraceback;

	if (PyErr_Occurred())
	{
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		PyErr_NormalizeException(&ptype,&pvalue,&ptraceback);

		char *msg, *file, *text;
		int line, offset;

		int res = PyArg_ParseTuple(pvalue,"s(siis)",&msg,&file,&line,&offset,&text);

		PyObject *line_no = PyObject_GetAttrString(pvalue,"lineno");
		PyObject *line_no_str = PyObject_Str(line_no);
		PyObject *line_no_unicode = PyUnicode_AsEncodedString(line_no_str,"utf-8", "Error");
		char *actual_line_no = PyBytes_AsString(line_no_unicode);  // Line number

		PyObject *ptext = PyObject_GetAttrString(pvalue,"text");
		PyObject *ptext_str = PyObject_Str(ptext);
		PyObject *ptext_no_unicode = PyUnicode_AsEncodedString(ptext_str,"utf-8", "Error");
		char *error_line = PyBytes_AsString(ptext_no_unicode);  // Line in error

		// Remove the trailing newline from the string
		char *newline = rindex(error_line,  '\n');
		if (newline)
		{
			*newline = '\0';
		}

		// Not managed to find a way to get the actual error message from Python
		// so use the string representation of the Error class and tidy it up, e.g.
		// SyntaxError('invalid syntax', ('/tmp/scripts/test_addition_script_script.py', 9, 1, '}\n'))
		PyObject *pstr = PyObject_Repr(pvalue);
		PyObject *perr = PyUnicode_AsEncodedString(pstr, "utf-8", "Error");
		char *err_msg = PyBytes_AsString(perr);
		char *end = index(err_msg, ',');
		if (end)
		{
			*end = '\0';
		}
		end = index(err_msg, '(');
		if (end)
		{
			*end = ' ';
		}

		if (strncmp(err_msg, "TypeError \"convert()", strlen("TypeError \"convert()")) == 0)
		{
			// Special catch to give better error message
			m_logger->error("The convert function defined in the Python script not have the correct number of arguments defined");
		}
		else if (error_line == NULL || actual_line_no == NULL || strcmp(error_line, "<NULL>") == 0
			       	|| strcmp(actual_line_no, "<NULL>") == 0 || *error_line == 0)
		{
			m_logger->error("Python error: %s in supplied script", err_msg);
		}
		else
		{
			m_logger->error("Python error: %s in %s at line %s of supplied script", err_msg, error_line, actual_line_no);
		}

		PyErr_Clear();
	}
}

void PythonScript::createJSON(PyObject *pValue, Value& node, Document::AllocatorType& alloc)
{
PyObject *key = NULL;
PyObject *value = NULL;
Py_ssize_t pos = 0;

	while (PyDict_Next(pValue, &pos, &key, &value))
	{
		const char *name = PyUnicode_Check(key) ? 
			PyUnicode_AsUTF8(key)
			: PyBytes_AsString(key);
		if (PyLong_Check(value) || PyLong_Check(value))
		{
			node.AddMember(Value(name, alloc), Value((int64_t)PyLong_AsLong(value)), alloc);
		}
		else if (PyFloat_Check(value))
		{
			node.AddMember(Value(name, alloc),Value(PyFloat_AS_DOUBLE(value)), alloc);
		}
		else if (PyBytes_Check(value))
		{
			node.AddMember(Value(name, alloc),Value(PyBytes_AsString(value), alloc), alloc);
		}
		else if (PyUnicode_Check(value))
		{
			node.AddMember(Value(name, alloc),Value(PyUnicode_AsUTF8(value), alloc), alloc);
		}
		else if (PyDict_Check(value))
		{
			Value child(kObjectType);
			createJSON(value, child, alloc);
			node.AddMember(Value(name, alloc), child, alloc);
		}
		else
		{
			m_logger->error("Not adding data for '%s', unable to map type", name);
		}
	}
}
