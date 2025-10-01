#include "playlist_sync_service.hpp"
#include "apple_music_auth.hpp"

#include <foobar2000/SDK/playlist_manager.h>
#include <foobar2000/SDK/foobar2000.h>
#include <foobar2000/helpers/playlist_lock.h>

#include <pfc/list.h>

using namespace foo_apple_music;

playlist_sync_service::playlist_sync_service() : m_client(std::make_unique<apple_music_client>()) {}

void playlist_sync_service::ensure_tokens() {
    const auto tokens = apple_music_auth_manager::instance().tokens();
    if (!tokens) {
        throw exception_io_data();
    }
    m_client->set_tokens(tokens->developer_token, tokens->music_user_token);
}

void playlist_sync_service::synchronize() {
    ensure_tokens();

    auto playlists = m_client->fetch_playlists();
    auto playlist_api = playlist_manager::get();

    in_playlist_switcher switcher(*playlist_api);

    for (const auto& pl : playlists) {
        pfc::string8 playlist_name;
        playlist_name << " " << pl.name.c_str();

        t_size index = pfc_infinite;
        const t_size playlist_count = playlist_api->get_playlist_count();
        for (t_size i = 0; i < playlist_count; ++i) {
            if (playlist_api->playlist_get_name(i) == playlist_name) {
                index = i;
                break;
            }
        }

        if (index == pfc_infinite) {
            index = playlist_api->create_playlist(playlist_count, playlist_name.get_ptr());
        }

        playlist_lock::ptr lock = playlist_lock::try_acquire(*playlist_api, index);
        if (!lock.is_valid()) {
            continue;
        }

        playlist_api->playlist_clear(index);

        pfc::list_t<metadb_handle_ptr> handles;
        for (const auto& tr : pl.tracks) {
            pfc::string8 path;
            path << "applemusic://track/" << tr.id.c_str();
            playable_location_impl location(path, 0);
            metadb_handle_ptr handle;
            if (metadb::get()->handle_create(handle, location)) {
                handles.add_item(handle);
            }
        }

        playlist_api->playlist_add_items(index, handles.get_ptr(), handles.get_size());
    }
}
