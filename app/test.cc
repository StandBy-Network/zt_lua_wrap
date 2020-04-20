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

#include <iostream>

#include <unistd.h>
#include <signal.h>

#include <arpa/inet.h>
#include <ZeroTier.h>

#include <zt_lua_wrap.h>

static int node_ready = false, network_ready = false;

auto callback_func(zts_callback_msg *msg)
{
    if(msg->eventCode == ZTS_EVENT_NODE_ONLINE)
    {
        std::cout << "Node " << std::hex << msg->node->address << " is online" << std::endl;
        node_ready = true;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NODE_UNRECOVERABLE_ERROR)
    {
        std::cout << "Node_unrecoverable_error occured" << std::endl;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_OK)
    {
        std::cout << "Network " << std::hex << msg->network->nwid << " OK" << std::endl;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_READY_IP4)
    {
        std::cout << "Network ready for IP4" << std::endl;
        network_ready = true;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_READY_IP6)
    {
        std::cout << "Network ready for IP6" << std::endl;
        network_ready = true;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_REQUESTING_CONFIG)
    {
        std::cout << "Requesting network configuration" << std::endl;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_PEER_P2P)
    {
        std::cout << std::hex << msg->peer->address << " ready for P2P" << std::endl;
        return;
    }
}

namespace
{

static constexpr int PORT = 9000;

}

auto ctrl_c_handler(int signal) -> void
{
    zts_stop();
    exit(1);
}

auto main() -> int
{
    struct sigaction sig;
    sig.sa_handler = ctrl_c_handler;
    sigaction(SIGINT, &sig, nullptr);

    std::cout << "Starting" << std::endl;
    zts_start("zt_runtime", callback_func, ZTS_DEFAULT_PORT);
    while(!node_ready) { sleep(1); }

    std::cout << "Joining" << std::endl;
    uint64_t nwid = 0x93afae59635ebb07;
    zts_join(nwid);
    while(!network_ready) { sleep(1); }
    

    zt_lua::start();

    lua_State *l = luaL_newstate();
    zt_lua::register_wrappers(l);
    luaL_openlibs(l);

    if(luaL_dofile(l, "lua.lua"))
    {
        std::cerr << "Couldn't do lua file" << std::endl;
    }

    zt_lua::stop();
    zts_stop();

    return 0;
}