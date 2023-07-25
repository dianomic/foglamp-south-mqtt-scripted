#include <gtest/gtest.h>
#include <resultset.h>
#include <string.h>
#include <string>
#include <pyruntime.h>
#include <Python.h>

using namespace std;

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    wchar_t *programName = Py_DecodeLocale("unitTest", NULL);
    Py_SetProgramName(programName);
    PyMem_RawFree(programName);

    return RUN_ALL_TESTS();
}

