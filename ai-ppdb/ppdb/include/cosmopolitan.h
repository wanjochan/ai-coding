#ifndef COSMOPOLITAN_H_
#define COSMOPOLITAN_H_

#if defined(_WIN32)

//win for quick dev and test only, and the binary is not portable for arm osx
//https://justine.lol/cosmopolitan/windows-compiling.html
//https://justine.lol/cosmopolitan/cosmopolitan.zip
//https://justine.lol/linux-compiler-on-windows/cross9.zip
#include "../../../repos/cosmos/cosmopolitan.h"

#else

#include <cosmo.h>

/* Standard C headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ctype.h>
#include <stdbool.h>
#include <setjmp.h>
#include <signal.h>     /* 信号处理 */
#include <pthread.h>    /* 线程支持 */
#include <stdatomic.h>  /* 原子操作 */
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#endif
#endif /* COSMOPOLITAN_H_ */
