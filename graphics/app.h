#ifndef _app_h_
#define _app_h_

#include "vm/tl.h"

// for native toolkits to implmement
void toolkit_init(int argc, char** argv); // any init code for types for window/app/graphics
void toolkit_start(); // should exit only when mainloop exits
void toolkit_stop(); // called from hotel thread to stop the loop

// to signal back that the toolkit is up and running
void toolkit_started();

void toolkit_launch();

#endif // _app_h_
