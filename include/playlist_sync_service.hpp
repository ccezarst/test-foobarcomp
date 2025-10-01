#pragma once

#include "apple_music_client.hpp"

#include <memory>

namespace foo_apple_music {

class playlist_sync_service {
public:
    playlist_sync_service();

    void synchronize();

private:
    std::unique_ptr<apple_music_client> m_client;
    void ensure_tokens();
};

} // namespace foo_apple_music
