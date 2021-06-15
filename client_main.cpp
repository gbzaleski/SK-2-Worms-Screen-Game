#include <iostream>
#include <cstdint>
#include <unistd.h>
#include "game_constant.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <netinet/tcp.h>
#include <vector>
#include <algorithm>
#include <set>

// Auxiliary struct for holding game settings.
struct launch_settings
{
    std::string server_name;
    std::string player_name;
    size_t port{};
    std::string gui_server;
    size_t gui_port{};

    launch_settings() = default;

    launch_settings(std::string sn, std::string pn, size_t p, std::string gs, size_t gp)
    : server_name(std::move(sn)), player_name(std::move(pn)), port(p), gui_server(std::move(gs)), gui_port(gp) {};
};

// Analyses input arguments.
launch_settings get_game_settings(int argc, char *argv[])
{
    const char OPT_UNKNOWN_SIGN = '?';
    launch_settings result(argv[1],"", game_constant::DEFAULT_PORT,
                                    game_constant::DEFAULT_SERVER, game_constant::DEFAULT_GUI_PORT);

    argc--;
    for (int i = 0; i < argc; ++i)
        argv[i] = argv[i + 1];

    int opt;
    while ((opt = getopt(argc, argv, game_constant::PLAYER_OPTSTRING)) != -1)
    {
        switch(opt)
        {
            case game_constant::NAME_OF_PLAYER:
                result.player_name = optarg;
                if (result.player_name.empty() == false
                    && is_nick_fine(result.player_name) == false)
                    throw game_constant::WrongValueArgument{};

                break;

            case game_constant::PORT:
                if (is_integer(optarg) == false)
                    throw game_constant::NotNumberArgument{};
                result.port = atoi(optarg);
                break;

            case game_constant::GUI_SERVER:
                result.gui_server = optarg;
                break;

            case game_constant::GUI_PORT:
                if (is_integer(optarg) == false)
                    throw game_constant::NotNumberArgument{};

                result.gui_port = atoi(optarg);
                break;

            case OPT_UNKNOWN_SIGN:
                throw game_constant::WrongValueArgument{};
        }
    }

    // Checks if all arguments were processed.
    if (optind != argc)
    {
        throw game_constant::ArgumentException{};
    }

    return result;
}

// Global variables for storing player's and game's settings.
bool game_concluded;
uint64_t session_id;
uint8_t turn_direction;
uint32_t next_expected_event_no;
std::vector <std::string> get_player;
uint32_t game_width, game_height;
uint32_t current_game_id;
std::set <uint32_t> previous_game_id;

// TCP connection socket.
int tcp_sock;

// Analyses input from GUI.
void receive_from_GUI()
{
    std::string read_moves;
    size_t pos;
    char buffer[game_constant::BUFFER_SIZE];
    do
    {
        int flags = 0;
        int rcv_len = recv(tcp_sock, buffer, sizeof(buffer), flags);
        if (rcv_len < 0)
        {
            std::cerr << "GUI receive error" << std::endl;
            exit(EXIT_FAILURE);
        }

        read_moves += std::string(buffer, rcv_len);
        if ((pos = read_moves.rfind(game_constant::GUI_LEFT_TURN)) != std::string::npos)
        {
            turn_direction = game_constant::LEFT_TURN;
            read_moves = read_moves.substr(pos);
        }
        if ((pos = read_moves.rfind(game_constant::GUI_RIGHT_TURN)) != std::string::npos)
        {
            turn_direction = game_constant::RIGHT_TURN;
            read_moves = read_moves.substr(pos);
        }
        if ((pos = read_moves.rfind(game_constant::GUI_FORWARD)) != std::string::npos)
        {
            turn_direction = game_constant::FORWARD_TURN;
            read_moves = read_moves.substr(pos);
        }
        if ((pos = read_moves.rfind(game_constant::GUI_FORWARD2)) != std::string::npos)
        {
            turn_direction = game_constant::FORWARD_TURN;
            read_moves = read_moves.substr(pos);
        }
    } while(true);
}

