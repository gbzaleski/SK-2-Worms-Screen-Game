#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <map>
#include <thread>
#include "game_constant.h"
#include "UDP_server.h"
#include "game.h"
#include "randomiser.h"

// Analyses input arguments.
std::map<char, uint32_t> get_game_settings(int argc, char *argv[])
{
    const char OPT_UNKNOWN_SIGN = '?';
    auto game_settings(game_constant::DEFAULT_GAME_SETTINGS);

    int opt;
    while ((opt = getopt(argc, argv, game_constant::SERVER_OPTSTRING)) != -1)
    {
        if (is_integer(optarg) == false)
            throw game_constant::NotNumberArgument{};

        int64_t check_value = atol(optarg);
        if (check_value < 0 || check_value > (int64_t)std::numeric_limits<u_int32_t>::max())
            throw game_constant::WrongValueArgument{};

        uint32_t argvalue = check_value;

        switch(opt)
        {
            case game_constant::PORT:
                if (game_constant::MIN_PORT <= argvalue
                    && argvalue <= game_constant::MAX_PORT)
                    game_settings[game_constant::PORT] = argvalue;
                else
                    throw game_constant::WrongValueArgument{};
                break;

            case game_constant::SEED:
                game_settings[game_constant::SEED] = argvalue;

                break;

            case game_constant::TURNING:
                if (game_constant::MIN_TURNING <= argvalue
                    && argvalue <= game_constant::MAX_TURNING)
                    game_settings[game_constant::TURNING] = argvalue;
                else
                    throw game_constant::WrongValueArgument{};
                break;

            case game_constant::VELOCITY:
                if (game_constant::MIN_VELOCITY <= argvalue
                    && argvalue <= game_constant::MAX_VELOCITY)
                    game_settings[game_constant::VELOCITY] = argvalue;
                else
                    throw game_constant::WrongValueArgument{};
                break;

            case game_constant::BOARD_WIDTH:
                if (game_constant::MIN_WIDTH <= argvalue
                    && argvalue <= game_constant::MAX_WIDTH)
                    game_settings[game_constant::BOARD_WIDTH] = argvalue;
                else
                    throw game_constant::WrongValueArgument{};
                break;

            case game_constant::BOARD_HEIGHT:
                if (game_constant::MIN_HEIGHT <= argvalue
                   && argvalue <= game_constant::MAX_HEIGHT)
                game_settings[game_constant::BOARD_HEIGHT] = argvalue;
                else
                    throw game_constant::WrongValueArgument{};
                break;

            case OPT_UNKNOWN_SIGN:
                throw game_constant::WrongValueArgument{};
        }
    }

    // Checks if all arguments were processed.
    if (optind < argc)
    {
        throw game_constant::ArgumentException{};
    }

    return game_settings;
}

// Global variable for game status.
bool game_concluded;

// Receives and analyses datagram from players.
void receive_datagrams(UDPServer &server, Game &game, Randomiser &randomiser)
{
    std::set<std::string> ready_players;
    const std::string tab[] = {"FORWARD", "RIGHT", "LEFT"};

    while (game_concluded == true
        && ready_players.size() < std::max((size_t) 2, server.get_client_number()))
    {
        auto datagram = server.receive_datagram();
        if (datagram.valid == false)
            continue;
        game.add_player(datagram.player_name);

        if (datagram.player_name.empty() == false
            && (datagram.turn_direction == game_constant::RIGHT_TURN
                 || datagram.turn_direction == game_constant::LEFT_TURN))
            ready_players.insert(datagram.player_name);
    }
    auto players_threshold = server.get_client_number();
    game.start(randomiser);
    game_concluded = false;

    while (game_concluded == false)
    {
        auto datagram = server.receive_datagram();
        if (datagram.valid == false)
            continue;

        game.add_player(datagram.player_name);
        game.set_direction(datagram.player_name, datagram.turn_direction);
    }

    std::set<std::string> finish_players;
    while (game_concluded == true && finish_players.size() < players_threshold)
    {
        auto datagram = server.receive_datagram();
        if (datagram.valid == false)
            continue;

        if (ready_players.find(datagram.player_name) == ready_players.end())
        {
            ready_players.insert(datagram.player_name);
            game.add_player(datagram.player_name);
        }

        if (datagram.player_name.empty() == false
            && datagram.next_expected_event_no == game.get_final_event())
            finish_players.insert(datagram.player_name);
    }
}

// Moves players in equal time intervals with regard to their angle and position.
void make_turns(Game &game, std::map<char, uint32_t> settings, UDPServer &server)
{
    auto last_action = std::chrono::system_clock::now();
    const int64_t interval = int64_t(1e9) / settings[game_constant::VELOCITY];

    while (game_concluded)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server.check_sleepers();
    }

    while (game_concluded == false)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds
                (interval - (std::chrono::system_clock::now() - last_action).count()));
        last_action = std::chrono::system_clock::now();

        game_concluded = game.make_turn();
    }
}

int main(int argc, char *argv[])
{
    std::map<char, uint32_t> game_settings;

    try
    {
        game_settings = get_game_settings(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    Randomiser randomiser(game_settings[game_constant::SEED]);
    UDPServer server(game_settings);
    try
    {
        server.start();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    try
    {
        while (true)
        {
            Game game(game_settings, server);
            game_concluded = true;

            std::thread datagram_receiver(receive_datagrams,
                  std::ref(server), std::ref(game), std::ref(randomiser));

            std::thread turns_maker(make_turns,
                  std::ref(game), game_settings, std::ref(server));

            turns_maker.join();
            datagram_receiver.join();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Server starts new games in loop.
}


