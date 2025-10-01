#include <cstdint>
#include <stdexcept>
#include <string>

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

        t_uint32 get_version() override
        {
            return foobar2000_client::FOOBAR2000_CLIENT_VERSION;
        }

        pservice_factory_base get_service_list() override
        {
            return nullptr;
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
        }

        bool is_debug() override
        {
#ifdef _DEBUG
            return true;
#else
            return false;
#endif
        }

    private:
        std::string profile_path_;
        std::string module_name_;
    };

    struct client_context
    {
        placeholder_client client;
        HINSTANCE instance = nullptr;
    };

    client_context g_client_context;
} // namespace fb2k_placeholder
} // namespace

void foo_apple_music_placeholder()
{
    throw std::logic_error("foo_apple_music placeholder - replace with actual implementation");
}

// foobar2000 validates components by looking for an exported foobar2000_get_interface symbol.
// The placeholder implementation does not provide any usable functionality yet, but exporting
// the expected entry point keeps the test plugin loadable by foobar2000.
FB2K_EXPORT foobar2000_client* foobar2000_get_interface(foobar2000_api* api, HINSTANCE instance)
{
    fb2k_placeholder::api_storage() = api;
    fb2k_placeholder::g_client_context.instance = instance;
    return &fb2k_placeholder::g_client_context.client;
}

#undef FB2K_EXPORT
