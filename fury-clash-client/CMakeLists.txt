cmake_minimum_required(VERSION 3.20)

# ── iOS detection must happen before project() ───────────────────────────
# When CMAKE_SYSTEM_NAME=iOS is passed, enable ObjC alongside C.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    project(FuryClash VERSION 0.1.0 LANGUAGES C OBJC)
    set(FC_PLATFORM_IOS TRUE)
else()
    project(FuryClash VERSION 0.1.0 LANGUAGES C)
    set(FC_PLATFORM_IOS FALSE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── SDL3 ─────────────────────────────────────────────────────────────────
find_package(SDL3 REQUIRED CONFIG)

# ── Sources ───────────────────────────────────────────────────────────────
file(GLOB_RECURSE CLIENT_SOURCES
    "src/*.c"
    "src/**/*.c"
)

# Always exclude platform-specific files from the default glob
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "platform_ios\\.m$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "platform_android\\.c$")

if(FC_PLATFORM_IOS)
    # iOS: replace the SDL3 desktop platform shim with the UIKit ObjC version
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "platform_sdl3\\.c$")
    list(APPEND CLIENT_SOURCES src/platform/platform_ios.m)
endif()

# ── Executable / App bundle ───────────────────────────────────────────────
add_executable(fury-clash ${CLIENT_SOURCES})

target_include_directories(fury-clash PRIVATE src)

# ── iOS-specific configuration ────────────────────────────────────────────
if(FC_PLATFORM_IOS)
    target_compile_definitions(fury-clash PRIVATE FC_PLATFORM_IOS=1)

    # Required Apple frameworks
    target_link_libraries(fury-clash PRIVATE
        SDL3::SDL3
        "-framework UIKit"
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreHaptics"
        "-framework CoreMotion"
        "-framework GameController"
        "-framework Metal"
        "-framework QuartzCore"
    )

    # App bundle properties consumed by Xcode generator
    set_target_properties(fury-clash PROPERTIES
        MACOSX_BUNDLE                       TRUE
        MACOSX_BUNDLE_INFO_PLIST            "${CMAKE_SOURCE_DIR}/platform/ios/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME           "Fury Clash"
        MACOSX_BUNDLE_BUNDLE_VERSION        "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING  "0.1.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER        "com.furyclash.game"

        # Xcode-specific signing & deployment attributes
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER   "com.furyclash.game"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY      "1,2"    # iPhone + iPad
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET  "15.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE             "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM            ""        # set in CI or Xcode UI
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY          "iPhone Developer"
        XCODE_ATTRIBUTE_ENABLE_BITCODE              "NO"      # bitcode deprecated iOS 16
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC       "YES"
    )

    # Embed the launch screen storyboard into the app bundle
    target_sources(fury-clash PRIVATE
        "${CMAKE_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )

    # Embed assets into the bundle
    file(GLOB_RECURSE ASSET_FILES
        "${CMAKE_SOURCE_DIR}/assets/*"
    )
    foreach(ASSET ${ASSET_FILES})
        # Preserve directory structure under Resources/assets/
        file(RELATIVE_PATH REL "${CMAKE_SOURCE_DIR}/assets" "${ASSET}")
        get_filename_component(ASSET_DIR "${REL}" DIRECTORY)
        target_sources(fury-clash PRIVATE "${ASSET}")
        set_source_files_properties("${ASSET}"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets/${ASSET_DIR}"
        )
    endforeach()

else()
    # ── Desktop (Linux / macOS) ────────────────────────────────────────
    target_link_libraries(fury-clash PRIVATE SDL3::SDL3 m)

    target_compile_options(fury-clash PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-g -O0 -fsanitize=address>
    )
    target_link_options(fury-clash PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
    )

    # ── Tests (desktop only) ───────────────────────────────────────────
    enable_testing()
    foreach(TEST rollback hitbox input_motion serializer)
        add_executable(test_${TEST} tests/test_${TEST}.c)
        target_include_directories(test_${TEST} PRIVATE src)
        target_link_libraries(test_${TEST} PRIVATE SDL3::SDL3 m)
        add_test(NAME ${TEST} COMMAND test_${TEST})
    endforeach()

    # ── Install (desktop only) ─────────────────────────────────────────
    install(TARGETS fury-clash DESTINATION bin)
    install(DIRECTORY assets/ DESTINATION share/fury-clash/assets)
endif()

# ── Compile definitions (shared) ──────────────────────────────────────────
target_compile_definitions(fury-clash PRIVATE
    $<$<CONFIG:Debug>:FC_DEBUG>
)
