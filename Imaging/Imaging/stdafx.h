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

#include <tbb/tbb.h>

// reference additional headers your program requires here
#if INCLUDE_JPEG_SUPPORT
#pragma comment(lib, "jpeg-static.lib")
#endif


