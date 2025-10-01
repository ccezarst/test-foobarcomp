#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <ctime>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <numbers>

#if defined(__has_include)
#    if __has_include("foobar2000/SDK/component.h")
#        include "foobar2000/SDK/component.h"
#        define FB2K_HAS_REAL_SDK 1
#    endif
#endif

#ifndef FB2K_HAS_REAL_SDK
class stream_writer
{
};

class stream_reader
{
};

class abort_callback
{
};

class service_factory_base
{
public:
    virtual ~service_factory_base() = default;
};

using t_uint32 = std::uint32_t;

struct HINSTANCE__
{
};

using HINSTANCE = HINSTANCE__*;

class foobar2000_api
{
public:
    virtual ~foobar2000_api() = default;
};

foobar2000_api* g_foobar2000_api = nullptr;

class foobar2000_client
{
public:
    using pservice_factory_base = service_factory_base*;

    enum
    {
        FOOBAR2000_CLIENT_VERSION_COMPATIBLE = 72,
        FOOBAR2000_CLIENT_VERSION = 78,
    };

    virtual ~foobar2000_client() = default;
    virtual t_uint32 get_version() = 0;
    virtual pservice_factory_base get_service_list() = 0;
    virtual void get_config(stream_writer*, abort_callback&) = 0;
    virtual void set_config(stream_reader*, abort_callback&) = 0;
    virtual void set_library_path(const char*, const char*) = 0;
    virtual void services_init(bool) = 0;
    virtual bool is_debug() = 0;
};
#endif

namespace foo_apple_music
{
    namespace
    {
        std::string to_lower(std::string_view text)
        {
            std::string normalised;
            normalised.reserve(text.size());
            for (unsigned char ch : text)
            {
                normalised.push_back(static_cast<char>(std::tolower(ch)));
            }
            return normalised;
        }

        double match_score(std::string_view field, std::string_view query)
        {
            if (query.empty())
            {
                return 0.0;
            }

            const std::string lower_field = to_lower(field);
            const std::string lower_query = to_lower(query);
            const std::size_t pos = lower_field.find(lower_query);
            if (pos == std::string::npos)
            {
                return -1.0;
            }

            const double closeness = 1.0 - static_cast<double>(pos) / static_cast<double>(lower_field.size() + 1);
            const double coverage = static_cast<double>(lower_query.size()) / static_cast<double>(lower_field.size() + 1);
            return (closeness * 0.6) + (coverage * 0.4);
        }

        std::string make_session_token(std::string_view username, std::string_view password)
        {
            const std::string combined = std::string(username) + "::" + std::string(password);
            const std::size_t hash_value = std::hash<std::string>{}(combined);
            std::ostringstream stream;
            stream << std::hex << std::uppercase << hash_value;
            return stream.str();
        }

        std::string format_timestamp(const std::chrono::system_clock::time_point& tp)
        {
            if (tp.time_since_epoch().count() == 0)
            {
                return {};
            }

            const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
#if defined(_WIN32)
            std::tm tm_storage{};
            gmtime_s(&tm_storage, &tt);
            const std::tm* tm_ptr = &tm_storage;
#else
            const std::tm* tm_ptr = std::gmtime(&tt);
#endif
            if (!tm_ptr)
            {
                return {};
            }

            std::ostringstream stream;
            stream << std::put_time(tm_ptr, "%Y-%m-%dT%H:%M:%SZ");
            return stream.str();
        }
    } // namespace

    struct TrackMetadata
    {
        std::string id;
        std::string title;
        std::string artist;
        std::string album;
        std::chrono::seconds duration{0};
    };

    class Playlist
    {
    public:
        Playlist() = default;

        Playlist(std::string name, std::string description, std::vector<std::string> track_ids)
            : name_(std::move(name))
            , description_(std::move(description))
            , track_ids_(std::move(track_ids))
        {
        }

        const std::string& name() const noexcept
        {
            return name_;
        }

        const std::string& description() const noexcept
        {
            return description_;
        }

        const std::vector<std::string>& track_ids() const noexcept
        {
            return track_ids_;
        }

        std::size_t size() const noexcept
        {
            return track_ids_.size();
        }

    private:
        std::string name_;
        std::string description_;
        std::vector<std::string> track_ids_;
    };

