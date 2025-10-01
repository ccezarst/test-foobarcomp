#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace foo_apple_music {

struct track_stream {
    std::string url;
    std::string codec;
    std::chrono::seconds expires_in{0};
};

struct track {
    std::string id;
    std::string name;
    std::string artist_name;
    std::string album_name;
    std::string artwork_url;
    int disc_number = 1;
    int track_number = 0;
    double duration_seconds = 0.0;
    std::optional<track_stream> stream;
};

struct playlist {
    std::string id;
    std::string name;
    std::string description;
    std::vector<track> tracks;
    std::chrono::system_clock::time_point last_modified;
};

} // namespace foo_apple_music
