# foo_apple_music

`foo_apple_music` is an experimental foobar2000 component that integrates Apple Music playlists directly into the foobar2000 media library. The component uses the Apple Music REST API and the MusicKit media token system to mirror the user's playlists as foobar2000 autoplaylists and to fetch lossless HLS streams for playback through the standard input framework.

> **Important:** This component is provided for educational purposes. You must have an active Apple Music subscription and obtain your own Apple Music developer token and music-user-token. Respect Apple's terms of service and regional licensing restrictions.

## Features

- Authenticating with Apple Music via MusicKit developer and user tokens using an in-component sign-in dialog.
- Syncing Apple Music playlists into foobar2000 and keeping them updated on start-up and on demand.
- Streaming AAC or ALAC HLS tracks from Apple Music using foobar2000's input services.
- Simple caching layer to avoid repeated Apple Music API calls.

## Project layout

```
.
├── CMakeLists.txt            # CMake build script that wraps the foobar2000 SDK
├── README.md                 # This documentation
├── include/
│   ├── apple_music_auth.hpp  # Token management utilities
│   ├── apple_music_client.hpp# Apple Music REST client wrappers
│   ├── apple_music_models.hpp# Plain data models shared between modules
│   └── playlist_sync_service.hpp
├── src/
│   ├── apple_music_auth.cpp
│   ├── apple_music_client.cpp
│   ├── apple_music_input.cpp
│   ├── playlist_sync_service.cpp
│   ├── preferences.cpp         # Preferences page plumbing
│   ├── apple_music_login_dialog.cpp # Embedded MusicKit sign-in dialog
│   └── component_entry.cpp
├── res/
│   ├── foo_apple_music.rc      # Dialog resources
│   └── resource.h
└── thirdparty/
    └── foobar2000/          # Expected location for the foobar2000 SDK submodule
```

> The repository expects the foobar2000 SDK to be present under `thirdparty/foobar2000`. Either add it as a Git submodule or copy the SDK manually before building.

## Building

1. Install Visual Studio 2022 with the Desktop development with C++ workload.
2. Install CMake 3.24 or newer.
3. Clone the repository and obtain the foobar2000 SDK (>= 2023-09-26).
4. Configure the build from a "x64 Native Tools" shell:

   ```bat
   git submodule update --init --recursive
   cmake -S . -B build -DFOOBAR2000_SDK_DIR=thirdparty/foobar2000 -A x64
   cmake --build build --config Release
   ```

5. Copy `build/Release/foo_apple_music.fb2k-component` to your foobar2000 `components/` directory.

## Obtaining tokens

- **Developer token**: Create an App in [Apple Developer](https://developer.apple.com/music/) portal and generate a private key. Use the provided PowerShell snippet in `docs/token_helper.ps1` to create a signed JWT.
- **Music user token**: Install the component, launch foobar2000, and open *File → Preferences → Tools → Apple Music*. Enter your developer token, press **Log in to Apple Music…**, and complete the Apple ID prompt inside foobar2000. The component stores the returned music-user-token automatically.

Both tokens are stored using foobar2000's configuration store and encrypted with Windows DPAPI.

## Usage

1. After signing in through the Apple Music preferences page, invoke *Library → Sync Apple Music Playlists*.
2. The command creates autoplaylists named ` <Playlist Name>` for every playlist you have in Apple Music.
3. Select a playlist and press play. The input service requests a signed HLS stream and hands it to the built-in FFmpeg decoder.

## Limitations

- Apple Music playback URLs expire after a few minutes; the component refreshes them automatically when playback starts.
- High-resolution lossless streaming is only available on Windows 11 build 22000 or later.
- Family accounts and shared playlists are not currently supported.
- The project ships without the foobar2000 SDK for licensing reasons.

## License

Distributed under the MIT License. See `LICENSE` for more information.

## Status

This project is an early proof-of-concept. Expect bugs, missing error handling, and potential incompatibilities with future Apple Music API changes.