    struct PlaylistSummary
    {
        std::string name;
        std::string description;
        std::size_t track_count = 0;
    };

    struct PlaylistView
    {
        std::string name;
        std::string description;
        std::vector<TrackMetadata> tracks;
    };

    class MusicLibrary
    {
    public:
        MusicLibrary() = default;

        void add_track(TrackMetadata track)
        {
            const std::string track_id = track.id;
            track_index_[track_id] = tracks_.size();
            tracks_.push_back(std::move(track));
        }

        void add_playlist(Playlist playlist)
        {
            playlist_index_[playlist.name()] = playlists_.size();
            playlists_.push_back(std::move(playlist));
        }

        void set_user_playlists(std::vector<Playlist> playlists)
        {
            user_playlists_ = std::move(playlists);
            user_playlist_index_.clear();
            for (std::size_t i = 0; i < user_playlists_.size(); ++i)
            {
                user_playlist_index_[user_playlists_[i].name()] = i;
            }
        }

        std::vector<TrackMetadata> search(std::string_view query) const
        {
            struct ScoredTrack
            {
                double score;
                const TrackMetadata* track;
            };

            std::vector<ScoredTrack> scored_tracks;
            scored_tracks.reserve(tracks_.size());

            for (const auto& track : tracks_)
            {
                const double title_score = match_score(track.title, query);
                const double artist_score = match_score(track.artist, query);
                const double album_score = match_score(track.album, query);

                const double best = std::max({title_score, artist_score, album_score});
                if (best < 0.0)
                {
                    continue;
                }

                scored_tracks.push_back({best, &track});
            }

            std::sort(scored_tracks.begin(), scored_tracks.end(), [](const ScoredTrack& lhs, const ScoredTrack& rhs) {
                if (std::abs(lhs.score - rhs.score) > std::numeric_limits<double>::epsilon())
                {
                    return lhs.score > rhs.score;
                }
                return lhs.track->title < rhs.track->title;
            });

            std::vector<TrackMetadata> result;
            result.reserve(scored_tracks.size());
            for (const auto& scored : scored_tracks)
            {
                result.push_back(*scored.track);
            }
            return result;
        }

        std::vector<PlaylistSummary> playlists() const
        {
            std::vector<PlaylistSummary> summaries;
            summaries.reserve(playlists_.size());
            for (const auto& playlist : playlists_)
            {
                summaries.push_back(PlaylistSummary{playlist.name(), playlist.description(), playlist.size()});
            }
            return summaries;
        }

        PlaylistView playlist(std::string_view name) const
        {
            const Playlist& playlist_ref = playlist_by_name(name);
            PlaylistView view;
            view.name = playlist_ref.name();
            view.description = playlist_ref.description();
            view.tracks.reserve(playlist_ref.track_ids().size());
            for (const auto& track_id : playlist_ref.track_ids())
            {
                view.tracks.push_back(track_by_id(track_id));
            }
            return view;
        }

        std::vector<PlaylistSummary> user_playlists() const
        {
            std::vector<PlaylistSummary> summaries;
            summaries.reserve(user_playlists_.size());
            for (const auto& playlist : user_playlists_)
            {
                summaries.push_back(PlaylistSummary{playlist.name(), playlist.description(), playlist.size()});
            }
            return summaries;
        }

        PlaylistView user_playlist(std::string_view name) const
        {
            const Playlist& playlist_ref = user_playlist_by_name(name);
            PlaylistView view;
            view.name = playlist_ref.name();
            view.description = playlist_ref.description();
            view.tracks.reserve(playlist_ref.track_ids().size());
            for (const auto& track_id : playlist_ref.track_ids())
            {
                view.tracks.push_back(track_by_id(track_id));
            }
            return view;
        }

        std::vector<TrackMetadata> search_excluding(std::string_view query, const std::vector<std::string>& excluded_ids) const
        {
            if (excluded_ids.empty())
            {
                return search(query);
            }

            const std::unordered_set<std::string> excluded(excluded_ids.begin(), excluded_ids.end());

            std::vector<TrackMetadata> results = search(query);
            results.erase(
                std::remove_if(results.begin(), results.end(), [&excluded](const TrackMetadata& track) {
                    return excluded.find(track.id) != excluded.end();
                }),
                results.end());
            return results;
        }

