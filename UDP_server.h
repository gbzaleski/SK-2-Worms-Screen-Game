
#ifndef ROBALETHEGAME_UDP_SERVER_H
#define ROBALETHEGAME_UDP_SERVER_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdexcept>
#include <map>
#include <cstdint>
#include <vector>
#include <chrono>
#include <set>
#include <cstring>
#include <mutex>
#include "game_constant.h"

class UDPError: public std::runtime_error
{
    public:
    UDPError(const char *w) : std::runtime_error(w) {}
};

// Comparators for struct sockaddr_in6.
bool operator<(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B);
bool operator==(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B);
bool operator!=(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B);

struct datagram_input
{
    uint64_t session_id;
    uint8_t turn_direction;
    uint32_t next_expected_event_no;
    std::string player_name;
    bool valid;
};

class UDPServer
{
    public:
    UDPServer() = delete;

    explicit UDPServer(std::map<char, uint32_t>);

    // Copy and move semantics are disabled.
    UDPServer (const UDPServer &) = delete;
    UDPServer &operator=(const UDPServer &) = delete;

    void start();

    datagram_input receive_datagram();

    void send_datagram(const std::vector<std::string> &, uint32_t);

    size_t get_client_number();

    size_t get_empty_number();

    void check_sleepers();

    ~UDPServer();

    private:
    int con_socket;
    uint32_t port;
    struct sockaddr_in6 server_address;
    std::map<std::string, struct sockaddr_in6> client_adress;
    std::set<struct sockaddr_in6> empty_clients;
    std::mutex address_mutex;
    std::map<struct sockaddr_in6, std::chrono::time_point<std::chrono::system_clock>> client_last_time;
    std::map<struct sockaddr_in6, uint32_t> client_next_emit;
    char buffer[game_constant::BUFFER_SIZE];
};

#endif //ROBALETHEGAME_UDP_SERVER_H
