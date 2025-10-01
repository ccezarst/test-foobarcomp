#pragma once

#include <string>
#include <optional>

namespace foo_apple_music {

struct token_bundle {
    std::string developer_token;
    std::string music_user_token;
};

class apple_music_auth_manager {
public:
    static apple_music_auth_manager& instance();

    void set_developer_token(std::string token);
    void set_music_user_token(std::string token);

    [[nodiscard]] std::optional<token_bundle> tokens() const;
    [[nodiscard]] std::string developer_token_value() const;
    [[nodiscard]] std::string music_user_token_value() const;
    [[nodiscard]] bool has_music_user_token() const;

    void clear();
    void clear_user_token();

private:
    apple_music_auth_manager() = default;

    std::optional<token_bundle> m_tokens;
};

} // namespace foo_apple_music