// Send datagram with current status.
int sock;
void report_current_status(launch_settings settings)
{
    auto last_action = std::chrono::system_clock::now();
    size_t len;
    ssize_t snd_len;

    char *mess = (char*) malloc(game_constant::BUFFER_SIZE);
    while (true)
    {
        // Equal interval synchronisation.
        std::this_thread::sleep_for(std::chrono::nanoseconds
            (game_constant::INTEVAL_LENGTH_NS - (std::chrono::system_clock::now() - last_action).count()));
        last_action = std::chrono::system_clock::now();

        uint64_t a = htobe64(session_id);
        uint8_t b = turn_direction;
        uint32_t c = htonl(next_expected_event_no);
        len = sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(char) * settings.player_name.size();

        memcpy(mess, &a, sizeof(a));
        memcpy(mess + sizeof(a), &b, sizeof(b));
        memcpy(mess + sizeof(a) + sizeof(b), &c, sizeof(c));
        memcpy(mess + sizeof(a) + sizeof(b) + sizeof(c), settings.player_name.c_str(),
               sizeof(char) * settings.player_name.size());

        snd_len = write(sock, mess, len);
        if (snd_len != (ssize_t)len)
        {
            std::cerr << "Partial / failed write error." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

int parse_UDP(const std::string &status)
{
    uint32_t len, event_no, crc32value;
    uint8_t type;
    memcpy(&len, &status[0], sizeof(len));
    len = ntohl(len);
    memcpy(&event_no, &status[0] + sizeof(len), sizeof(event_no));
    memcpy(&type, &status[0] + sizeof(len) + sizeof(event_no), sizeof(type));
    memcpy(&crc32value, &status[0] + len + sizeof(event_no), sizeof(crc32value));
    event_no = ntohl(event_no);
    crc32value = ntohl(crc32value);
    uint32_t crc32_here = crc32(status.c_str(), status.size() - sizeof(crc32_here));

    if (crc32value != crc32_here)
    {
        // Ignore rest of datagram.
        return -2;
    }

    if (event_no != next_expected_event_no)
    {
        return -1;
    }

    if (type < 4)
    {
        if (type == 0) // Create new game.
        {
            uint32_t maxx, maxy;
            memcpy(&maxx, &status[0] + sizeof(len) + sizeof(type) + sizeof(event_no), sizeof(maxx));
            memcpy(&maxy, &status[0] + sizeof(len) + sizeof(type) + sizeof(event_no) + sizeof(maxx), sizeof(maxy));
            maxx = ntohl(maxx);
            maxy = ntohl(maxy);
            game_width = maxx;
            game_height = maxy;

            // Check if board size if fine.
            if (game_width < game_constant::MIN_WIDTH || game_width > game_constant::MAX_WIDTH
                || game_height < game_constant::MIN_HEIGHT || game_height > game_constant::MAX_HEIGHT)
            {
                std::cerr << "Wrong board size" << std::endl;
                exit(EXIT_FAILURE);
            }

            auto players = status.substr(sizeof(len) + sizeof(type) + sizeof(event_no) + sizeof(maxx) + sizeof(maxy));
            players = std::string(players.c_str(), players.size() - sizeof(crc32_here));
            for (auto &c: players)
                if (c == '\0')
                    c = ' ';

            while (players.back() == ' ')
                players = std::string(players.c_str(), players.size() - 1);

            get_player.clear();
            auto players_backup = players;
            char *ptr = strtok(&players[0], " ");
            while (ptr != NULL)
            {
                get_player.push_back(ptr);
                ptr = strtok(NULL, " ");
            }

            std::string message = "NEW_GAME " + std::to_string(maxx) + " " + std::to_string(maxy) + " " + players_backup;
            message += '\n';

            // Checks if players are fine;
            if (get_player.size() < 2 || get_player.size() > 25)
            {
                std::cerr << "Wrong number of players." << std::endl;
                exit(EXIT_FAILURE);
            }

            for (const auto &player: get_player)
            {
                if (is_nick_fine(player) == false)
                {
                    std::cerr << "Wrong name of player." << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            auto copy = get_player;
            std::sort(copy.begin(), copy.end());
            for (size_t i = 0; i < get_player.size(); ++i)
            {
                if (copy[i] != get_player[i])
                {
                    std::cerr << "Wrong order of players." << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            size_t snd_len = write(tcp_sock, message.c_str(), message.size());
            if (snd_len != message.size())
            {
                std::cerr << "New game write error." << std::endl;
                exit(EXIT_FAILURE);
            }

            next_expected_event_no++;
            return 0;
        }
        else if (type == 1) // New pixel.
        {
            uint8_t player_id;
            uint32_t posx, posy;
            memcpy(&player_id, &status[0] + sizeof(len) + sizeof(type)+ sizeof(event_no), sizeof(player_id));
            memcpy(&posx, &status[0] + sizeof(len) + sizeof(type) + sizeof(player_id) + sizeof(event_no), sizeof(posx));
            memcpy(&posy, &status[0] + sizeof(len) + sizeof(type) + sizeof(player_id) + sizeof(event_no) + sizeof(posx), sizeof(posy));
            posx = ntohl(posx);
            posy = ntohl(posy);

            // Checks if command is correct.
            if (player_id > get_player.size())
            {
                std::cerr << "Wrong player." << std::endl;
                exit(EXIT_FAILURE);
            }
            else if (posx >= game_width || posy >= game_height)
            {
                std::cerr << "Wrong pixel position." << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string message = "PIXEL " + std::to_string(posx) + " " + std::to_string(posy) + " " + get_player[player_id];
            message += '\n';

            size_t snd_len = write(tcp_sock, message.c_str(), message.size());
            if (snd_len != message.size())
            {
                std::cerr << "Pixel write error." << std::endl;
                exit(EXIT_FAILURE);
            }

            next_expected_event_no++;
            return 1;
        }
        else if (type == 2)
        {
            uint8_t player_id;
            memcpy(&player_id, &status[0] + sizeof(len) + sizeof(event_no) + sizeof(type), sizeof(player_id));
            std::string player = std::to_string(player_id);
            std::string message = "PLAYER_ELIMINATED " + get_player[player_id];
            message += '\n';
            size_t snd_len = write(tcp_sock, message.c_str(), message.size());
            if (snd_len != message.size())
            {
                std::cerr << "Player eliminated write error." << std::endl;
                exit(EXIT_FAILURE);
            }

            next_expected_event_no++;
            return 2;
        }
        else if (type == 3) // Game over.
        {
            return 3;
        }
    }
    return -1;
}

// Receives UDP datagrams from server.
void analyse_datagram()
{
    char buffer[game_constant::BUFFER_SIZE];

    // Waiting for game to start.
    while (game_concluded == true)
    {
        int flags = 0;
        int message_len = recv(sock, buffer, sizeof(buffer), flags);
        if (message_len < 0)
        {
            std::cerr << "Error on datagram from client socket." << std::endl;
            exit(EXIT_FAILURE);
        }
        else if ((size_t)message_len > game_constant::MAX_UDP_SIZE)
        {
            std::cerr << "Too big UDP message." << std::endl;
            exit(EXIT_FAILURE);
        }
        auto status = std::string(buffer, message_len);

        uint32_t game_id;
        memcpy(&game_id, &status[0], sizeof(game_id));
        game_id = ntohl(game_id);
        status = status.substr(sizeof(game_id));

        if (previous_game_id.find(game_id) == previous_game_id.end())
        {
            previous_game_id.insert(game_id);
            current_game_id = game_id;
            game_concluded = false;
            next_expected_event_no = 0;
        }

        while (status.empty() == false)
        {
            uint32_t len;
            memcpy(&len, &status[0], sizeof(len));
            len = ntohl(len);
            size_t package_len = sizeof(len) + len + sizeof(uint32_t);
            int resp = parse_UDP(status.substr(0, package_len));
            status = status.substr(package_len);

            // If crc value is wrong rest of datagram is ignored.
            if (resp == -2)
                break;
        }
    }

    // Game in progress.
    while (game_concluded == false)
    {
        int flags = 0;
        int message_len = recv(sock, buffer, sizeof(buffer), flags);
        if (message_len < 0)
        {
            std::cerr << "Error on datagram from client socket" << std::endl;
            exit(EXIT_FAILURE);
        }

        auto status = std::string(buffer, message_len);
        uint32_t game_id;
        memcpy(&game_id, &status[0], sizeof(game_id));
        game_id = ntohl(game_id);
        if (game_id != current_game_id)
            continue;

        status = status.substr(sizeof(game_id));

        while (status.empty() == false)
        {
            uint32_t len;
            memcpy(&len, &status[0], sizeof(len));
            len = ntohl(len);
            size_t package_len = sizeof(len) + len + sizeof(uint32_t);
            int resp = parse_UDP(status.substr(0, package_len));
            status = status.substr(package_len);

            // If crc value if wrong rest of datagram is ignored.
            if (resp == -2)
                break;

            if (resp == 3)
            {
                game_concluded = true;
                break;
            }
        }
    }
}

void set_up_TCP(launch_settings settings)
{
    int err;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    err = getaddrinfo(settings.gui_server.c_str(),
                      std::to_string(settings.gui_port).c_str(), &addr_hints, &addr_result);
    if (err == EAI_SYSTEM)
    {
        std::cerr << "Getaddrinfo: " <<  gai_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }
    else if (err != 0)
    {
        std::cerr << "getaddrinfo: " <<  gai_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialise socket according to getaddrinfo results.
    tcp_sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (tcp_sock < 0)
    {
        std::cerr << "Socket error" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Connect socket to the server.
    if (connect(tcp_sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
    {
        std::cerr << "Connect error" << std::endl;
        exit(EXIT_FAILURE);
    }

    int flag = 1;
    int result = setsockopt(tcp_sock,IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    if (result < 0)
    {
        std::cerr << "Disable Nagle error" << std::endl;
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(addr_result);
}

void set_up(launch_settings settings)
{
    // Converting host/port in string to struct addrinfo.
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;

    if (getaddrinfo(settings.server_name.c_str(), std::to_string(settings.port).c_str(), &addr_hints, &addr_result) != 0)
    {
        std::cerr << settings.server_name.c_str() << std::endl;
        std::cerr << "Getaddrinfo error." << std::endl;
        exit(EXIT_FAILURE);
    }

    sock = socket(addr_result->ai_family, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        std::cerr << "Socket error." << std::endl;
        exit(EXIT_FAILURE);
    }

    int con = connect(sock, addr_result->ai_addr, addr_result->ai_addrlen);
    if (con < 0)
    {
        std::cerr << "Connect error." << std::endl;
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(addr_result);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " game_server [-n player_name] [-p n] [-i gui_server] [-r n]" << std::endl;
        exit(EXIT_FAILURE);
    }

    launch_settings player_settings;
    try
    {
        player_settings = get_game_settings(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    set_up_TCP(player_settings);
    set_up(player_settings);
    std::thread GUI_receiver(receive_from_GUI);
    std::thread server_reporter(report_current_status, player_settings);
    session_id = std::chrono::duration_cast<std::chrono::microseconds>
            (std::chrono::system_clock::now().time_since_epoch()).count();
    next_expected_event_no = 0;

    while (true)
    {
        turn_direction = game_constant::FORWARD_TURN;
        game_concluded = true;
        get_player.clear();

        std::thread server_analyser(analyse_datagram);
        server_analyser.join();
    }
    // Client runs in loop.
}
