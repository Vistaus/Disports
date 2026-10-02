#pragma once

class Preferences;

// Disports 0.8 (the Qt 5 / Python version) had the same app id, so its
// files are in this version's folders. The first time this version starts,
// what it left is carried over and removed:
//   ~/.local/share/disports.jukfiuu/token          the sign-in
//   ~/.config/disports.jukfiuu/disports.jukfiuu.conf  its settings
//     (Qt.labs.settings: themeMode, inlineGifPlayback,
//      blockedMessageVisibility, maxComposerLines)
namespace Migration {

// Before the saved token is read (Session::start()).
void fromVersion08(Preferences* preferences);

}
