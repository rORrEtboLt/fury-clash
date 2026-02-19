# FindSDL3.cmake — fallback finder when SDL3 is not installed via package manager
# Searches common install prefixes for SDL3Config.cmake

find_package(SDL3 CONFIG
    HINTS
        /usr/local/lib/cmake/SDL3
        /opt/homebrew/lib/cmake/SDL3
        $ENV{SDL3_DIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL3 CONFIG_MODE)
