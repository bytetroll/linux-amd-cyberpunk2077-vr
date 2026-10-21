#!/usr/bin/env python3
"""Run actual Weapon carry/Holster Lua callbacks with a mocked slot lifecycle."""
import ctypes
import ctypes.util
from pathlib import Path
import sys


def main():
    repo = Path(__file__).resolve().parents[2]
    name = ctypes.util.find_library("lua5.4")
    if not name:
        raise SystemExit("Lua 5.4 shared library not found")
    lua = ctypes.CDLL(name)
    lua.luaL_newstate.restype = ctypes.c_void_p
    lua.luaL_openlibs.argtypes = [ctypes.c_void_p]
    lua.lua_pushlstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
    lua.lua_setglobal.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lua.luaL_loadbufferx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                   ctypes.c_char_p, ctypes.c_char_p]
    lua.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                             ctypes.c_ssize_t, ctypes.c_void_p]
    lua.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    lua.lua_tolstring.restype = ctypes.c_char_p
    lua.lua_close.argtypes = [ctypes.c_void_p]
    state = lua.luaL_newstate()
    if not state:
        raise SystemExit("Could not create Lua state")
    try:
        lua.luaL_openlibs(state)
        weapon = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / "mods/cet/CyberpunkVRPort_Weapon/init.lua"
        inputs = {"weapon": weapon.read_bytes(),
                  "holster": (repo / "mods/cet/CyberpunkVRPort_Holster/init.lua").read_bytes()}
        for key, data in inputs.items():
            lua.lua_pushlstring(state, data, len(data))
            lua.lua_setglobal(state, key.encode())
        fixture = Path(__file__).with_name("weapon_carry.lua").read_bytes()
        result = lua.luaL_loadbufferx(state, fixture, len(fixture), b"weapon_carry_fixture", None)
        if result == 0:
            result = lua.lua_pcallk(state, 0, 1, 0, 0, None)
        message = lua.lua_tolstring(state, -1, None)
        print(message.decode() if message else "<no fixture result>")
        return result
    finally:
        lua.lua_close(state)


if __name__ == "__main__":
    raise SystemExit(main())
