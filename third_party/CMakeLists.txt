# Libraries used unmodified from upstream, as git submodules.

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann-json/single_include)

# Boost.Regex is header-only and runs without the rest of Boost.
add_library(boost_regex INTERFACE)
target_include_directories(boost_regex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
target_compile_definitions(boost_regex INTERFACE BOOST_REGEX_STANDALONE)

add_subdirectory(qrcodegen)
add_subdirectory(rnnoise)

# Abseil, only for webrtc-audio-processing below (Ubuntu's libabsl-dev can't
# be installed next to Clickable's cross compilers).
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(abseil-cpp EXCLUDE_FROM_ALL)
find_package(Threads REQUIRED)
add_subdirectory(webrtc-audio-processing)

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

# Voice end-to-end encryption (DAVE): Discord's libdave, on Cisco's MLS
# library. libdave's own CMake expects vcpkg; its sources are built here the
# way its vcpkg packaging builds them (OpenSSL 3, mlspp in namespace mlspp,
# no GREASE, no persistent keys).
set(MLS_CXX_NAMESPACE "mlspp" CACHE STRING "" FORCE)
set(DISABLE_GREASE ON CACHE BOOL "" FORCE)
set(TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(mlspp EXCLUDE_FROM_ALL)
# Both treat warnings as errors; a newer compiler must not break our build.
foreach(target mlspp bytes tls_syntax hpke mls_vectors mls_ds)
    if(TARGET ${target})
        target_compile_options(${target} PRIVATE -Wno-error)
    endif()
endforeach()

set(DAVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libdave/cpp)
file(GLOB_RECURSE DAVE_SOURCES CONFIGURE_DEPENDS ${DAVE_DIR}/src/*.cpp)
list(FILTER DAVE_SOURCES EXCLUDE REGEX ".*(persisted_key|bindings_|boringssl_).*")
list(APPEND DAVE_SOURCES ${DAVE_DIR}/src/mls/persisted_key_pair_null.cpp)
add_library(dave STATIC ${DAVE_SOURCES})
target_include_directories(dave PUBLIC ${DAVE_DIR}/includes PRIVATE ${DAVE_DIR}/src)
target_compile_definitions(dave PRIVATE WITH_OPENSSL3)
target_link_libraries(dave PUBLIC mlspp PRIVATE OpenSSL::Crypto nlohmann_json)
set_target_properties(dave PROPERTIES POSITION_INDEPENDENT_CODE ON)