        const TrackMetadata& track_by_id(std::string_view id) const
        {
            const auto it = track_index_.find(std::string(id));
            if (it == track_index_.end())
            {
                throw std::out_of_range("Unknown Apple Music track identifier");
            }
            return tracks_.at(it->second);
        }

        static MusicLibrary create_demo_library()
        {
            MusicLibrary library;
            library.add_track({"am_001", "Shimmering Lights", "Aurora Bloom", "Skyline Echoes", std::chrono::seconds(210)});
            library.add_track({"am_002", "Neon Rain", "Midnight City", "Urban Nights", std::chrono::seconds(198)});
            library.add_track({"am_003", "Gravity", "The Wanderers", "Celestial", std::chrono::seconds(248)});
            library.add_track({"am_004", "Above the Clouds", "Featherfall", "Horizons", std::chrono::seconds(230)});
            library.add_track({"am_005", "Wildfire", "Nova Pulse", "Ignite", std::chrono::seconds(205)});
            library.add_track({"am_006", "Reflections", "Analog Dreams", "Fragments", std::chrono::seconds(242)});
            library.add_track({"am_007", "Aurora Trails", "Lumen", "Northern Skies", std::chrono::seconds(225)});
            library.add_track({"am_008", "Midnight Drive", "City Nights", "Highways", std::chrono::seconds(215)});

            library.add_playlist(Playlist("Morning Boost", "Upbeat tracks to start your day", {"am_001", "am_005", "am_008"}));
            library.add_playlist(Playlist("Night Shift", "Late-night ambient electronics", {"am_002", "am_003", "am_006", "am_007"}));
            library.add_playlist(Playlist("Focus Flow", "Instrumental cues for deep work", {"am_004", "am_006", "am_007"}));

            library.set_user_playlists(create_demo_user_playlists("Guest Listener"));

            return library;
        }

        static std::vector<Playlist> create_demo_user_playlists(std::string_view listener_name)
        {
            const std::string prefix = std::string(listener_name);
            std::vector<Playlist> playlists;
            playlists.emplace_back(prefix + " Favorites", "Songs you have favourited recently", std::vector<std::string>{"am_001", "am_003", "am_005"});
            playlists.emplace_back(prefix + " Chill Mix", "Laid-back selections based on your play history", std::vector<std::string>{"am_004", "am_006", "am_007"});
            playlists.emplace_back(prefix + " Commute", "Energetic tunes curated from your Apple Music library", std::vector<std::string>{"am_002", "am_005", "am_008"});
            return playlists;
        }

    private:
        const Playlist& playlist_by_name(std::string_view name) const
        {
            const auto it = playlist_index_.find(std::string(name));
            if (it == playlist_index_.end())
            {
                throw std::out_of_range("Unknown Apple Music playlist");
            }
            return playlists_.at(it->second);
        }

        const Playlist& user_playlist_by_name(std::string_view name) const
        {
            const auto it = user_playlist_index_.find(std::string(name));
            if (it == user_playlist_index_.end())
            {
                throw std::out_of_range("Unknown Apple Music user playlist");
            }
            return user_playlists_.at(it->second);
        }

        std::vector<TrackMetadata> tracks_;
        std::unordered_map<std::string, std::size_t> track_index_;
        std::vector<Playlist> playlists_;
        std::unordered_map<std::string, std::size_t> playlist_index_;
        std::vector<Playlist> user_playlists_;
        std::unordered_map<std::string, std::size_t> user_playlist_index_;
    };

    class StreamingSession
    {
    public:
        static constexpr double kSampleRate = 44100.0;

        StreamingSession(TrackMetadata metadata, std::uint64_t id)
            : metadata_(std::move(metadata))
            , id_(id)
            , total_frames_(std::max<std::uint64_t>(1, static_cast<std::uint64_t>(metadata_.duration.count()) * static_cast<std::uint64_t>(kSampleRate)))
        {
            if (total_frames_ == 0)
            {
                total_frames_ = static_cast<std::uint64_t>(kSampleRate * 30);
            }

            const std::size_t hash_seed = std::hash<std::string>{}(metadata_.id);
            base_frequency_left_ = 180.0 + static_cast<double>(hash_seed % 220);
            base_frequency_right_ = base_frequency_left_ * 1.05;
            phase_step_left_ = (std::numbers::pi * 2.0 * base_frequency_left_) / kSampleRate;
            phase_step_right_ = (std::numbers::pi * 2.0 * base_frequency_right_) / kSampleRate;
        }

