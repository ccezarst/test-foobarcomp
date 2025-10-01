#include "apple_music_client.hpp"
#include "apple_music_auth.hpp"
#include "apple_music_models.hpp"

#include <foobar2000/SDK/foobar2000.h>
#include <foobar2000/SDK/input_impl.h>

using namespace foo_apple_music;

namespace {
    class input_apple_music : public input_stubs {
    public:
        input_apple_music() = default;

        void open(service_ptr_t<file> p_file, const char* p_path, t_input_open_reason p_reason, abort_callback& p_abort) override {
            p_abort.check();
            m_path = p_path;

            const auto tokens = apple_music_auth_manager::instance().tokens();
            if (!tokens) {
                throw exception_io_data();
            }

            apple_music_client client;
            client.set_tokens(tokens->developer_token, tokens->music_user_token);

            const std::string track_id = extract_track_id(p_path);
            auto stream = client.fetch_stream(track_id);
            if (!stream) {
                throw exception_io_data();
            }

            input_open_file_helper(m_decoder, p_file, stream->url.c_str(), p_reason, p_abort);
        }

        void get_info(file_info& p_info, abort_callback& p_abort) override {
            p_abort.check();
        }

        void decode_initialize(unsigned p_flags, abort_callback& p_abort) override {
            if (m_decoder.is_empty()) {
                throw exception_io_data();
            }
            m_decoder->initialize(p_flags, p_abort);
        }

        bool decode_run(audio_chunk& p_chunk, abort_callback& p_abort) override {
            return m_decoder.is_valid() && m_decoder->run(p_chunk, p_abort);
        }

        void decode_seek(double p_seconds, abort_callback& p_abort) override {
            if (m_decoder.is_valid()) {
                m_decoder->seek(p_seconds, p_abort);
            }
        }

        bool decode_can_seek() override {
            return m_decoder.is_valid() && m_decoder->can_seek();
        }

        t_filestats get_file_stats(abort_callback& p_abort) override {
            if (m_decoder.is_valid()) {
                return m_decoder->get_stats(p_abort);
            }
            return t_filestats();
        }

        static const char* g_get_name() { return "Apple Music Input"; }
        static const char* g_get_description() { return "Streams Apple Music tracks"; }
        static GUID g_get_guid() { return {0xccdf54c6, 0x8f1f, 0x45c5, {0x95, 0xfd, 0xc8, 0x94, 0x1c, 0x39, 0x8e, 0xa3}}; }

    private:
        std::string extract_track_id(const char* p_path) {
            std::string_view path_view(p_path);
            constexpr std::string_view prefix = "applemusic://track/";
            if (path_view.rfind(prefix, 0) == 0) {
                return std::string(path_view.substr(prefix.size()));
            }
            throw exception_io_data();
        }

        service_ptr_t<input_decoder> m_decoder;
        std::string m_path;
    };

    class input_entry_apple_music : public input_entry_impl<input_entry_apple_music> {
    public:
        static bool g_is_our_path(const char* p_path, const char* p_extension) {
            pfc::string_extension ext(p_path);
            if (p_extension != nullptr && stricmp_utf8(p_extension, "applemusic") == 0) {
                return true;
            }
            return strncmp(p_path, "applemusic://", 12) == 0;
        }

        static const char* g_get_name() { return input_apple_music::g_get_name(); }
        static const GUID g_get_guid() { return input_apple_music::g_get_guid(); }
        static const char* g_get_description() { return input_apple_music::g_get_description(); }
        static unsigned g_get_flags() { return flag_decode | flag_seekable | flag_dynamic_info | flag_dynamic_info_track; }
        static bool g_is_our_content_type(const char*) { return false; }

        static input_stubs::ptr g_create() { return new service_impl_t<input_apple_music>(); }
    };

    static service_factory_single_t<input_entry_apple_music> g_input_factory;
}
