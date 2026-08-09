#pragma once

#include <string>
#include <vector>

#include "tennis_watch/core/point.hpp"


std::vector<Point> parse_csv(const std::string& filepath);
