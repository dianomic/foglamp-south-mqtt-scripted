#ifndef _PYTHON_SCRIPT_H
#define _PYTHON_SCRIPT_H
/**
 * Python script encapsulation for south plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */

#include <logger.h>
#include <Python.h>
#include <pyruntime.h>
#include <rapidjson/document.h>

class PythonScript {
	public:
		PythonScript(const std::string& name);
		~PythonScript();
		bool			setScript(const std::string& file);
		rapidjson::Document	*execute(const std::string& message, const std::string& topic,  std::string& asset);
	private:
		void createJSON(PyObject *pValue, rapidjson::Value& node, rapidjson::Document::AllocatorType& alloc);
		void freeMemObj(PyObject *obj1);
		void freeMemAll(PyObject *obj1, char *str, PyObject *obj3);

		std::string		m_script;
		bool			m_init;
		Logger			*m_logger;
		PyObject		*m_pFunc;
		PyObject		*m_pModule;
		PythonRuntime		*m_runtime;
};

#endif