        std::uint64_t id() const noexcept
        {
            return id_;
        }

        const TrackMetadata& metadata() const noexcept
        {
            return metadata_;
        }

        bool finished() const noexcept
        {
            return frames_generated_ >= total_frames_;
        }

        std::size_t read_frames(float* destination, std::size_t frame_capacity)
        {
            if (!destination || frame_capacity == 0 || finished())
            {
                return 0;
            }

            const std::uint64_t frames_remaining = total_frames_ - frames_generated_;
            const std::size_t frames_to_generate = static_cast<std::size_t>(std::min<std::uint64_t>(frames_remaining, frame_capacity));

            constexpr double amplitude = 0.18;
            const std::uint64_t fade_samples = std::min<std::uint64_t>(
                static_cast<std::uint64_t>(kSampleRate * 0.02),
                total_frames_ / 4);

            for (std::size_t i = 0; i < frames_to_generate; ++i)
            {
                const std::uint64_t absolute_frame = frames_generated_ + static_cast<std::uint64_t>(i);

                double envelope = 1.0;
                if (fade_samples > 0)
                {
                    if (absolute_frame < fade_samples)
                    {
                        envelope *= static_cast<double>(absolute_frame) / static_cast<double>(fade_samples);
                    }
                    if (absolute_frame > total_frames_ - fade_samples)
                    {
                        const std::uint64_t frames_until_end = total_frames_ - absolute_frame;
                        envelope *= static_cast<double>(frames_until_end) / static_cast<double>(fade_samples);
                    }
                }

                const double left_sample = std::sin(phase_left_) * amplitude * envelope;
                const double right_sample = std::sin(phase_right_) * amplitude * envelope;

                destination[(i * 2)] = static_cast<float>(left_sample);
                destination[(i * 2) + 1] = static_cast<float>(right_sample);

                phase_left_ += phase_step_left_;
                phase_right_ += phase_step_right_;

                if (phase_left_ >= std::numbers::pi * 2.0)
                {
                    phase_left_ -= std::numbers::pi * 2.0;
                }
                if (phase_right_ >= std::numbers::pi * 2.0)
                {
                    phase_right_ -= std::numbers::pi * 2.0;
                }
            }

            frames_generated_ += static_cast<std::uint64_t>(frames_to_generate);
            return frames_to_generate;
        }

    private:
        TrackMetadata metadata_;
        std::uint64_t id_ = 0;
        std::uint64_t total_frames_ = 0;
        std::uint64_t frames_generated_ = 0;
        double base_frequency_left_ = 0.0;
        double base_frequency_right_ = 0.0;
        double phase_left_ = 0.0;
        double phase_right_ = 0.0;
        double phase_step_left_ = 0.0;
        double phase_step_right_ = 0.0;
    };

    class StreamingEngine
    {
    public:
        std::shared_ptr<StreamingSession> start(TrackMetadata metadata)
        {
            const std::uint64_t id = next_session_id_.fetch_add(1, std::memory_order_relaxed);
            auto session = std::make_shared<StreamingSession>(std::move(metadata), id);

            const std::scoped_lock lock(mutex_);
            sessions_.emplace(id, session);
            return session;
        }

        std::shared_ptr<StreamingSession> get(std::uint64_t id) const
        {
            const std::scoped_lock lock(mutex_);
            const auto it = sessions_.find(id);
            if (it == sessions_.end())
            {
                return nullptr;
            }
            return it->second;
        }

        void stop(std::uint64_t id)
        {
            const std::scoped_lock lock(mutex_);
            sessions_.erase(id);
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::shared_ptr<StreamingSession>> sessions_;
        std::atomic_uint64_t next_session_id_{1};
    };

    class AppleMusicService
    {
    public:
        struct SettingsState
        {
            std::string account_name;
            std::string session_token;
            std::chrono::system_clock::time_point last_login{};
            bool remember_me = true;
            bool auto_login = false;
            bool logged_in = false;
        };

        AppleMusicService()
            : library_(MusicLibrary::create_demo_library())
        {
        }

