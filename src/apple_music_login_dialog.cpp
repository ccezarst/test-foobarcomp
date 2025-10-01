#include "apple_music_login_dialog.hpp"

#include <pfc/stringcvt.h>

#include <atlbase.h>
#include <atlhost.h>
#include <atlwin.h>
#include <exdisp.h>
#include <mshtml.h>
#include <shlwapi.h>
#include <wininet.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <cwchar>

#include "resource.h"

extern CComModule _Module;

namespace foo_apple_music {

namespace {

constexpr wchar_t kTokenPrefix[] = L"AppleMusicToken:";

std::string escape_js(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\'':
            escaped += "\\'";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string build_login_html(const std::string& developer_token) {
    static constexpr char kTemplate[] = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <title>Apple Music Sign-In</title>
    <style>
        body { font-family: "Segoe UI", sans-serif; margin: 0; padding: 16px; background: #121212; color: #f5f5f5; }
        h1 { font-size: 20px; margin-bottom: 12px; }
        button { padding: 10px 16px; font-size: 15px; border-radius: 4px; border: none; cursor: pointer; }
        button.primary { background: #fa243c; color: #fff; margin-right: 12px; }
        button.secondary { background: #303030; color: #f5f5f5; }
        #status { margin-top: 16px; font-size: 13px; min-height: 18px; }
        a { color: #4aa3ff; }
    </style>
    <script src="https://js-cdn.music.apple.com/musickit/v3/musickit.js"></script>
</head>
<body>
    <h1>Sign in to Apple Music</h1>
    <p>Sign in with your Apple ID to authorise foobar2000 to access your Apple Music library.</p>
    <div>
        <button id="signIn" class="primary">Sign in</button>
        <button id="signOut" class="secondary">Sign out</button>
    </div>
    <div id="status">Waiting…</div>
    <p style="margin-top:24px;font-size:12px;opacity:0.7">After you complete sign-in the dialog closes automatically.</p>
    <script>
        (async function() {
            const developerToken = '__DEVELOPER_TOKEN__';
            try {
                MusicKit.configure({
                    developerToken,
                    app: {
                        name: 'foobar2000 Apple Music',
                        build: '1.0'
                    }
                });
            } catch (err) {
                document.getElementById('status').textContent = 'Configuration error: ' + err;
                return;
            }

            const music = MusicKit.getInstance();
            const status = document.getElementById('status');

            document.getElementById('signIn').addEventListener('click', async () => {
                status.textContent = 'Waiting for Apple ID sign-in…';
                try {
                    const token = await music.authorize();
                    const storefront = music.userStorefrontId || 'Apple Music user';
                    status.textContent = 'Signed in as ' + storefront;
                    document.title = 'AppleMusicToken:' + token;
                } catch (err) {
                    status.textContent = 'Sign-in failed: ' + err;
                }
            });

            document.getElementById('signOut').addEventListener('click', async () => {
                try {
                    await music.unauthorize();
                    status.textContent = 'Signed out';
                    document.title = 'AppleMusicToken:';
                } catch (err) {
                    status.textContent = 'Could not sign out: ' + err;
                }
            });
        })();
    </script>
</body>
</html>)";

    std::string html = kTemplate;
    const std::string escaped_token = escape_js(developer_token);
    const std::string placeholder = "__DEVELOPER_TOKEN__";
    if (const auto pos = html.find(placeholder); pos != std::string::npos) {
        html.replace(pos, placeholder.size(), escaped_token);
    }
    return html;
}

std::filesystem::path write_login_file(const std::string& developer_token) {
    std::error_code ec;
    auto temp_dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        temp_dir = std::filesystem::current_path(ec);
    }
    auto file = temp_dir / std::filesystem::path("foo_apple_music_login_" + std::to_string(GetCurrentProcessId()) + ".html");
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return {};
    }
    const std::string html = build_login_html(developer_token);
    out.write(html.data(), static_cast<std::streamsize>(html.size()));
    return file;
}

std::wstring path_to_url(const std::filesystem::path& path) {
    wchar_t buffer[INTERNET_MAX_URL_LENGTH] = {};
    DWORD size = std::size(buffer);
    if (FAILED(UrlCreateFromPathW(path.c_str(), buffer, &size, 0))) {
        return std::wstring();
    }
    return std::wstring(buffer);
}

class apple_music_login_dialog_impl final : public CDialogImpl<apple_music_login_dialog_impl> {
public:
    enum {IDD = IDD_APPLE_MUSIC_LOGIN};

    explicit apple_music_login_dialog_impl(std::string developer_token)
        : m_developer_token(std::move(developer_token)) {}

    std::optional<std::string> run(HWND parent) {
        INT_PTR code = DoModal(parent);
        if (code == IDOK) {
            return m_result_token;
        }
        return std::nullopt;
    }

    BEGIN_MSG_MAP(apple_music_login_dialog_impl)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
        AtlAxWinInit();

        m_login_file = write_login_file(m_developer_token);
        if (m_login_file.empty()) {
            EndDialog(IDCANCEL);
            return TRUE;
        }
        std::error_code exists_ec;
        if (!std::filesystem::exists(m_login_file, exists_ec)) {
            EndDialog(IDCANCEL);
            return TRUE;
        }

        CAxWindow ax(GetDlgItem(IDC_APPLE_MUSIC_BROWSER));
        CComPtr<IUnknown> unk;
        HRESULT hr = ax.CreateControlEx(L"Shell.Explorer", nullptr, nullptr, &unk, &m_browser_disp);
        if (FAILED(hr) || !unk) {
            EndDialog(IDCANCEL);
            return TRUE;
        }

        unk->QueryInterface(IID_PPV_ARGS(&m_browser));
        if (!m_browser) {
            EndDialog(IDCANCEL);
            return TRUE;
        }

        const std::wstring url = path_to_url(m_login_file);
        if (!url.empty()) {
            CComVariant vtEmpty;
            CComVariant vtUrl(url.c_str());
            m_browser->Navigate2(&vtUrl, &vtEmpty, &vtEmpty, &vtEmpty, &vtEmpty);
        }

        SetDlgItemText(IDC_APPLE_MUSIC_LOGIN_STATUS, L"Use your Apple ID to sign in.");
        m_timer_id = SetTimer(1, 400, nullptr);
        return TRUE;
    }

    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
        if (!m_browser) {
            return 0;
        }

        CComPtr<IDispatch> dispatch;
        if (FAILED(m_browser->get_Document(&dispatch)) || !dispatch) {
            return 0;
        }

        CComQIPtr<IHTMLDocument2> doc(dispatch);
        if (!doc) {
            return 0;
        }

        CComBSTR title;
        if (FAILED(doc->get_title(&title)) || !title) {
            return 0;
        }

        std::wstring title_str(title, title.Length());
        if (title_str.rfind(kTokenPrefix, 0) != 0) {
            return 0;
        }

        const size_t prefix_len = std::wcslen(kTokenPrefix);
        if (title_str.size() <= prefix_len) {
            return 0;
        }
        const std::wstring token_w = title_str.substr(prefix_len);
        std::string token = pfc::stringcvt::string_utf8_from_wide(token_w.c_str());
        if (!token.empty()) {
            m_result_token = std::move(token);
            EndDialog(IDOK);
        }
        return 0;
    }

    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
        if (m_timer_id != 0) {
            KillTimer(m_timer_id);
            m_timer_id = 0;
        }
        if (!m_login_file.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_login_file, ec);
        }
        m_browser.Release();
        m_browser_disp.Release();
        return 0;
    }

    LRESULT OnCancel(WORD, WORD, HWND, BOOL&) {
        EndDialog(IDCANCEL);
        return 0;
    }

    std::string m_developer_token;
    std::optional<std::string> m_result_token;
    std::filesystem::path m_login_file;
    UINT_PTR m_timer_id = 0;
    CComPtr<IDispatch> m_browser_disp;
    CComPtr<IWebBrowser2> m_browser;
};

} // namespace

std::optional<std::string> run_apple_music_login(HWND parent, const std::string& developer_token) {
    apple_music_login_dialog_impl dlg(developer_token);
    return dlg.run(parent);
}

} // namespace foo_apple_music

