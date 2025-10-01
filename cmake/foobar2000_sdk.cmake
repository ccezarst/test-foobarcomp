# Helper to expose the foobar2000 SDK as a CMake target without relying on an embedded
# CMake project inside the vendor tree. Consumers must set FOOBAR2000_SDK_DIR to the root
# of the unpacked foobar2000 SDK before including this file.

cmake_policy(PUSH)
cmake_policy(SET CMP0071 NEW)

if(TARGET foobar2000_sdk)
    cmake_policy(POP)
    return()
endif()

if(NOT DEFINED FOOBAR2000_SDK_DIR)
    message(FATAL_ERROR "FOOBAR2000_SDK_DIR must be defined before including foobar2000_sdk.cmake")
endif()

set(_fb2k_sdk_root "${FOOBAR2000_SDK_DIR}")
if(NOT IS_DIRECTORY "${_fb2k_sdk_root}")
    message(FATAL_ERROR "FOOBAR2000_SDK_DIR='${_fb2k_sdk_root}' is not a directory")
endif()

set(_fb2k_include_dirs
    "${_fb2k_sdk_root}/SDK"
    "${_fb2k_sdk_root}/pfc"
    "${_fb2k_sdk_root}/foobar2000/ATLHelpers"
)

# Normalise include directories so that missing folders do not trigger warnings later.
set(_fb2k_existing_include_dirs "")
foreach(_dir IN LISTS _fb2k_include_dirs)
    if(IS_DIRECTORY "${_dir}")
        list(APPEND _fb2k_existing_include_dirs "${_dir}")
    endif()
endforeach()
if(NOT _fb2k_existing_include_dirs)
    message(FATAL_ERROR "None of the expected SDK include directories were found under '${_fb2k_sdk_root}'")
endif()

set(_fb2k_lib_dir "${_fb2k_sdk_root}/lib/${CMAKE_VS_PLATFORM_NAME}")
set(_fb2k_import_names
    pfc
    foobar2000_sdk_helpers
    foobar2000_component_client
)

set(_fb2k_can_use_imported TRUE)
if(NOT EXISTS "${_fb2k_lib_dir}")
    set(_fb2k_can_use_imported FALSE)
endif()

foreach(_lib IN LISTS _fb2k_import_names)
    if(NOT _fb2k_can_use_imported)
        break()
    endif()
    set(_release_candidate "${_fb2k_lib_dir}/${_lib}.lib")
    if(NOT EXISTS "${_release_candidate}")
        set(_fb2k_can_use_imported FALSE)
    endif()
endforeach()

if(_fb2k_can_use_imported)
    add_library(foobar2000_sdk INTERFACE)
    target_include_directories(foobar2000_sdk INTERFACE ${_fb2k_existing_include_dirs})

    foreach(_lib IN LISTS _fb2k_import_names)
        set(_import_target "foobar2000_sdk_${_lib}")
        if(TARGET "${_import_target}")
            continue()
        endif()

        add_library("${_import_target}" STATIC IMPORTED GLOBAL)
        set_target_properties("${_import_target}" PROPERTIES
            IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
        )

        set(_release_path "${_fb2k_lib_dir}/${_lib}.lib")
        set_target_properties("${_import_target}" PROPERTIES
            IMPORTED_LOCATION "${_release_path}"
            IMPORTED_LOCATION_RELEASE "${_release_path}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_release_path}"
            IMPORTED_LOCATION_MINSIZEREL "${_release_path}"
        )

        # Try to locate a dedicated debug library. Common naming conventions include suffixes `_d` and `d`.
        set(_debug_path "")
        foreach(_suffix "_d" "d" "D")
            if(_debug_path)
                break()
            endif()
            set(_candidate "${_fb2k_lib_dir}/${_lib}${_suffix}.lib")
            if(EXISTS "${_candidate}")
                set(_debug_path "${_candidate}")
            endif()
        endforeach()

        if(_debug_path)
            set_target_properties("${_import_target}" PROPERTIES IMPORTED_LOCATION_DEBUG "${_debug_path}")
        else()
            # Fall back to the release library for debug configurations if no dedicated binary exists.
            set_target_properties("${_import_target}" PROPERTIES IMPORTED_LOCATION_DEBUG "${_release_path}")
        endif()

        target_link_libraries(foobar2000_sdk INTERFACE "${_import_target}")
    endforeach()
else()
    file(GLOB_RECURSE _fb2k_pfc_sources CONFIGURE_DEPENDS "${_fb2k_sdk_root}/pfc/*.cpp")
    file(GLOB_RECURSE _fb2k_atlhelpers_sources CONFIGURE_DEPENDS "${_fb2k_sdk_root}/foobar2000/ATLHelpers/*.cpp")
    file(GLOB _fb2k_component_sources CONFIGURE_DEPENDS "${_fb2k_sdk_root}/SDK/foobar2000_component_client/*.cpp")

    set(_fb2k_all_sources
        ${_fb2k_pfc_sources}
        ${_fb2k_atlhelpers_sources}
        ${_fb2k_component_sources}
    )

    if(NOT _fb2k_all_sources)
        message(FATAL_ERROR "No foobar2000 SDK sources were found. Ensure the SDK layout matches expectations.")
    endif()

    add_library(foobar2000_sdk STATIC ${_fb2k_all_sources})
    target_include_directories(foobar2000_sdk PUBLIC ${_fb2k_existing_include_dirs})
endif()

cmake_policy(POP)
