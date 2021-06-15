#include "game.h"
#include "game_constant.h"
#include <algorithm>
#include <utility>
#include <iostream>
#include <cmath>
#include <cstring>

// Comparison for sorting players.
bool compare_worms(const worm &A, const worm &B)
{
    return A.player < B.player;
}

// Game settings.
Game::Game(std::map<char, uint32_t> settings, UDPServer &_server) : server(_server)
{
    game_id = 0;
    players_alive = 0;
    final_event = 0;
    width = settings[game_constant::BOARD_WIDTH];
    height = settings[game_constant::BOARD_HEIGHT];
    turning = settings[game_constant::TURNING];
}

// Adding new player (not if there are already too many).
void Game::add_player(std::string _player)
{
    if (_player.empty() == false
        && server.get_client_number() + server.get_empty_number() < game_constant::MAX_PLAYERS_NUMBER)
    {
        worm new_worm;
        new_worm.player = std::move(_player);
        new_worm.is_out = true;

        for (const auto &worm_unit: worm_status)
            if (worm_unit.player == new_worm.player)
                return;

        worm_status.push_back(new_worm);
    }
}

// Commencing game.
void Game::start(Randomiser &randomiser)
{
    game_id = randomiser.rand();
    sort(worm_status.begin(), worm_status.end(), compare_worms);

    size_t ind = 0;
    for (auto &worm_unit: worm_status)
    {
        worm_unit.x = game_constant::CENTRE + randomiser.rand() % width;
        worm_unit.y = game_constant::CENTRE + randomiser.rand() % height;
        worm_unit.angle = randomiser.rand() % game_constant::FULL_ROTATE;
        worm_unit.is_out = false;
        players_alive++;
        get_id[worm_unit.player] = ind++;
    }

    call_new_game();
    make_turn(true);
}

// New game event.
void Game::call_new_game()
{
    std::string message = "nono0maxxmaxy"; // event_no - event_type - maxx - maxy.
    message = message.substr(0, 13);
    const uint32_t event_no = htonl(0);
    const uint8_t type = 0;
    const uint32_t send_width  = htonl(width);
    const uint32_t send_height = htonl(height);
    memcpy(&message[0], &event_no, sizeof(event_no));
    memcpy(&message[0] + sizeof(event_no), &type, sizeof(type));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type), &send_width, sizeof(send_width));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type) + sizeof(send_width), &send_height, sizeof(send_height));
    for (const auto &worm_unit: worm_status)
        message += worm_unit.player + '\0';

    if (message.back() != '\0')
        message += '\0';

    const uint32_t len = htonl((uint32_t) message.size());
    message = "llll" + message + "cccc";
    memcpy(&message[0], &len, sizeof(len));

    const uint32_t crc32_value = htonl(crc32(message.c_str(), message.size() - sizeof(crc32_value)));
    memcpy(&message[0] + message.size() - sizeof(crc32_value), &crc32_value, sizeof(crc32_value));

    events_to_emit.push_back(message);
    server.send_datagram(events_to_emit, game_id);
}

// Eaten pixel event.
void Game::call_pixel(const pixel &p, uint8_t player_id)
{
    std::string message = "nono1pwwwwhhhh"; // event_no - event_type - player - x - y.
    message = message.substr(0, 14);
    const uint32_t event_no = htonl((uint32_t)events_to_emit.size());
    const uint8_t type = 1;
    const uint32_t send_x = htonl(p.x);
    const uint32_t send_y = htonl(p.y);
    memcpy(&message[0], &event_no, sizeof(event_no));
    memcpy(&message[0] + sizeof(event_no), &type, sizeof(type));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type), &player_id, sizeof(player_id));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type) + sizeof(player_id), &send_x, sizeof(send_x));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type) + sizeof(player_id) + sizeof(send_x), &send_y, sizeof(send_y));

    const uint32_t len = htonl((uint32_t) message.size());
    message = "llll" + message + "cccc";
    memcpy(&message[0], &len, sizeof(len));

    const uint32_t crc32_value = htonl(crc32(message.c_str(), message.size() - sizeof(crc32_value)));
    memcpy(&message[0] + message.size() - sizeof(crc32_value), &crc32_value, sizeof(crc32_value));

    events_to_emit.push_back(message);
}

