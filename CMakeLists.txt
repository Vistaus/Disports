cmake_minimum_required(VERSION 3.21)
project(disports VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_NAME "disports")
set(APP_ID "disports.jukfiuu")

option(CLICK_MODE "Install as an Ubuntu Touch click package" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Network WebSockets)
find_package(OpenSSL REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(third_party)

# The client core, see src/discord/CMakeLists.txt.
add_subdirectory(src/discord)

qt_add_executable(disports
    src/main.cpp
    src/DiscordUrls.cpp src/DiscordUrls.h
    src/MessageFormatter.cpp src/MessageFormatter.h
    src/OfflineCache.cpp src/OfflineCache.h
    src/ImageCache.cpp src/ImageCache.h
    src/Preferences.cpp src/Preferences.h
    src/RemoteAuth.cpp src/RemoteAuth.h
    src/Session.cpp src/Session.h
    src/backend/CoreGlobals.cpp src/backend/CoreGlobals.h
    src/backend/QtFrontend.cpp src/backend/QtFrontend.h
    src/backend/QtHttpClient.cpp src/backend/QtHttpClient.h
    src/backend/QtWebsocketClient.cpp src/backend/QtWebsocketClient.h
    src/models/ChannelListModel.cpp src/models/ChannelListModel.h
    src/models/EmojiPickerModel.cpp src/models/EmojiPickerModel.h
    src/models/GuildListModel.cpp src/models/GuildListModel.h
    src/models/MessageListModel.cpp src/models/MessageListModel.h
    src/models/UnreadDmListModel.cpp src/models/UnreadDmListModel.h
    src/media/GstVideoPlayer.cpp src/media/GstVideoPlayer.h
)

# Video goes through GStreamer (hardware decoders on the phone via
# gstreamer1.0-droid), see src/media/GstVideoPlayer.h.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

qt_add_qml_module(disports
    URI Disports
    QML_FILES
        qml/Main.qml
        qml/LoginPage.qml
        qml/MainPage.qml
        qml/Sidebar.qml
        qml/SidebarIcon.qml
        qml/FolderPreviewIcon.qml
        qml/StatusDot.qml
        qml/ChannelPanel.qml
        qml/ChatPanel.qml
        qml/ChatPage.qml
        qml/MessageDelegate.qml
        qml/PanelHeader.qml
        qml/UnreadBadge.qml
        qml/ConnectionBanner.qml
        qml/SettingsPage.qml
        qml/EmojiPicker.qml
        qml/ReactionBar.qml
        qml/MediaPreview.qml
        qml/EmbedCard.qml
        qml/GifVideo.qml
        qml/VideoPlayer.qml
        qml/StickerView.qml
        qml/PollCard.qml
        qml/MediaViewerPage.qml
        qml/ChannelInfoPage.qml
)

qt_add_resources(disports "assets"
    PREFIX "/"
    BASE ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/assets/logo.svg
        ${PROJECT_SOURCE_DIR}/assets/send.svg
        ${PROJECT_SOURCE_DIR}/assets/emoji.svg
)

qt_add_resources(disports "data"
    PREFIX "/"
    FILES
        data/emoji.json
)

target_include_directories(disports PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(disports PRIVATE DISPORTS_VERSION="${PROJECT_VERSION}"
    DISPORTS_ARCH_TRIPLET="${CMAKE_LIBRARY_ARCHITECTURE}")

target_link_libraries(disports PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::WebSockets
    PkgConfig::GST
    OpenSSL::Crypto
    discord
    qrcodegen
)

install(TARGETS disports RUNTIME DESTINATION bin)

if(CLICK_MODE)
    configure_file(manifest.json.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.json)
    configure_file(disports.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/disports.desktop)
    install(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
            ${CMAKE_CURRENT_BINARY_DIR}/disports.desktop
            disports.apparmor
        DESTINATION .
    )
    install(DIRECTORY assets DESTINATION .)
endif()
