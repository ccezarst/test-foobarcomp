#include "apple_music_preferences.hpp"
#include "apple_music_auth.hpp"
#include "apple_music_login_dialog.hpp"

#include <foobar2000/helpers/preferences_page_impl.h>
#include <foobar2000/helpers/ui_helpers.h>
#include <pfc/string_conv.h>

#include <atlbase.h>
#include <atlwin.h>

#include "resource.h"

#include <string>

extern CComModule _Module;

namespace foo_apple_music {

const GUID kAppleMusicPreferencesGuid =
    {0x2c272b57, 0xb6c5, 0x4b74, {0x9a, 0x41, 0xf7, 0x51, 0x3f, 0x0b, 0x85, 0x8f}};

namespace {

pfc::string8 format_token_preview(const std::string& token) {
    if (token.empty()) {
        return "";
    }

    constexpr size_t kPreview = 12;
    if (token.size() <= kPreview * 2) {
        return token.c_str();
    }

    pfc::string8 preview;
    preview << token.substr(0, kPreview).c_str() << "…" << token.substr(token.size() - kPreview).c_str();
    return preview;
}

class apple_music_preferences_page final : public preferences_page_impl<apple_music_preferences_page> {
public:
    enum {IDD = IDD_APPLE_MUSIC_PREFS};

    apple_music_preferences_page() = default;

    BEGIN_MSG_MAP(apple_music_preferences_page)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_HANDLER(IDC_EDIT_DEVELOPER_TOKEN, EN_CHANGE, OnDeveloperChanged)
        COMMAND_HANDLER(IDC_BUTTON_APPLE_MUSIC_LOGIN, BN_CLICKED, OnSignInClicked)
        COMMAND_HANDLER(IDC_BUTTON_APPLE_MUSIC_SIGN_OUT, BN_CLICKED, OnSignOutClicked)
    END_MSG_MAP()

    t_uint32 get_state() override {
        t_uint32 state = preferences_state::resettable;
        if (has_changes()) {
            state |= preferences_state::changed;
        }
        return state;
    }

    void reset() override {
        apple_music_auth_manager& auth = apple_music_auth_manager::instance();
        m_pending_developer = auth.developer_token_value();
        m_pending_user = auth.music_user_token_value();
        m_initial_developer = m_pending_developer;
        m_initial_user = m_pending_user;

        uSetDlgItemText(m_hWnd, IDC_EDIT_DEVELOPER_TOKEN, m_pending_developer.c_str());
        const auto preview = format_token_preview(m_pending_user);
        uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_TOKEN, preview.get_ptr());
        update_status();
        on_change();
    }

    void apply() override {
        apple_music_auth_manager& auth = apple_music_auth_manager::instance();
        auth.set_developer_token(m_pending_developer);
        if (!m_pending_user.empty()) {
            auth.set_music_user_token(m_pending_user);
        } else {
            auth.clear_user_token();
        }

        m_initial_developer = m_pending_developer;
        m_initial_user = m_pending_user;
        on_change();
    }

private:
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
        apple_music_auth_manager& auth = apple_music_auth_manager::instance();
        m_initial_developer = auth.developer_token_value();
        m_initial_user = auth.music_user_token_value();
        m_pending_developer = m_initial_developer;
        m_pending_user = m_initial_user;

        uSetDlgItemText(m_hWnd, IDC_EDIT_DEVELOPER_TOKEN, m_pending_developer.c_str());
        const auto preview = format_token_preview(m_pending_user);
        uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_TOKEN, preview.get_ptr());
        update_status();
        return TRUE;
    }

    LRESULT OnDeveloperChanged(WORD, WORD, HWND, BOOL&) {
        pfc::string8 value;
        uGetDlgItemText(m_hWnd, IDC_EDIT_DEVELOPER_TOKEN, value);
        std::string converted = value.c_str();
        if (converted != m_pending_developer) {
            m_pending_developer = std::move(converted);
            on_change();
        }
        return 0;
    }

    LRESULT OnSignInClicked(WORD, WORD, HWND, BOOL&) {
        pfc::string8 developer_text;
        uGetDlgItemText(m_hWnd, IDC_EDIT_DEVELOPER_TOKEN, developer_text);
        if (developer_text.is_empty()) {
            uMessageBox(m_hWnd, "Enter a valid MusicKit developer token before signing in.", "Apple Music", MB_ICONWARNING | MB_OK);
            return 0;
        }

        const std::string developer = developer_text.c_str();
        if (developer.size() < 10) {
            uMessageBox(m_hWnd, "The supplied developer token appears to be invalid.", "Apple Music", MB_ICONWARNING | MB_OK);
            return 0;
        }

        auto token = run_apple_music_login(m_hWnd, developer);
        if (!token) {
            return 0;
        }

        m_pending_developer = developer;
        m_pending_user = *token;
        const auto preview = format_token_preview(m_pending_user);
        uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_TOKEN, preview.get_ptr());
        update_status();
        on_change();
        return 0;
    }

    LRESULT OnSignOutClicked(WORD, WORD, HWND, BOOL&) {
        if (m_pending_user.empty()) {
            return 0;
        }

        m_pending_user.clear();
        uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_TOKEN, "");
        update_status();
        on_change();
        return 0;
    }

    void on_change() {
        if (m_callback.is_valid()) {
            m_callback->on_state_changed();
        }
    }

    void update_status() {
        if (m_pending_user.empty()) {
            uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_STATUS, "Not signed in");
            enable_sign_out(false);
        } else {
            uSetDlgItemText(m_hWnd, IDC_STATIC_APPLE_MUSIC_STATUS, "Signed in");
            enable_sign_out(true);
        }
    }

    void enable_sign_out(bool enable) {
        const HWND button = GetDlgItem(IDC_BUTTON_APPLE_MUSIC_SIGN_OUT);
        if (button != nullptr) {
            EnableWindow(button, enable ? TRUE : FALSE);
        }
    }

    bool has_changes() const {
        return m_pending_developer != m_initial_developer || m_pending_user != m_initial_user;
    }

    std::string m_initial_developer;
    std::string m_initial_user;
    std::string m_pending_developer;
    std::string m_pending_user;
};

preferences_page_factory_t<apple_music_preferences_page> g_factory;

} // namespace

} // namespace foo_apple_music

