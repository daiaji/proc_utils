-- Luacheck configuration for proc_utils-ffi project

-- Recognize 'ffi' and 'bit' as globals provided by LuaJIT
globals = {
  "ffi",
  "bit"
}

-- Allow defining fields on a read-only table (for module definitions)
read_globals = {
  "proc" -- Used in test files
}

-- Standard LuaJIT environment
std = "luajit"

-- Ignore unused argument warnings for callback functions, which often have a fixed signature.
unused_args = "allow"