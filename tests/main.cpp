#include <gtest/gtest.h>
#include <resultset.h>
#include <string.h>
#include <string>

using namespace std;

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    setenv("PYTHONPATH", ".", 1);

    testing::GTEST_FLAG(repeat) = 200;
    testing::GTEST_FLAG(shuffle) = true;
    testing::GTEST_FLAG(death_test_style) = "threadsafe";

    return RUN_ALL_TESTS();
}

