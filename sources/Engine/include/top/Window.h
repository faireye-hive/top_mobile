#pragma once
#ifdef __EMSCRIPTEN__
#include "top/WebWindow.h"
namespace top { using Window = WebWindow; }
#else
#include "top/GlxWindow.h"
namespace top { using Window = GlxWindow; }
#endif
