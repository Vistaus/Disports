# Disports

A Discord client built for Ubuntu Touch.

> [!IMPORTANT]
> Using any unofficial Discord client is against Discord's [Terms of Service](https://discord.com/terms) and may result in your account being restricted or permanently banned. **Use Disports at your own risk.**

## Installation

Disports can currently be installed on your Ubuntu Touch device via the [Open-Store](https://open-store.io/app/disports.jukfiuu), [GitHub releases](https://github.com/jukfiuune/Disports/releases) or by building it yourself using [Clickable](https://clickable-ut.dev). It needs Ubuntu Touch 24.04-2.x or newer.

Updating from Disports 0.8 keeps you signed in and keeps your settings.

## Logging In

### QR Code Login (Recommended)

This is the safest and easiest way to sign in. It works just like scanning a QR code in the official Discord client.

1. Open Disports. The sign-in page shows a QR code.
2. On an Android or iOS device, open Discord and go to:
   **User Settings -> Scan QR Code**
3. Point your camera at the QR code shown in Disports.
4. Confirm the login on your device - you're in!

### Email and Password Login (Not Recommended)

> [!WARNING]
> Signing in with your password from an unofficial client makes Discord more suspicious of your account. Use the QR code if you can.

1. On the sign-in page, tap **Sign in with a password (not recommended)**.
2. Enter your email or phone number and your password, and tap **Sign in**.
3. Discord will most likely ask you to prove you're human: solve the check that appears.
4. If your account has two-factor authentication, enter the code from your authenticator app, a backup code, or tap **Text me a code instead** to get one by SMS.

Your password is only sent to Discord and is never saved. Accounts that only use a security key for two-factor authentication can't sign in this way.

### Token Login (Not Recommended)

> [!WARNING]
> Token login is strongly discouraged. Your token is essentially your password - sharing or exposing it can compromise your account. It also means two devices will share the same session, which Discord may flag as suspicious. Only use this method if QR code login isn't an option for you.

If you still need to use token login, here's how to find your token:

1. Open [Discord](https://discord.com/app) in a desktop browser and log in.
2. Press <kbd>F12</kbd> to open the browser's developer tools.
3. Go to the **Network** tab, then press <kbd>F5</kbd> to reload the page.
4. In the filter/search box, type `discord api`.
5. Click on any request that appears, and look in the **Request Headers** section.
6. Find the `Authorization` header - its value is your token.
7. In Disports, tap **Paste a token instead (not recommended)**, paste it, and tap **Sign in**.

## Planned features

- [ ] Nitro features
- [ ] Notifications

## Translating

See [TRANSLATING.md](TRANSLATING.md).

## Warnings

- **Terms of Service** - Discord does not officially support third-party clients. Using Disports may violate Discord's Terms of Service and could lead to your account being restricted or banned. Disports tries to behave as closely to the official client as possible to minimise this risk, but no guarantees can be made.

- **Token security** - If you use token login, treat your token like a password. Never share it with anyone or paste it into websites or apps you don't trust.

- **No affiliation** - Disports is an independent project and is not affiliated with, endorsed by, or supported by Discord Inc.

## Credits

- The Discord client library in `src/discord/` is based on the core of [Discord Messenger](https://github.com/DiscordMessenger/dm) by iProgramInCpp, used under the MIT license (see [src/discord/LICENSE.DiscordMessenger](src/discord/LICENSE.DiscordMessenger)).
- [nlohmann/json](https://github.com/nlohmann/json), [Boost.Regex](https://github.com/boostorg/regex), [QR Code generator](https://github.com/nayuki/QR-Code-generator) and [Qt Image Formats](https://github.com/qt/qtimageformats).
- Lottie stickers are drawn with [rlottie](https://github.com/Samsung/rlottie), LGPL-2.1+, shipped unchanged as its own library.
- Voice calls use [libdave](https://github.com/discord/libdave) and [MLSpp](https://github.com/cisco/mlspp) for end-to-end encryption, [Opus](https://opus-codec.org), [WebRTC Audio Processing](https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing) (with [Abseil](https://github.com/abseil/abseil-cpp)) and [RNNoise](https://github.com/xiph/rnnoise).

## License

Disports is released under the MIT license, see [LICENSE](LICENSE).