        std::vector<TrackMetadata> search(std::string_view query) const
        {
            const std::scoped_lock lock(mutex_);
            return library_.search(query);
        }

        std::vector<TrackMetadata> search_missing(std::string_view query, const std::vector<std::string>& excluded_ids) const
        {
            const std::scoped_lock lock(mutex_);
            return library_.search_excluding(query, excluded_ids);
        }

        std::vector<PlaylistSummary> list_playlists() const
        {
            const std::scoped_lock lock(mutex_);
            return library_.playlists();
        }

        std::vector<PlaylistSummary> list_user_playlists() const
        {
            const std::scoped_lock lock(mutex_);
            if (!settings_.logged_in)
            {
                return {};
            }
            return library_.user_playlists();
        }

        PlaylistView load_playlist(std::string_view name) const
        {
            const std::scoped_lock lock(mutex_);
            return library_.playlist(name);
        }

        PlaylistView load_user_playlist(std::string_view name) const
        {
            const std::scoped_lock lock(mutex_);
            if (!settings_.logged_in)
            {
                throw std::runtime_error("Apple Music account is not authenticated");
            }
            return library_.user_playlist(name);
        }

        bool login(std::string username, std::string password)
        {
            if (username.empty() || password.empty())
            {
                const std::scoped_lock lock(mutex_);
                last_error_ = "Username and password are required";
                return false;
            }

            const std::string token = make_session_token(username, password);
            const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

            const std::scoped_lock lock(mutex_);
            settings_.account_name = std::move(username);
            settings_.session_token = token;
            settings_.last_login = now;
            settings_.logged_in = true;
            account_display_name_ = settings_.account_name;
            last_error_.reset();
            library_.set_user_playlists(MusicLibrary::create_demo_user_playlists(account_display_name_));
            return true;
        }

        void logout()
        {
            const std::scoped_lock lock(mutex_);
            settings_.logged_in = false;
            settings_.session_token.clear();
            account_display_name_.clear();
            library_.set_user_playlists(std::vector<Playlist>{});
            last_error_.reset();
        }

        void set_remember_me(bool value)
        {
            const std::scoped_lock lock(mutex_);
            settings_.remember_me = value;
        }

        void set_auto_login(bool value)
        {
            const std::scoped_lock lock(mutex_);
            settings_.auto_login = value;
        }

        SettingsState settings() const
        {
            const std::scoped_lock lock(mutex_);
            return settings_;
        }

        std::optional<std::string> last_error() const
        {
            const std::scoped_lock lock(mutex_);
            return last_error_;
        }

        bool is_logged_in() const
        {
            const std::scoped_lock lock(mutex_);
            return settings_.logged_in;
        }

        std::string last_login_timestamp() const
        {
            const std::scoped_lock lock(mutex_);
            return format_timestamp(settings_.last_login);
        }

        std::string account_display_name() const
        {
            const std::scoped_lock lock(mutex_);
            return account_display_name_;
        }

        std::shared_ptr<StreamingSession> start_stream(std::string_view track_id)
        {
            TrackMetadata track;
            {
                const std::scoped_lock lock(mutex_);
                if (!settings_.logged_in)
                {
                    throw std::runtime_error("Apple Music account is not authenticated");
                }
                track = library_.track_by_id(track_id);
            }
            return streaming_.start(std::move(track));
        }

        std::shared_ptr<StreamingSession> get_stream(std::uint64_t id) const
        {
            return streaming_.get(id);
        }

        void stop_stream(std::uint64_t id)
        {
            streaming_.stop(id);
        }

        static constexpr const char* component_name() noexcept
        {
            return "Apple Music Bridge";
        }

        static constexpr const char* settings_window_title() noexcept
        {
            return "Apple Music Account";
        }

    private:
        mutable std::mutex mutex_;
        MusicLibrary library_;
        StreamingEngine streaming_;
        SettingsState settings_;
        std::string account_display_name_;
        std::optional<std::string> last_error_;
    };
} // namespace foo_apple_music

namespace
{
#ifdef _WIN32
#    define FB2K_EXPORT extern "C" __declspec(dllexport)
#else
#    define FB2K_EXPORT extern "C"
#endif

namespace fb2k_placeholder
{
    inline foobar2000_api*& api_storage()
    {
#if defined(FB2K_HAS_REAL_SDK)
        return ::g_foobar2000_api;
#else
        return g_foobar2000_api;
#endif
    }

