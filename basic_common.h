#pragma once

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#endif
#define CHECK(x)       do { int _x = (x); if (_x < 0) return _x; } while(0)
#define ENSURE(x,e)    do { if (!(x)) return e; } while(0)