// Player eliminated event.
void Game::call_eliminated(uint8_t player_id)
{
    std::string message = "nono2p"; // event_no - event_type - player
    message = message.substr(0, 6);
    const uint32_t event_no = htonl((uint32_t)events_to_emit.size());
    const uint8_t type = 2;

    memcpy(&message[0], &event_no, sizeof(event_no));
    memcpy(&message[0] + sizeof(event_no), &type, sizeof(type));
    memcpy(&message[0] + sizeof(event_no) + sizeof(type), &player_id, sizeof(player_id));

    const uint32_t len = htonl((uint32_t) message.size());
    message = "llll" + message + "cccc";
    memcpy(&message[0], &len, sizeof(len));

    const uint32_t crc32_value = htonl(crc32(message.c_str(), message.size() - sizeof(crc32_value)));
    memcpy(&message[0] + message.size() - sizeof(crc32_value), &crc32_value, sizeof(crc32_value));

    events_to_emit.push_back(message);
}

// Game over event.
void Game::call_game_over()
{
    std::string message = "nono3"; // event_no - event_type - player
    message = message.substr(0, 5);
    final_event = events_to_emit.size();
    const uint32_t event_no = htonl(final_event);
    const uint8_t type = 3;
    memcpy(&message[0], &event_no, sizeof(event_no));
    memcpy(&message[0] + sizeof(event_no), &type, sizeof(type));

    const uint32_t len = htonl((uint32_t) message.size());
    message = "llll" + message + "cccc";
    memcpy(&message[0], &len, sizeof(len));

    const uint32_t crc32_value = htonl(crc32(message.c_str(), message.size() - sizeof(crc32_value)));
    memcpy(&message[0] + message.size() - sizeof(crc32_value), &crc32_value, sizeof(crc32_value));

    events_to_emit.push_back(message);
}

// Updates player's direction.
void Game::set_direction(const std::string &player, uint8_t _direction)
{
    if (get_id.find(player) != get_id.end())
    {
        const auto id = get_id[player];
        if (worm_status[id].is_out == false)
            worm_status[id].direction = _direction;
    }
}

// Checks if player is out of board.
bool Game::is_outposition(const pixel &p) const
{
    return p.x + 1 == 0 || p.y + 1 == 0 || p.x == width || p.y == height;
}

// One turn of game.
bool Game::make_turn(bool first_iteration)
{
    server.check_sleepers();

    for (auto &worm_unit: worm_status)
    {
        if (worm_unit.is_out)
            continue;

        pixel last_pos(worm_unit.x, worm_unit.y);
        worm_unit.x += std::cos(game_constant::RADIAN_RATIO * worm_unit.angle);
        worm_unit.y += std::sin(game_constant::RADIAN_RATIO * worm_unit.angle);

        const auto direction = worm_unit.direction;
        worm_unit.angle -= direction == game_constant::LEFT_TURN ? turning : 0;
        worm_unit.angle += direction == game_constant::RIGHT_TURN ? turning : 0;
        worm_unit.angle %= game_constant::FULL_ROTATE;

        pixel new_pos(worm_unit.x, worm_unit.y);

        if (first_iteration)
            new_pos = last_pos;
        else if (new_pos == last_pos)
            continue;

        if (eaten_pixels.find(new_pos) != eaten_pixels.end())
        {
            worm_unit.is_out = true;
            call_eliminated(get_id[worm_unit.player]);
            players_alive--;
            if (players_alive == 1)
            {
                call_game_over();
                break;
            }
            continue;
        }

        if (is_outposition(new_pos))
        {
            worm_unit.is_out = true;
            call_eliminated(get_id[worm_unit.player]);
            players_alive--;
            if (players_alive == 1)
            {
                call_game_over();
                break;
            }
            continue;
        }

        call_pixel(new_pos, get_id[worm_unit.player]);
        eaten_pixels.insert(new_pos);
    }
    
    server.send_datagram(events_to_emit, game_id);
    return players_alive == 1;
}

// Utility function for last barrier.
uint32_t Game::get_final_event()
{
    return final_event;
}


