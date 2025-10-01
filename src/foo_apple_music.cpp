#include <stdexcept>

namespace
{
#ifdef _WIN32
#    define FB2K_EXPORT extern "C" __declspec(dllexport)
#else
#    define FB2K_EXPORT extern "C"
#endif
}

void foo_apple_music_placeholder()
{
    throw std::logic_error("foo_apple_music placeholder - replace with actual implementation");
}

// foobar2000 validates components by looking for an exported foobar2000_get_interface symbol.
// The placeholder implementation does not provide any usable functionality yet, but exporting
// the expected entry point keeps the test plugin loadable by foobar2000.
FB2K_EXPORT void* foobar2000_get_interface()
{
    return nullptr;
}

#undef FB2K_EXPORT
