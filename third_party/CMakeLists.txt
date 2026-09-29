# Libraries used unmodified from upstream, as git submodules.

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann-json/single_include)

# Boost.Regex is header-only and runs without the rest of Boost.
add_library(boost_regex INTERFACE)
target_include_directories(boost_regex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
target_compile_definitions(boost_regex INTERFACE BOOST_REGEX_STANDALONE)

add_subdirectory(qrcodegen)

# Qt's WebP image plugin, built against the system libwebp. Discord's media
# proxy serves previews as WebP, and Ubuntu Touch has no WebP plugin for
# its Qt 6 yet. The app loads it from lib/<triplet>/plugins/imageformats.
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp libwebpdemux libwebpmux)

set(QWEBP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qtimageformats/src/plugins/imageformats/webp)
add_library(qwebp MODULE
    ${QWEBP_DIR}/main.cpp
    ${QWEBP_DIR}/qwebphandler.cpp
    ${QWEBP_DIR}/qwebphandler_p.h
)
target_link_libraries(qwebp PRIVATE Qt6::Core Qt6::Gui PkgConfig::WEBP)
set_target_properties(qwebp PROPERTIES
    AUTOMOC ON
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/imageformats
)
if(CLICK_MODE)
    install(TARGETS qwebp LIBRARY DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/plugins/imageformats)
endif()
