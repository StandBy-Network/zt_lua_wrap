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

#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include <iostream>
#include <sstream>

#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ZeroTierSockets.h>

namespace zt_lua
{

using msg_map_t = std::map<uint64_t, std::queue<std::string>>;

std::mutex msg_map_mut;
msg_map_t msg_map;

std::thread thread;

constexpr int MSG_MAX_LENGTH = 10000;

constexpr int PORT = 9000;

bool run = false;

uint64_t network_id;

auto register_wrappers(lua_State *l) -> void
{
    lua_pushcfunction(l, send);
    lua_setglobal(l, "send");
    lua_pushcfunction(l, recv);
    lua_setglobal(l, "recv");
}

auto create_sock() -> int
{
    return zts_socket(ZTS_PF_INET6, ZTS_SOCK_DGRAM, 0);
}

auto push_msg(const uint64_t node_id, const std::string &msg) -> void
{
    std::lock_guard<std::mutex> lock(msg_map_mut);
    msg_map[node_id].push(msg);
}

auto pop_msg(uint64_t node_id) -> std::optional<std::string>
{
    std::lock_guard<std::mutex> lock(msg_map_mut);
    try
    {
        auto q = msg_map.at(node_id);
        if(!q.empty())
        {
            const std::string s = q.front();
            q.pop();
            return s;
        }
        return std::nullopt;
    }
    catch(const std::out_of_range &)
    {
        return std::nullopt;
    }
}

auto id_to_addr_str(uint64_t node_id) -> std::string
{
    std::stringstream ss;
    ss << "fd" << std::hex << network_id << "9993" << node_id;
    std::string out;
    for(int i = 0; i < ss.str().length(); i++)
    {
        out.push_back(ss.str()[i]);
        if(i % 4 == 3 && i < ss.str().length() - 1)
        {
            out.push_back(':');
        }
    }

    return out;
}

auto addr_str_to_id(const std::string addr) -> uint64_t
{
    std::string no_colons;
    for(int i = 0; i < addr.length(); i++)
    {
        if(addr[i] != ':')
        {
            no_colons.push_back(addr[i]);
        }
    }

    std::string id_string;
    for(int i = 22; i < no_colons.length(); i++)
    {
        id_string.push_back(no_colons[i]);
    }

    return std::stoull(id_string, 0, 16);
}

auto addr_to_string(zts_sockaddr_in6 *addr) -> std::optional<std::string>
{
    char ad[ZTS_INET6_ADDRSTRLEN];
    if(zts_inet_ntop(ZTS_AF_INET6, &(addr->sin6_addr), ad, ZTS_INET6_ADDRSTRLEN))
    {
        std::string s(ad);

        return s;
    }
    return std::nullopt;
}

auto string_to_addr(const std::string str) -> std::optional<zts_sockaddr_in6>
{
    zts_sockaddr_in6 addr;
    if(zts_inet_pton(ZTS_AF_INET6, str.c_str(), &addr.sin6_addr) > 0)
    {
        addr.sin6_family = ZTS_AF_INET6;
        return addr;
    }
    
    return std::nullopt;
    
}

auto lstring_to_string(const char *lstr, int len) -> const std::string
{
    std::string ret;
    for(int i = 0; i < len; i++)
    {
        ret.push_back(lstr[i]);
    }

    return ret;
}

auto listener() -> void
{
    // This lock in my opinion/theoretically guarantees, that no one is going to be able to manipulate the 
    // threads vector until this function returns, if they use the mutex anyway, and that means, until 
    // zt_lua::stop() is called;
    int recv_fd;
    if((recv_fd = create_sock()) < 0)
    {
        std::cerr << "Couldn't create socket to listen on: err: " << recv_fd << " zts_errno: " << zts_errno << std::endl;
        return;
    }

    int err;
    if((err = zts_fcntl(recv_fd, ZTS_F_SETFL, ZTS_O_NONBLOCK)) < 0)
    {
        std::cerr << "Couldn't set socket to be non blocking: err: " << err << " zts_errno: " << zts_errno << std::endl;
        zts_close(recv_fd);
        return;
    }

    auto opt = string_to_addr("::");
    if(static_cast<bool>(opt))
    {
        zts_sockaddr_in6 addr = opt.value();
        addr.sin6_port = zts_htons(PORT);
        if((err = zts_bind(recv_fd, (const zts_sockaddr *)&addr, sizeof(addr))) < 0)
        {
            std::cerr << "Couldn't bind to \"any_address\": err: " << err << " zts_errno: " << zts_errno << std::endl;
            zts_close(recv_fd);
            return;
        }

        while(run)
        {
            char msg[MSG_MAX_LENGTH];
            int recvd;
            zts_sockaddr_in6 recv_addr;
            zts_socklen_t recv_addr_len = sizeof(recv_addr);
            if((recvd = zts_recvfrom(recv_fd, msg, MSG_MAX_LENGTH, 0, (zts_sockaddr *)&recv_addr, &recv_addr_len)) > 0)
            {
                auto maybe = addr_to_string(&recv_addr);
                if(static_cast<bool>(maybe))
                {
                    std::string s = maybe.value();
                    push_msg(addr_str_to_id(s), lstring_to_string(msg, recvd));
                }
                else
                {
                    std::cerr << "Wrong address from zts_accept or addr_to_string is malfuncioning" << std::endl;
                }
            };
        }
    }
    else
    {
        std::cerr << "Wrong address string entered" << std::endl;
    }
    
    zts_close(recv_fd);
}

auto start(uint64_t nwid) -> void
{
    if(!run)
    {
        network_id = nwid;
        run = true;
        thread = std::thread(listener);
    }
}

auto stop() -> void
{
    if(run)
    {
        run = false;
        if(thread.joinable())
        {
            thread.join();
        }
    }
}

auto send(lua_State *l) -> int
{
    if(lua_isinteger(l, -2))
    {
        if(lua_isstring(l, -1))
        {
            uint64_t node_id = lua_tointeger(l, -2);
            std::string msg = lua_tostring(l, -1);
            // create fd
            int fd;
            if((fd = create_sock()) < 0)
            {
                lua_pushinteger(l, -1);
                lua_pushstring(l, "Couldn't create socket for sending");
                return 2;
            }
            int max_tries = 10;
            int tries = 0;
            int err;
            /* zts_sockaddr_storage test;
            if((err = zts_get_rfc4193_addr(&test, network_id, node_id)) != ZTS_ERR_OK)
            {
                lua_pushinteger(l, -1);
                lua_pushstring(l, "Couldn't get address based on node id");
                return 2;
            } */
            auto opt = string_to_addr(id_to_addr_str(node_id));
            if(static_cast<bool>(opt))
            {
                zts_sockaddr_in6 addr = opt.value();
                addr.sin6_port = zts_htons(PORT);
                // send
                int sent;
                if((sent = zts_sendto(fd, msg.c_str(), msg.length(), 0, (const zts_sockaddr *)&addr, sizeof(addr))) < 0)
                {
                    lua_pushinteger(l, -1);
                    lua_pushstring(l, "Couldn't send any data");
                    return 2;
                }
                // close fd
                zts_close(fd);
                // return bytes sent
                lua_pushinteger(l, sent);
                return 1;
            }
            lua_pushinteger(l, -1);
            lua_pushstring(l, "id_to_addr_str or string_to_addr is malfunctioning");
            return 2;
        }
        else
        {
            lua_pushinteger(l, -1);
            lua_pushstring(l, "The second argument has to be a string");
            return 2;
        }
    }
    else
    {
        lua_pushinteger(l, -1);
        lua_pushstring(l, "The first argument has to be a valid node_id of type integer");
        return 2;
    }
}

auto send_many(lua_State *l) -> int
{
    return 0;
}

auto recv(lua_State *l) -> int
{
    if(lua_isinteger(l, -1))
    {
        // return message from the queue
        uint64_t node_id = lua_tointeger(l, -1);
        auto opt = pop_msg(node_id);
        if(static_cast<bool>(opt))
        {
            std::string s = opt.value();
            lua_pushinteger(l, s.length());
            lua_pushlstring(l, s.c_str(), s.length());
            return 2;
        }
        lua_pushinteger(l, -1);
        lua_pushstring(l, "No message in queue");
        return 2;
    }
    else
    {
        lua_pushinteger(l, -1);
        lua_pushstring(l, "The argument has to be a valid node_id of type integer");
        return 2;
    }
    
}

} // namespace zt_lua