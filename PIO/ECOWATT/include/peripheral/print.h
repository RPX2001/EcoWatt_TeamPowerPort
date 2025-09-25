#ifndef PRINT_H
#define PRINT_H

#include "driver/debug.h"

#define print(...) debug.log(__VA_ARGS__)
#define print_init() debug.init()

#endif