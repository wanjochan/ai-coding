#include "test_cmdline.h"

int main(void)
{
    TEST_BEGIN("PPDB Command Line Framework Test");

    TEST_RUN(test_cmdline_register);
    TEST_RUN(test_cmdline_help);
    TEST_RUN(test_cmdline_execute);

    TEST_END();
    return 0;
} 