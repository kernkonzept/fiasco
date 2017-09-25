#ifndef INITCALLS_H__
#define INITCALLS_H__

#include "globalconfig.h"

#define FIASCO_INIT		__attribute__ ((__section__ (".initcall.text")))
#define FIASCO_INITDATA		__attribute__ ((__section__ (".initcall.data")))

#define FIASCO_INIT_AND_PM
#define FIASCO_INIT_AND_PM_DATA

#define FIASCO_INIT_CPU_AND_PM
#define FIASCO_INIT_CPU_AND_PM_DATA

#ifdef CONFIG_MP
# define FIASCO_INIT_CPU
# define FIASCO_INITDATA_CPU
#else
# define FIASCO_INIT_CPU FIASCO_INIT
# define FIASCO_INITDATA_CPU FIASCO_INITDATA
#endif

#endif // INITCALLS_H__
