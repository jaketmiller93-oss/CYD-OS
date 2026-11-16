/* Build guard for the actual Lua library, not just its users. */
#include "lua.h"
_Static_assert(sizeof(lua_Integer) == 8, "CYD Lua requires 64-bit integers in the library");
_Static_assert(sizeof(lua_Number) == 8, "CYD Lua requires double precision numbers");
unsigned cyd_lua_integer_bytes(void) { return sizeof(lua_Integer); }
unsigned cyd_lua_number_bytes(void) { return sizeof(lua_Number); }
