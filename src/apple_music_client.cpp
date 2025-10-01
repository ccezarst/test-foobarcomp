#include "apple_music_client.hpp"
#include "apple_music_auth.hpp"

#include <foobar2000/helpers/http_client.h>
#include <foobar2000/helpers/json_helpers.h>
#include <foobar2000/SDK/foobar2000.h>

#include <nlohmann/json.hpp>

using namespace foo_apple_music;

namespace {
    constexpr const char* kBaseUrl = "https://api.music.apple.com";

    pfc::string8 make_endpoint(const char* path) {
        pfc::string8 url;
        url << kBaseUrl << path;
        return url;
    }
}

apple_music_client::apple_music_client() = default;

void apple_music_client::set_tokens(std::string developer_token, std::string music_user_token) {
    m_developer_token = std::move(developer_token);
    m_music_user_token = std::move(music_user_token);
}

std::string apple_music_client::build_authorization_header() const {
    pfc::string8 header;
    header << "Bearer " << m_developer_token.c_str();
    return header.c_str();
}

std::vector<playlist> apple_music_client::fetch_playlists() {
    std::vector<playlist> playlists;

    auto http = http_client::create();
    auto request = http->create_request("GET");

    pfc::string8 endpoint = make_endpoint("/v1/me/library/playlists?include=tracks");
    request->add_header("Authorization", build_authorization_header().c_str());
    request->add_header("Music-User-Token", m_music_user_token.c_str());

    auto response = request->run(endpoint.get_ptr(), core_api::get_main_window());
    auto response_text = response->read_string();

    const auto json = nlohmann::json::parse(response_text.get_ptr(), nullptr, true, true);

    if (!json.contains("data")) {
        return playlists;
    }

    for (const auto& item : json.at("data")) {
        playlist pl;
        pl.id = item.at("id").get<std::string>();
        pl.name = item.at("attributes").value("name", "Apple Music Playlist");
        pl.description = item.at("attributes").value("description", nlohmann::json{}).value("standard", "");

        if (item.contains("relationships") && item["relationships"].contains("tracks")) {
            for (const auto& track_item : item["relationships"]["tracks"]["data"]) {
                track t;
                t.id = track_item.at("id").get<std::string>();
                t.name = track_item.at("attributes").value("name", "Unknown");
                t.artist_name = track_item.at("attributes").value("artistName", "Unknown Artist");
                t.album_name = track_item.at("attributes").value("albumName", "");
                t.track_number = track_item.at("attributes").value("trackNumber", 0);
                t.disc_number = track_item.at("attributes").value("discNumber", 1);
                t.duration_seconds = track_item.at("attributes").value("durationInMillis", 0) / 1000.0;
                if (track_item.at("attributes").contains("artwork")) {
                    const auto& artwork = track_item.at("attributes").at("artwork");
                    pl.description = pl.description;
                    t.artwork_url = artwork.value("url", "");
                }
                pl.tracks.push_back(std::move(t));
            }
        }
        playlists.push_back(std::move(pl));
    }

    return playlists;
}

std::optional<track> apple_music_client::fetch_track(const std::string& track_id) {
    auto http = http_client::create();
    auto request = http->create_request("GET");

    pfc::string8 endpoint = make_endpoint(("/v1/catalog/us/songs/" + track_id).c_str());
    request->add_header("Authorization", build_authorization_header().c_str());
    request->add_header("Music-User-Token", m_music_user_token.c_str());

    auto response = request->run(endpoint.get_ptr(), core_api::get_main_window());
    auto response_text = response->read_string();
    const auto json = nlohmann::json::parse(response_text.get_ptr(), nullptr, true, true);

    if (!json.contains("data") || json.at("data").empty()) {
        return std::nullopt;
    }

    const auto& item = json.at("data").front();
    track t;
    t.id = item.at("id").get<std::string>();
    t.name = item.at("attributes").value("name", "Unknown");
    t.artist_name = item.at("attributes").value("artistName", "Unknown Artist");
    t.album_name = item.at("attributes").value("albumName", "");
    t.track_number = item.at("attributes").value("trackNumber", 0);
    t.disc_number = item.at("attributes").value("discNumber", 1);
    t.duration_seconds = item.at("attributes").value("durationInMillis", 0) / 1000.0;

    return t;
}

std::optional<track_stream> apple_music_client::fetch_stream(const std::string& track_id) {
    auto http = http_client::create();
    auto request = http->create_request("GET");

    pfc::string8 endpoint = make_endpoint(("/v1/me/stations/next-track?trackId=" + track_id).c_str());
    request->add_header("Authorization", build_authorization_header().c_str());
    request->add_header("Music-User-Token", m_music_user_token.c_str());

    auto response = request->run(endpoint.get_ptr(), core_api::get_main_window());
    auto response_text = response->read_string();
    const auto json = nlohmann::json::parse(response_text.get_ptr(), nullptr, true, true);

    if (!json.contains("data") || json.at("data").empty()) {
        return std::nullopt;
    }

    const auto& attributes = json.at("data").front().at("attributes");
    if (!attributes.contains("assetURL")) {
        return std::nullopt;
    }

    track_stream stream;
    stream.url = attributes.value("assetURL", "");
    stream.codec = attributes.value("codecName", "aac");
    stream.expires_in = std::chrono::seconds(attributes.value("durationInMillis", 0) / 1000);
    return stream;
}
