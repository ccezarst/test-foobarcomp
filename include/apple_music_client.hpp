#pragma once

#include "apple_music_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace foo_apple_music {

class apple_music_client {
public:
    apple_music_client();

    void set_tokens(std::string developer_token, std::string music_user_token);

    [[nodiscard]] std::vector<playlist> fetch_playlists();
    [[nodiscard]] std::optional<track> fetch_track(const std::string& track_id);
    [[nodiscard]] std::optional<track_stream> fetch_stream(const std::string& track_id);

private:
    std::string m_developer_token;
    std::string m_music_user_token;

    [[nodiscard]] std::string build_authorization_header() const;
};

} // namespace foo_apple_music
