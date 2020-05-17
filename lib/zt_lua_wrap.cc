/* MIT License

Copyright (c) 2020 StandBy.Network

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <zt_lua_wrap.h>

#include <lua.hpp>

namespace standby_network
{

//--------------------------------------------------------------helpers-----------------------------------------------------------------

auto udp_send(lua_State *l) -> int
{
    if(lua_isinteger(l, -2))
    {
        if(lua_isstring(l, -1))
        {
            CommLayer *c = *static_cast<CommLayer **>(lua_getextraspace(l));
            uint64_t node_id = lua_tointeger(l, -2);
            const char *msg = lua_tostring(l, -1);

            int err;
            if((err = c->udp_send(node_id, msg)) >= 0)
            {
                lua_pushinteger(l, err);
                return 1;
            }
            else
            {
                /* couldn't send */
                lua_pushinteger(l, err);
                lua_pushstring(l, "Couldn't send the data");
                return 2;
            }
        }
        else
        {
            /* not a string */
            lua_pushinteger(l, -1);
            lua_pushstring(l, "The second argument must be a string");
            return 2;
        }
    }
    else
    {
        /* not an int */
        lua_pushinteger(l, -1);
        lua_pushstring(l, "The first argument must be an integeer");
        return 2;
    }
}

auto udp_recv(lua_State *l) -> int
{
    if(lua_isinteger(l, -1))
    {
        CommLayer *c = *static_cast<CommLayer **>(lua_getextraspace(l));
        uint64_t node_id = lua_tointeger(l, -1);
        
        std::optional<std::string> msg = c->udp_recv(node_id);
        if(static_cast<bool>(msg))
        {
            std::string s = msg.value();
            lua_pushstring(l, s.c_str());
            return 1;
        }
        return 0;
    }
    else
    {
        lua_pushinteger(l, -1);
        lua_pushstring(l, "The argument must be an integer");
        return 2;
    }
}

auto rudp_send(lua_State *l) -> int
{
    return 0;
}

auto rudp_recv(lua_State *l) -> int
{
    return 0;
}

//--------------------------------------------------------------memebers----------------------------------------------------------------

ZTLua::ZTLua(uint64_t nwid, int port) : 
    c(nwid, port)
{

}

ZTLua::~ZTLua()
{ }

auto ZTLua::register_wrappers(lua_State *l) -> void
{
    *static_cast<CommLayer **>(lua_getextraspace(l)) = &c;
    lua_pushcfunction(l, udp_send);
    lua_setglobal(l, "udp_send");
    lua_pushcfunction(l, udp_recv);
    lua_setglobal(l, "udp_recv");
    lua_pushcfunction(l, rudp_send);
    lua_setglobal(l, "rudp_send");
    lua_pushcfunction(l, rudp_recv);
    lua_setglobal(l, "rudp_recv");
}

} // namespace zt_lua