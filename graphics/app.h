#ifndef _app_h_
#define _app_h_

#include "vm/tl.h"

typedef struct {
    tlNativeCb cb;
    tlArgs* args;
    tlHandle result;
} tlRunOnMain;

// for native toolkits to implmement
void toolkit_init(int argc, char** argv); // any init code for types for window/app/graphics
void toolkit_start(); // should exit only when mainloop exits
void toolkit_stop(); // called from hotel thread to stop the loop
void toolkit_schedule(tlRunOnMain* onmain); // run a native function in the tookit thread

// callbacks to notify integration of events
void toolkit_started();
void toolkit_schedule_done(tlRunOnMain* onmain);

// others
void toolkit_launch();
tlHandle tl_on_toolkit(tlNativeCb cb, tlArgs* args);
void tl_on_toolkit_async(tlNativeCb cb, tlArgs* args);

// indicate toolkit thread is blocked on hotel
void block_toolkit();
void unblock_toolkit();

#endif // _app_h_
