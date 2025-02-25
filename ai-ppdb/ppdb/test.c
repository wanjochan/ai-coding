#include "cosmopolitan.h"

int main(int argc, char **argv) {
    write(1, "Hello from TCC!\n", 15);
    write(1, "Arguments:\n", 11);
    for (int i = 0; i < argc; i++) {
        write(1, "  argv[", 7);
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d", i);
        write(1, buf, len);
        write(1, "] = ", 4);
        write(1, argv[i], strlen(argv[i]));
        write(1, "\n", 1);
    }
    return 0;
} 