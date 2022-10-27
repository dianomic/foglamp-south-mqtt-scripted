#include <gtest/gtest.h>
#include <plugin_api.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <python_script.h>
#include <rapidjson/document.h>

using namespace std;
using namespace rapidjson;


TEST(MQTTScripted, SimplePython)
{
	PythonScript python("Test1");
	const char *fname = "script.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    return { \"a\" : \"b\" }\n");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname), true);
	string message = "{ \"a\" : \"b\" }";
	string topic ="unittest";
	string asset = "test1";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_NE(doc, (Document *)0);
	ASSERT_EQ(doc->HasParseError(), false);
	ASSERT_EQ(doc->IsObject(), true);
	ASSERT_EQ(doc->HasMember("a"), true);
	ASSERT_EQ((*doc)["a"].IsString(), true);
	ASSERT_STREQ((*doc)["a"].GetString(), "b");
	unlink(fname);
}

TEST(MQTTScripted, UpdateScript)
{
	PythonScript python("Test1");
	const char *fname = "update.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    return { \"a\" : \"b\" }\n");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname), true);
	string message = "{ \"a\" : \"b\" }";
	string topic = "unittest";
	string asset = "test1";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_NE(doc, (Document *)0);
	ASSERT_EQ(doc->HasParseError(), false);
	ASSERT_EQ(doc->IsObject(), true);
	ASSERT_EQ(doc->HasMember("a"), true);
	ASSERT_EQ((*doc)["a"].IsString(), true);
	ASSERT_STREQ((*doc)["a"].GetString(), "b");

	// Now change the script
	const char *fname2 = "update2.py";
	fp = fopen(fname2, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    return { \"temperature\" : 98 }\n");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname2), true);
	doc = python.execute(message, topic, asset);
	ASSERT_NE(doc, (Document *)0);
	ASSERT_EQ(doc->HasParseError(), false);
	ASSERT_EQ(doc->IsObject(), true);
	ASSERT_EQ(doc->HasMember("temperature"), true);
	ASSERT_EQ((*doc)["temperature"].IsNumber(), true);
	ASSERT_EQ((*doc)["temperature"].GetInt(), 98);
	unlink(fname);
}

TEST(MQTTScripted, NoDict)
{
	PythonScript python("Test1");
	const char *fname = "error.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    return \"a\"\n");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname), true);
	string message = "{ \"a\" : \"b\" }";
	string topic = "unittest";
	string asset = "test1";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_EQ(doc, (Document *)0);
	unlink(fname);
}


TEST(MQTTScripted, NoFunc)
{
	PythonScript python("Test1");
	const char *fname = "error2.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def foo(message, topic):\n");
	fprintf(fp, "    return \"a\"\n");
	fclose(fp);
	string topic = "unittest";
	ASSERT_EQ(python.setScript(fname), false);
	unlink(fname);
}

TEST(MQTTScripted, RuntimError)
{
	PythonScript python("Test1");
	const char *fname = "syntax.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    nonsense");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname), true);
	string message = "{ \"a\" : \"b\" }";
	string asset = "test1";
	string topic = "unittest";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_EQ(doc, (Document *)0);
	unlink(fname);
}

TEST(MQTTScripted, RuntimError2)
{
	PythonScript python("Test1");
	const char *fname = "rt2.py";
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "def convert(message, topic):\n");
	fprintf(fp, "    if ( a < b)");
	fprintf(fp, "        return 2");
	fprintf(fp, "    return 1");
	fclose(fp);
	ASSERT_EQ(python.setScript(fname), false);
	string message = "{ \"a\" : \"b\" }";
	string asset = "test1";
	string topic = "unittest";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_EQ(doc, (Document *)0);
	unlink(fname);
}


TEST(MQTTScripted, NoFile)
{
	PythonScript python("Test1");
	const char *fname = "none.py";
	ASSERT_EQ(python.setScript(fname), false);
	string message = "{ \"a\" : \"b\" }";
	string asset = "test1";
	string topic = "unittest";
	Document *doc = python.execute(message, topic, asset);
	ASSERT_EQ(doc, (Document *)0);
}

