#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "xil_printf.h"
#include "xparameters.h"
#include "xgpiops.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xstatus.h"
#include "xttcps.h"
#include "xil_io.h"
#include "xllfifo.h"
#include "xil_cache.h"
#include "xbasic_types.h"
#include "xuartps.h"
#include "xiicps.h"

#include "stdio.h"

/*
 * If stdint.h cannot be located then:
 *   + If using GCC ensure the -nostdint options is *not* being used.
 *   + Ensure the project's include path includes the directory in which your
 *     compiler stores stdint.h.
 *   + Set any compiler options necessary for it to support C99, as technically
 *     stdint.h is only mandatory with C99 (FreeRTOS does not require C99 in any
 *     other way).
 *   + The FreeRTOS download includes a simple stdint.h definition that can be
 *     used in cases where none is provided by the compiler.  The files only
 *     contains the typedefs required to build FreeRTOS.  Read the instructions
 *     in FreeRTOS/source/stdint.readme for more information.
 */
#include <stdint.h> /* READ COMMENT ABOVE. */

#define PASS 0
#define FAIL 1