    class placeholder_client final : public foobar2000_client
    {
    public:
        using pservice_factory_base = typename foobar2000_client::pservice_factory_base;

        placeholder_client()
            : service_factory_(*this)
        {
        }

        t_uint32 get_version() override
        {
            return foobar2000_client::FOOBAR2000_CLIENT_VERSION;
        }

        pservice_factory_base get_service_list() override
        {
            return &service_factory_;
        }

        void get_config(stream_writer*, abort_callback&) override
        {
        }

        void set_config(stream_reader*, abort_callback&) override
        {
        }

        void set_library_path(const char* path, const char* name) override
        {
            profile_path_ = path ? path : "";
            module_name_ = name ? name : "";
        }

        void services_init(bool) override
        {
            ensure_service_ready();
        }

        bool is_debug() override
        {
#ifdef _DEBUG
            return true;
#else
            return false;
#endif
        }

        foo_apple_music::AppleMusicService& service()
        {
            return ensure_service_ready();
        }

    private:
        class apple_music_service_factory final : public service_factory_base
        {
        public:
            explicit apple_music_service_factory(placeholder_client& owner)
                : owner_(owner)
            {
            }

            foo_apple_music::AppleMusicService& service() const
            {
                return owner_.service();
            }

        private:
            placeholder_client& owner_;
        };

        foo_apple_music::AppleMusicService& ensure_service_ready()
        {
            const std::scoped_lock lock(service_mutex_);
            if (!service_)
            {
                service_ = std::make_unique<foo_apple_music::AppleMusicService>();
            }
            return *service_;
        }

        std::string profile_path_;
        std::string module_name_;
        mutable std::mutex service_mutex_;
        std::unique_ptr<foo_apple_music::AppleMusicService> service_;
        apple_music_service_factory service_factory_{*this};
    };

    struct client_context
    {
        placeholder_client client;
        HINSTANCE instance = nullptr;
    };

    client_context g_client_context;
} // namespace fb2k_placeholder
} // namespace

FB2K_EXPORT foobar2000_client* foobar2000_get_interface(foobar2000_api* api, HINSTANCE instance)
{
    fb2k_placeholder::api_storage() = api;
    fb2k_placeholder::g_client_context.instance = instance;
    return &fb2k_placeholder::g_client_context.client;
}

struct foo_apple_music_track_info
{
    const char* id = nullptr;
    const char* title = nullptr;
    const char* artist = nullptr;
    const char* album = nullptr;
    std::uint32_t duration_seconds = 0;
};

struct foo_apple_music_playlist_info
{
    const char* name = nullptr;
    const char* description = nullptr;
    std::uint32_t track_count = 0;
};

struct foo_apple_music_settings_info
{
    const char* account_name = nullptr;
    const char* session_token = nullptr;
    const char* last_login_utc = nullptr;
    bool remember_me = false;
    bool auto_login = false;
    bool logged_in = false;
};

FB2K_EXPORT const char* foo_apple_music_component_name()
{
    return foo_apple_music::AppleMusicService::component_name();
}

FB2K_EXPORT const char* foo_apple_music_settings_window_title()
{
    return foo_apple_music::AppleMusicService::settings_window_title();
}

FB2K_EXPORT bool foo_apple_music_login(const char* username, const char* password)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    try
    {
        const std::string user = username ? std::string(username) : std::string{};
        const std::string pass = password ? std::string(password) : std::string{};
        return service.login(user, pass);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

FB2K_EXPORT void foo_apple_music_logout()
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    service.logout();
}

FB2K_EXPORT void foo_apple_music_update_settings(bool remember_me, bool auto_login)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    service.set_remember_me(remember_me);
    service.set_auto_login(auto_login);
}

FB2K_EXPORT foo_apple_music_settings_info foo_apple_music_get_settings()
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const foo_apple_music::AppleMusicService::SettingsState state = service.settings();

    thread_local std::string account_storage;
    thread_local std::string token_storage;
    thread_local std::string timestamp_storage;

    account_storage = state.account_name;
    token_storage = state.session_token;
    timestamp_storage = service.last_login_timestamp();

    foo_apple_music_settings_info info;
    info.account_name = account_storage.empty() ? nullptr : account_storage.c_str();
    info.session_token = token_storage.empty() ? nullptr : token_storage.c_str();
    info.last_login_utc = timestamp_storage.empty() ? nullptr : timestamp_storage.c_str();
    info.remember_me = state.remember_me;
    info.auto_login = state.auto_login;
    info.logged_in = state.logged_in;
    return info;
}

