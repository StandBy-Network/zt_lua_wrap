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
#include <fstream>
#include <map>

#include <signal.h>

#include <ZeroTierSockets.h>

#include <zt_lua_wrap.h>
#include <config_reader.h>

static int node_ready = false, network_ready = false;

constexpr uint16_t ZTS_PORT = 9994;

auto callback_func(void *message) -> void
{
    zts_callback_msg *msg = static_cast<zts_callback_msg *>(message);
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
        std::cout << "Network ready for IP4. We don't care, we wait for IPv6" << std::endl;
        //network_ready = true;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_READY_IP6)
    {
        std::cout << "Network ready for IP6" << std::endl;
        network_ready = true;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_NETWORK_REQ_CONFIG)
    {
        std::cout << "Requesting network configuration" << std::endl;
        return;
    }
    if(msg->eventCode == ZTS_EVENT_PEER_DIRECT)
    {
        std::cout << std::hex << msg->peer->address << " ready for Direct communication" << std::endl;
        return;
    }
}

auto ctrl_c_handler(int signal) -> void
{
    zt_lua::stop();
    zts_stop();
    exit(1);
}

auto main(int argc, char *argv[]) -> int
{
    if(argc != 2)
    {
        std::cerr << "Please specify the path to the config file as the only argument" << std::endl;
        return 1;
    }

    struct sigaction sig;
    sig.sa_handler = ctrl_c_handler;
    sigaction(SIGINT, &sig, nullptr);

    const char *conf_path = argv[1];
    std::ifstream conf(conf_path);

    if(conf.good())
    {
        std::map<std::string, std::string> conf_map = zt_lua::read_conf(conf);
        std::cout << "Starting" << std::endl;
        int err;
        if((err = zts_start("zt_runtime", callback_func, ZTS_PORT)) == ZTS_ERR_OK)
        {
            while(!node_ready) { zts_delay_ms(50); }

            std::cout << "Joining" << std::endl;
            uint64_t nwid = std::stoull(conf_map["network_id"], 0, 16);
            std::cout << "network_id: " << std::hex << nwid << std::endl;
            if((err = zts_join(nwid)) == ZTS_ERR_OK)
            {
                while(!network_ready) { zts_delay_ms(50); }

                zt_lua::start(nwid);

                lua_State *l = luaL_newstate();
                zt_lua::register_wrappers(l);
                luaL_openlibs(l);

                if(luaL_dofile(l, "lua.lua"))
                {
                    std::cerr << "Couldn't do lua file" << std::endl;
                }

                zt_lua::stop();
            }
            else
            {
                std::cerr << "Couldn't join the netowkr: err: " << err << " zts_errno: " << zts_errno << std::endl;
            }
            
            zts_stop();
        }
        else
        {
            std::cerr << "Couldn't start ZeroTier service: error: " << err << " zts_errno: " << zts_errno << std::endl;
        }
    }

    return 0;
}