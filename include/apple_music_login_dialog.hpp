#pragma once

#include <optional>
#include <string>

#include <windows.h>

namespace foo_apple_music {

std::optional<std::string> run_apple_music_login(HWND parent, const std::string& developer_token);

}

