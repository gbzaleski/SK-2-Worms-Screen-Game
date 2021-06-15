#include "UDP_server.h"
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "game_constant.h"

// Comparators for struct sockaddr_in6.
bool operator<(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B)
{
    return 0 < memcmp(&A, &B, sizeof(A));
}
bool operator==(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B)
{
    return !(A < B) && !(B < A);
}
bool operator!=(const struct sockaddr_in6 &A, const struct sockaddr_in6 &B)
{
    return !(A == B);
}

// Setting up the port.
UDPServer::UDPServer(std::map<char, uint32_t> settings)
{
    port = settings[game_constant::PORT];
}

// Commencing connection.
void UDPServer::start()
{
    con_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (con_socket < 0)
    {
        throw UDPError("Error for UDP socket");
    }

    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(port);
    server_address.sin6_addr = in6addr_any;

    if (bind(con_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
    {
        throw UDPError("Error for UDP binding");
    }
}

// Obtain single datagram.
datagram_input UDPServer::receive_datagram()
{
    struct sockaddr_in6 client_address_temp;
    socklen_t rcva_len = sizeof(client_address_temp);
    int flags = 0;
    int len = recvfrom(con_socket, buffer, sizeof(buffer), flags,
                   (struct sockaddr *) &client_address_temp, &rcva_len);
    if (len < 0)
    {
        throw UDPError("Error on datagram from client socket");
    }
    datagram_input result;
    result.valid = true;
    size_t name_len = len - (sizeof(result.session_id) + sizeof(result.turn_direction)
                             + sizeof(result.next_expected_event_no));

    memcpy(&result.session_id, buffer, sizeof(result.session_id));
    memcpy(&result.turn_direction, buffer + sizeof(result.session_id), sizeof(result.turn_direction));
    memcpy(&result.next_expected_event_no, buffer + sizeof(result.session_id)
            + sizeof(result.turn_direction), sizeof(result.next_expected_event_no));

    result.player_name = std::string(buffer + len - name_len,  name_len);
    if (result.player_name.empty() == false
        && is_nick_fine(result.player_name) == false)
    {
        result.valid = false;
        return result;
    }

    result.session_id = be64toh(result.session_id);
    result.next_expected_event_no = ntohl(result.next_expected_event_no);

    std::lock_guard<std::mutex> lock(address_mutex);
    if (result.player_name.empty() == false) // Normal player.
    {
        client_adress[result.player_name] = client_address_temp;
    }
    else // Spectator.
    {
        empty_clients.insert(client_address_temp);
    }

    client_next_emit[client_address_temp] = result.next_expected_event_no;
    client_last_time[client_address_temp] = std::chrono::system_clock::now();

    return result;
}

// Sends events to every players and spectator.
void UDPServer::send_datagram(const std::vector<std::string> &messages, uint32_t game_id)
{
    const uint32_t game_id_htonled = htonl(game_id);

    std::lock_guard<std::mutex> lock(address_mutex);
    for (const auto &adress: client_adress)
    {
        uint32_t i = client_next_emit[adress.second];
        if (i >= messages.size() && messages.empty() == false)
            i = messages.size() - 1;

        while (i < messages.size())
        {
            std::string whole_message = "0000";
            memcpy(&whole_message[0], &game_id_htonled, sizeof(game_id_htonled));

            while (i < messages.size())
            {
                if (whole_message.size() + messages[i].size() <= game_constant::MAX_UDP_SIZE)
                {
                    whole_message += messages[i++];
                }
            }
            int flags = 0;
            int snd_len = sendto(con_socket, whole_message.c_str(), (size_t) whole_message.size(), flags,
                                 (struct sockaddr *) &(adress.second),
                                 (socklen_t) sizeof(adress.second));
            if (snd_len < 0)
                throw UDPError("Error on sending datagram to client socket.");
        }
    }

    for (const auto &adress: empty_clients)
    {
        uint32_t i = client_next_emit[adress];
        if (i >= messages.size() && messages.empty() == false)
            i = messages.size() - 1;

        while (i < messages.size())
        {
            std::string whole_message = "0000";
            memcpy(&whole_message[0], &game_id_htonled, sizeof(game_id_htonled));

            while (i < messages.size())
            {
                if (whole_message.size() + messages[i].size() <= game_constant::MAX_UDP_SIZE)
                {
                    whole_message += messages[i++];
                }
            }
            int flags = 0;
            int snd_len = sendto(con_socket, whole_message.c_str(), (size_t) whole_message.size(), flags,
                                 (struct sockaddr *) &(adress),
                                 (socklen_t) sizeof(adress));
            if (snd_len < 0)
                throw UDPError("Error on sending datagram to client socket.");
        }
    }
}

// Desctructor shuts down connection.
UDPServer::~UDPServer()
{
    close(con_socket);
}

// Get number of players.
size_t UDPServer::get_client_number()
{
    std::lock_guard<std::mutex> lock(address_mutex);
    return client_adress.size(); //+ empty_clients.size() - dead_clients.size();
}

// Get number of spectators.
size_t UDPServer::get_empty_number()
{
    std::lock_guard<std::mutex> lock(address_mutex);
    return empty_clients.size();
}

// Checks if any player is time-outed.
void UDPServer::check_sleepers()
{
    std::lock_guard<std::mutex> lock(address_mutex);
    for (const auto &client: empty_clients)
    {
        if ((std::chrono::system_clock::now() - client_last_time[client]).count() > game_constant::TIMEOUT_LENGTH_NS)
        {
            client_last_time.erase(client);
            empty_clients.erase(client);
        }
    }
    for (const auto &client: client_adress)
    {
        if ((std::chrono::system_clock::now() - client_last_time[client.second]).count() > game_constant::TIMEOUT_LENGTH_NS)
        {
            client_last_time.erase(client.second);
            client_adress.erase(client.first);
        }
    }
}
