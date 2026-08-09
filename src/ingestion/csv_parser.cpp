#include "tennis_watch/ingestion/csv_parser.hpp"

// add all this to csv_parser.hpp
#include <charconv>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string_view>


std::vector<Point> parse_csv(const std::string& filepath) {
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: could not open file.\n";
        return {};
    }

    std::vector<Point> points;  // initialise vector of points
    std::string line;
    std::getline(file, line);  // skip header
    std::getline(file, line);  // skip first line (point '0')

    while (std::getline(file, line)) {
        // skip empty lines
        if (line.empty()) {
            continue;
        }

        Point point;

        auto split_view = line | std::views::split(',');
        auto it = split_view.begin();
        auto end = split_view.end();

        // access the next wanted column
        auto get_wanted_col = [&it, end](int jump) -> std::string_view {
            if (it == end) return {};

            for (int i = 0; i < jump; i++) ++it;

            std::string_view val(*it);
            return val;
        };

        // match id
        std::string_view match_id_sv = get_wanted_col(0);

        // time
        std::string_view time_sv = get_wanted_col(1);
    }

    return points;
}
