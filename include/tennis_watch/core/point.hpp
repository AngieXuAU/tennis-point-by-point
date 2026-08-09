#pragma once
#include <chrono>
#include <cstdint>
#include <string>

enum class PointOutcome : std::uint8_t {
    Ace,
    DoubleFault,
    Winner,
    UFE,  // unforced error
    FE,   // forced error
    Routine
};

struct Point {
    // match context
    std::string match_id;
    int point_number;
    int set_number;
    int game_number;

    // scoreboard state
    int server;
    std::string p1_score;
    std::string p2_score;
    int p1_games_won;
    int p2_games_won;
    std::chrono::seconds time;  // time since start of first point

    // point outcome and info
    int point_winner;
    PointOutcome outcome;
    int rally_length;
    int serve_speed_kph;

    // serve info
    bool first_serve_in;
    bool second_serve_in;

    // key point info
    bool p1_break;
    bool p2_break;

    bool p1_net;
    bool p2_net;
};