# Libraries used unmodified from upstream, as git submodules.

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann-json/single_include)

# Boost.Regex is header-only and runs without the rest of Boost.
add_library(boost_regex INTERFACE)
target_include_directories(boost_regex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
target_compile_definitions(boost_regex INTERFACE BOOST_REGEX_STANDALONE)

add_subdirectory(qrcodegen)