FB2K_EXPORT const char* foo_apple_music_last_error()
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::optional<std::string> error = service.last_error();
    if (!error || error->empty())
    {
        return nullptr;
    }

    thread_local std::string storage;
    storage = *error;
    return storage.c_str();
}

FB2K_EXPORT std::size_t foo_apple_music_search_tracks(const char* query, foo_apple_music_track_info* out_tracks, std::size_t capacity)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::string_view query_view = query ? std::string_view(query) : std::string_view{};
    const std::vector<foo_apple_music::TrackMetadata> results = service.search(query_view);

    if (!out_tracks || capacity == 0)
    {
        return results.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(results.size() * 4);

    const std::size_t count = std::min<std::size_t>(capacity, results.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& track = results[i];
        storage.push_back(track.id);
        storage.push_back(track.title);
        storage.push_back(track.artist);
        storage.push_back(track.album);

        foo_apple_music_track_info& info = out_tracks[i];
        info.id = storage[storage.size() - 4].c_str();
        info.title = storage[storage.size() - 3].c_str();
        info.artist = storage[storage.size() - 2].c_str();
        info.album = storage[storage.size() - 1].c_str();
        info.duration_seconds = static_cast<std::uint32_t>(track.duration.count());
    }

    return results.size();
}

FB2K_EXPORT std::size_t foo_apple_music_search_remote_tracks(
    const char* query,
    const char* const* local_track_ids,
    std::size_t local_track_count,
    foo_apple_music_track_info* out_tracks,
    std::size_t capacity)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::string_view query_view = query ? std::string_view(query) : std::string_view{};

    std::vector<std::string> excluded_ids;
    if (local_track_ids && local_track_count > 0)
    {
        excluded_ids.reserve(local_track_count);
        for (std::size_t i = 0; i < local_track_count; ++i)
        {
            if (local_track_ids[i])
            {
                excluded_ids.emplace_back(local_track_ids[i]);
            }
        }
    }

    const std::vector<foo_apple_music::TrackMetadata> results = service.search_missing(query_view, excluded_ids);

    if (!out_tracks || capacity == 0)
    {
        return results.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(results.size() * 4);

    const std::size_t count = std::min<std::size_t>(capacity, results.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& track = results[i];
        storage.push_back(track.id);
        storage.push_back(track.title);
        storage.push_back(track.artist);
        storage.push_back(track.album);

        foo_apple_music_track_info& info = out_tracks[i];
        info.id = storage[storage.size() - 4].c_str();
        info.title = storage[storage.size() - 3].c_str();
        info.artist = storage[storage.size() - 2].c_str();
        info.album = storage[storage.size() - 1].c_str();
        info.duration_seconds = static_cast<std::uint32_t>(track.duration.count());
    }

    return results.size();
}

FB2K_EXPORT std::size_t foo_apple_music_list_playlists(foo_apple_music_playlist_info* out_playlists, std::size_t capacity)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::vector<foo_apple_music::PlaylistSummary> playlists = service.list_playlists();

    if (!out_playlists || capacity == 0)
    {
        return playlists.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(playlists.size() * 2);

    const std::size_t count = std::min<std::size_t>(capacity, playlists.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& playlist = playlists[i];
        storage.push_back(playlist.name);
        storage.push_back(playlist.description);

        foo_apple_music_playlist_info& info = out_playlists[i];
        info.name = storage[storage.size() - 2].c_str();
        info.description = storage[storage.size() - 1].c_str();
        info.track_count = static_cast<std::uint32_t>(playlist.track_count);
    }

    return playlists.size();
}

