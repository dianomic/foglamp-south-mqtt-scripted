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
#include <rapidjson/document.h>

class PythonScript {
	public:
		PythonScript(const std::string& name);
		~PythonScript();
		bool			setScript(const std::string& file);
		rapidjson::Document	*execute(const std::string& message);
	private:
		std::string		m_script;
		bool			m_init;
		Logger			*m_logger;
		void			*m_libpythonHandle;
		PyObject		*m_pFunc;
};

#endif
