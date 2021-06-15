//
// Created by gbz on 14.05.2021.
//

#ifndef ROBALETHEGAME_GAME_H
#define ROBALETHEGAME_GAME_H
#include <map>
#include <cstdint>
#include <vector>
#include <set>
#include <unordered_set>
#include "game_constant.h"
#include "randomiser.h"
#include "UDP_server.h"

class Game
{
    public:
    Game() = delete;

    Game(std::map<char, uint32_t>, UDPServer &);

    void add_player(std::string);

    void start(Randomiser &);

    bool make_turn(bool = false);

    void set_direction(const std::string &player, uint8_t turn);

    uint32_t get_final_event();

    private:
    uint32_t width;
    uint32_t height;
    uint32_t turning;
    uint32_t game_id;
    std::map<std::string, size_t> get_id;
    std::vector<worm> worm_status;
    std::set<pixel> eaten_pixels;
    uint32_t players_alive;
    UDPServer &server;
    std::vector<std::string> events_to_emit;
    uint32_t final_event;

    [[nodiscard]] bool is_outposition(const pixel &p) const;

    void call_new_game();

    void call_pixel(const pixel &p, uint8_t);

    void call_eliminated(uint8_t);

    void call_game_over();
};

#endif //ROBALETHEGAME_GAME_H
