#pragma once

#if defined DEBUG_relay
#include <stdio.h>
#include <errno.h>
#define debug_log(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_log(...)
#endif

#if defined DEBUG_VERBOSE_relay
#include <stdio.h>
#include <errno.h>
#define debug_log_v(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_log_v(...)
#endif
