# The client core: Discord's protocol, state and caches in plain C++,
# without Qt. The app implements its Frontend, HTTPClient and
# WebsocketClient (src/backend/).

file(GLOB_RECURSE DISCORD_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(discord STATIC ${DISCORD_SOURCES})

target_include_directories(discord PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(discord PUBLIC nlohmann_json boost_regex)
target_compile_features(discord PUBLIC cxx_std_17)
set_target_properties(discord PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)

# The inherited code has not been through a warning cleanup yet; turn
# warnings back on file by file as it gets reworked.
target_compile_options(discord PRIVATE -w)
