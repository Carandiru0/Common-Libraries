#pragma once
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN	// Exclude rarely-used stuff from Windows headers
#endif

#define NOMINMAX
#define _ITERATOR_DEBUG_LEVEL 0

#define _USE_MATH_DEFINES
#include <Math/superfastmath.h>

#define __TBB_USE_PROPORTIONAL_SPLIT_IN_BLOCKED_RANGES 0

#include <tbb/tbb_stddef.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/atomic.h>
#include <tbb/scalable_allocator.h>
#include <tbb/spin_rw_mutex.h>
#include <tbb/queuing_rw_mutex.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <tbb/parallel_invoke.h>
#include <tbb/task_group.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>

// reference additional headers your program requires here
#if INCLUDE_JPEG_SUPPORT
#pragma comment(lib, "jpeg-static.lib")
#endif