FB2K_EXPORT std::size_t foo_apple_music_list_user_playlists(foo_apple_music_playlist_info* out_playlists, std::size_t capacity)
{
    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::vector<foo_apple_music::PlaylistSummary> playlists = service.list_user_playlists();

    if (!out_playlists || capacity == 0)
    {
        return playlists.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(playlists.size() * 2);

    const std::size_t count = std::min<std::size_t>(capacity, playlists.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& playlist = playlists[i];
        storage.push_back(playlist.name);
        storage.push_back(playlist.description);

        foo_apple_music_playlist_info& info = out_playlists[i];
        info.name = storage[storage.size() - 2].c_str();
        info.description = storage[storage.size() - 1].c_str();
        info.track_count = static_cast<std::uint32_t>(playlist.track_count);
    }

    return playlists.size();
}

FB2K_EXPORT std::size_t foo_apple_music_load_playlist(const char* name, foo_apple_music_track_info* out_tracks, std::size_t capacity)
{
    if (!name)
    {
        return 0;
    }

    auto& service = fb2k_placeholder::g_client_context.client.service();
    foo_apple_music::PlaylistView view;
    try
    {
        view = service.load_playlist(name);
    }
    catch (const std::exception&)
    {
        return 0;
    }

    if (!out_tracks || capacity == 0)
    {
        return view.tracks.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(view.tracks.size() * 4);

    const std::size_t count = std::min<std::size_t>(capacity, view.tracks.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& track = view.tracks[i];
        storage.push_back(track.id);
        storage.push_back(track.title);
        storage.push_back(track.artist);
        storage.push_back(track.album);

        foo_apple_music_track_info& info = out_tracks[i];
        info.id = storage[storage.size() - 4].c_str();
        info.title = storage[storage.size() - 3].c_str();
        info.artist = storage[storage.size() - 2].c_str();
        info.album = storage[storage.size() - 1].c_str();
        info.duration_seconds = static_cast<std::uint32_t>(track.duration.count());
    }

    return view.tracks.size();
}

FB2K_EXPORT std::size_t foo_apple_music_load_user_playlist(const char* name, foo_apple_music_track_info* out_tracks, std::size_t capacity)
{
    if (!name)
    {
        return 0;
    }

    auto& service = fb2k_placeholder::g_client_context.client.service();
    foo_apple_music::PlaylistView view;
    try
    {
        view = service.load_user_playlist(name);
    }
    catch (const std::exception&)
    {
        return 0;
    }

    if (!out_tracks || capacity == 0)
    {
        return view.tracks.size();
    }

    thread_local std::vector<std::string> storage;
    storage.clear();
    storage.reserve(view.tracks.size() * 4);

    const std::size_t count = std::min<std::size_t>(capacity, view.tracks.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& track = view.tracks[i];
        storage.push_back(track.id);
        storage.push_back(track.title);
        storage.push_back(track.artist);
        storage.push_back(track.album);

        foo_apple_music_track_info& info = out_tracks[i];
        info.id = storage[storage.size() - 4].c_str();
        info.title = storage[storage.size() - 3].c_str();
        info.artist = storage[storage.size() - 2].c_str();
        info.album = storage[storage.size() - 1].c_str();
        info.duration_seconds = static_cast<std::uint32_t>(track.duration.count());
    }

    return view.tracks.size();
}

FB2K_EXPORT std::uint64_t foo_apple_music_open_stream(const char* track_id)
{
    if (!track_id)
    {
        return 0;
    }

    auto& service = fb2k_placeholder::g_client_context.client.service();
    try
    {
        const std::shared_ptr<foo_apple_music::StreamingSession> session = service.start_stream(track_id);
        return session ? session->id() : 0;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

FB2K_EXPORT std::size_t foo_apple_music_read_stream(std::uint64_t stream_id, float* buffer, std::size_t frame_capacity)
{
    if (stream_id == 0)
    {
        return 0;
    }

    auto& service = fb2k_placeholder::g_client_context.client.service();
    const std::shared_ptr<foo_apple_music::StreamingSession> session = service.get_stream(stream_id);
    if (!session)
    {
        return 0;
    }

    const std::size_t frames = session->read_frames(buffer, frame_capacity);
    if (frames == 0 && session->finished())
    {
        service.stop_stream(stream_id);
    }
    return frames;
}

FB2K_EXPORT void foo_apple_music_close_stream(std::uint64_t stream_id)
{
    if (stream_id == 0)
    {
        return;
    }

    auto& service = fb2k_placeholder::g_client_context.client.service();
    service.stop_stream(stream_id);
}

#undef FB2K_EXPORT
