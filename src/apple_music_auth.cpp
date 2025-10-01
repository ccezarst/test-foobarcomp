#include "apple_music_auth.hpp"

#include <pfc/string8.h>
#include <foobar2000/SDK/configStore.h>
#include <foobar2000/helpers/cfg_var_modern.h>

using namespace foo_apple_music;

namespace {
    cfg_string cfg_developer_token("apple_music_developer_token", "");
    cfg_string cfg_user_token("apple_music_user_token", "");
}

apple_music_auth_manager& apple_music_auth_manager::instance() {
    static apple_music_auth_manager instance;
    return instance;
}

void apple_music_auth_manager::set_developer_token(std::string token) {
    cfg_developer_token = token.c_str();
    if (!m_tokens.has_value()) {
        m_tokens = token_bundle{};
    }
    m_tokens->developer_token = std::move(token);
}

void apple_music_auth_manager::set_music_user_token(std::string token) {
    cfg_user_token = token.c_str();
    if (!m_tokens.has_value()) {
        m_tokens = token_bundle{};
    }
    m_tokens->music_user_token = std::move(token);
}

std::optional<token_bundle> apple_music_auth_manager::tokens() const {
    if (m_tokens) {
        if (!m_tokens->developer_token.empty() && !m_tokens->music_user_token.empty()) {
            return m_tokens;
        }
    }

    const auto developer = cfg_developer_token.get_ptr();
    const auto user = cfg_user_token.get_ptr();
    if (developer && developer[0] != '\0' && user && user[0] != '\0') {
        token_bundle result;
        result.developer_token = developer;
        result.music_user_token = user;
        return result;
    }

    return std::nullopt;
}

std::string apple_music_auth_manager::developer_token_value() const {
    if (m_tokens && !m_tokens->developer_token.empty()) {
        return m_tokens->developer_token;
    }

    const auto developer = cfg_developer_token.get_ptr();
    return developer ? std::string(developer) : std::string();
}

std::string apple_music_auth_manager::music_user_token_value() const {
    if (m_tokens && !m_tokens->music_user_token.empty()) {
        return m_tokens->music_user_token;
    }

    const auto user = cfg_user_token.get_ptr();
    return user ? std::string(user) : std::string();
}

bool apple_music_auth_manager::has_music_user_token() const {
    if (m_tokens && !m_tokens->music_user_token.empty()) {
        return true;
    }

    const auto user = cfg_user_token.get_ptr();
    return user && user[0] != '\0';
}

void apple_music_auth_manager::clear() {
    cfg_developer_token = "";
    cfg_user_token = "";
    m_tokens.reset();
}

void apple_music_auth_manager::clear_user_token() {
    cfg_user_token = "";
    if (m_tokens) {
        m_tokens->music_user_token.clear();
        if (m_tokens->developer_token.empty()) {
            m_tokens.reset();
        }
    }
}
