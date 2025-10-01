#include "playlist_sync_service.hpp"
#include "apple_music_auth.hpp"
#include "apple_music_preferences.hpp"

#include <foobar2000/SDK/foobar2000.h>
#include <foobar2000/SDK/mainmenu.h>
#include <foobar2000/SDK/ui_control.h>
#include <foobar2000/helpers/ui_helpers.h>

#include <atlbase.h>

CComModule _Module;

using namespace foo_apple_music;

namespace {

class mainmenu_command_sync : public mainmenu_commands {
public:
    enum {
        command_sync = 0,
        command_set_tokens,
        command_total
    }; // NOLINT

    t_uint32 get_command_count() override { return command_total; }

    GUID get_command(t_uint32 index) override {
        switch (index) {
        case command_sync: {
            static const GUID guid_sync = {0x5b160847, 0x52df, 0x43dc, {0x9e, 0x4a, 0xba, 0x63, 0x47, 0x57, 0x8e, 0x44}};
            return guid_sync;
        }
        case command_set_tokens: {
            static const GUID guid_tokens = {0x6bb86317, 0x2d85, 0x4bc9, {0xae, 0x1a, 0x0f, 0x8d, 0x48, 0x11, 0x73, 0x95}};
            return guid_tokens;
        }
        default:
            uBugCheck();
        }
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case command_sync:
            out = "Sync Apple Music Playlists";
            break;
        case command_set_tokens:
            out = "Apple Music Preferences";
            break;
        default:
            uBugCheck();
        }
    }

    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case command_sync:
            out = "Fetch playlists from Apple Music and mirror them in foobar2000.";
            return true;
        case command_set_tokens:
            out = "Open the Apple Music preferences page to manage login and token settings.";
            return true;
        default:
            return false;
        }
    }

    GUID get_parent() override { return mainmenu_groups::library; }

    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        switch (index) {
        case command_sync:
            try {
                playlist_sync_service service;
                service.synchronize();
            } catch (const std::exception& e) {
                console::print(pfc::string8() << "Apple Music sync failed: " << e.what());
            }
            break;
        case command_set_tokens:
            static_api_ptr_t<ui_control>()->show_preferences(kAppleMusicPreferencesGuid);
            break;
        default:
            break;
        }
    }

};

static service_factory_single_t<mainmenu_command_sync> g_mainmenu_factory;

} // namespace

DECLARE_COMPONENT_VERSION("Apple Music Integration", "0.1.0", "Integrates Apple Music playlists and streaming into foobar2000.");
VALIDATE_COMPONENT_FILENAME("foo_apple_music.dll");
