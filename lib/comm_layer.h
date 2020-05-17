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

#ifndef _COMM_LAYER_H_
#define _COMM_LAYER_H_

#include <bits/stdint-uintn.h>
#include <thread>
#include <mutex>
#include <queue>
#include <map>

namespace standby_network
{

const unsigned int MSG_MAX_LENGTH = 10000;

using msg_map = std::map<uint64_t, std::queue<std::string>>;

class CommLayer
{
public:
    CommLayer(uint64_t network_id, int port = 9000);
    CommLayer(const CommLayer &) = delete;
    CommLayer(CommLayer &&move);
    ~CommLayer();

public:
    auto operator=(const CommLayer &) -> const CommLayer & = delete;
    auto operator=(CommLayer &&move) -> const CommLayer &;

public:
    auto udp_send(uint64_t node_id, const std::string &msg) -> int;
    auto udp_recv(uint64_t node_id) -> std::optional<std::string>;

    auto rudp_send(uint64_t node_id, const std::string &msg) -> int;
    auto rudp_recv(uint64_t node_id) -> int;

public:
    auto port() const -> int;
    auto nwid() const -> uint64_t;

private:
    auto udp_listener() -> void;
    auto rudp_listener() -> void;

private:
    auto push_msg(std::mutex &mutex, msg_map &map, const uint64_t node_id, const std::string &msg) -> void;
    auto pop_msg(std::mutex &mutex, msg_map &map, const uint64_t node_id) -> std::optional<std::string>;

private:
    bool run;

    const int PORT = 9000;
    uint64_t NWID;

    std::thread udp_thread;
    std::thread rudp_thread;

    std::mutex udp_msg_q_mutex;
    msg_map udp_msg_queue;

    std::mutex rudp_msg_q_mutex;
    msg_map rudp_msg_queue;

};

} // namespace standby_network

#endif // _COMM_LAYER_H_