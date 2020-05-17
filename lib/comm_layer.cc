#include <comm_layer.h>

#include <iostream>
#include <sstream>

#include <ZeroTierSockets.h>

namespace standby_network
{

//--------------------------------------------------------------helpers-----------------------------------------------------------------

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

auto id_to_addr_str(const CommLayer &c, uint64_t node_id) -> std::string
{
    std::stringstream ss;
    ss << "fd" << std::hex << c.nwid() << "9993" << node_id;
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
    constexpr int hexa_digit_count = 10;
    constexpr int max_zeroes_to_fill = 3;
    std::string num_str("");
    int position_in_addr = addr.length() - 1;
    /**
     * The following is a simple state machine. States below.
     * We don't know, how many digits are between the colons in the address, because maximally
     * 1 zero can be present between any two colons. The statemachine works as follows:
     * We need exactly 10 digits in a string so we repeat until the length becomes equal to 10.
     * We keep track of our current position inside the addr string starting at the end and decrementing it everytime we get the
     * current character from the string and we decrement it in state 3 if the current character isn't a colon to make sure state 0
     * could see only digits.
     * In any state (except 0; in that state we have only 1 option: insert the current digit and continue to next state)
     * we have 2 options:
     *   1. we see a colon -> we know that we need more digits, so we have to fill in zeroes and continue to the next state
     *   2. we see a digit -> we fill in the digit and continue to the next state
     * The next state depends on how many more digits we need totally and in the current 4 digit group:
     *   1. if we are in any state and we had to fill in zeroes or the state is 3 (usual boolean operator precedence) ->
     *     a) if we need 2 more digits to complete the string
     *       we set the state to 2 (meaning 2 more digits to come before a colon (which is a lie in this case, because
     *       no colon is coming, but we are going to finish the while cycle anyways))
     *     b) otherwise
     *       we set the state to 0
     *   2. if we are in any state except 3 -> 
     *     we continue to the next state
     */ 
    // states: 0 -> 4 more digits needed before the next colon comes, 1 -> 3 more digits ..., 2 -> 2 more, 3 -> 1 more
    int state = 0;
    while(num_str.length() != 10)
    {
        if(!state)
        {
            num_str = addr[position_in_addr--] + num_str;
            state++;
            continue;
        }
        char c;
        if((c = addr[position_in_addr--]) == ':')
        {
            for(int i = 0; i < max_zeroes_to_fill - state; i++)
            {
                num_str = "0" + num_str;
            }
            if(num_str.length() == 8)
            {
                state = 2;
            }
            else
            {
                state = 0;
            }
            continue;
        }
        else
        {
            num_str = /* don't ask, I don't get it */ (char)c + num_str;
            if(state == 3)
            {
                if(num_str.length() == 8)
                {
                    state = 2;
                }
                else
                {
                    state = 0;
                }
                position_in_addr--;
                continue;
            }
            else
            {
                state++;
            }
        }
    }
    return std::stoull(num_str, 0, 16);
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

auto create_sock() -> int
{
    return zts_socket(ZTS_PF_INET6, ZTS_SOCK_DGRAM, 0);
}

//--------------------------------------------------------------members-----------------------------------------------------------------

CommLayer::CommLayer(uint64_t nwid, int port) : 
    run(true),
    PORT(port),
    NWID(nwid),
    udp_thread(&CommLayer::udp_listener, this),
    rudp_thread(&CommLayer::rudp_listener, this)
{
    
}

CommLayer::CommLayer(CommLayer &&move)
{
    *this = std::move(move);
}

CommLayer::~CommLayer()
{
    run = false;
    if(udp_thread.joinable())
    {
        udp_thread.join();
    }
    if(rudp_thread.joinable())
    {
        rudp_thread.join();
    }
}

const CommLayer & CommLayer::operator=(CommLayer &&move)
{
    std::swap(run, move.run);
    int *p = const_cast<int *>(&PORT);
    *p = move.PORT;
    uint64_t *n = const_cast<uint64_t *>(&NWID);
    *n = move.NWID;

    std::swap(udp_thread, move.udp_thread);
    std::swap(rudp_thread, move.rudp_thread);

    udp_msg_q_mutex.lock();
    move.udp_msg_q_mutex.lock();
    std::swap(udp_msg_queue, move.udp_msg_queue);
    move.udp_msg_q_mutex.unlock();
    udp_msg_q_mutex.unlock();

    rudp_msg_q_mutex.lock();
    move.rudp_msg_q_mutex.lock();
    std::swap(rudp_msg_queue, move.rudp_msg_queue);
    move.rudp_msg_q_mutex.unlock();
    rudp_msg_q_mutex.unlock();

    return *this;
}

auto CommLayer::udp_send(uint64_t node_id, const std::string &msg) -> int
{
    // create fd
    int fd;
    if((fd = create_sock()) < 0)
    {
        std::cerr << "Couldn't create UDP socket. err: " << fd << " errno: " << zts_errno << std::endl;
        return zts_errno;
    }
    int max_tries = 10;
    int tries = 0;
    int err;
    auto opt = string_to_addr(id_to_addr_str(*this, node_id));
    if(static_cast<bool>(opt))
    {
        zts_sockaddr_in6 addr = opt.value();
        addr.sin6_port = zts_htons(PORT);
        // send
        if((err = zts_sendto(fd, msg.c_str(), msg.length(), 0, (const zts_sockaddr *)&addr, sizeof(addr))) < 0)
        {
            std::cout << "Couldn't send any data. err: " << err << " errno: " << zts_errno << std::endl;
            err = zts_errno;
        }
    }
    else
    {
        err = -1;
        std::cerr << "An address-conversion error occurred" << std::endl;
    }
    // close fd
    zts_close(fd);

    return err;
}

auto CommLayer::udp_recv(uint64_t node_id) -> std::optional<std::string>
{
    // return message from the queue
    auto opt = pop_msg(udp_msg_q_mutex, udp_msg_queue, node_id);
    if(static_cast<bool>(opt))
    {
        return opt.value();
    }
    return std::nullopt;
}

auto CommLayer::rudp_send(uint64_t node_id, const std::string &msg) -> int
{
    return 0;
}

auto CommLayer::rudp_recv(uint64_t node_id) -> int
{
    return 0;
}

auto CommLayer::port() const -> int
{
    return PORT;
}

auto CommLayer::nwid() const -> uint64_t
{
    return NWID;
}

auto CommLayer::udp_listener() -> void
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
        addr.sin6_port = zts_htons(port());
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
                if(maybe)
                {
                    std::string s = maybe.value();
                    push_msg(udp_msg_q_mutex, udp_msg_queue, addr_str_to_id(s), lstring_to_string(msg, recvd));
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

auto CommLayer::rudp_listener() -> void
{

}

auto CommLayer::push_msg(std::mutex &mutex, msg_map &map, const uint64_t node_id, const std::string &msg) -> void
{
    std::lock_guard<std::mutex> lock(mutex);
    map[node_id].push(msg);
}

auto CommLayer::pop_msg(std::mutex &mutex, msg_map &map, uint64_t node_id) -> std::optional<std::string>
{
    std::lock_guard<std::mutex> lock(mutex);
    try
    {
        auto q = map.at(node_id);
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

} // namespace standby_network