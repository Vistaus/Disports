# Tests on the PC

The app runs in a container (`localhost/disports-test-gl`, on a virtual
screen) against a fake Discord server, `fake_discord.py`: a server with
roles and channels set up to exercise permissions (a hidden channel, a
read-only one, no history, no files, slowmode), members to mention, a voice
channel with people in it, and a DM with a call going on. It follows
Discord's two-step uploads and logs what it receives.

- `check.sh`: builds the app with AddressSanitizer, LeakSanitizer and UBSan
  (in `build/asan/`) and runs every scenario; fails on memory errors,
  undefined behaviour, leaks of our code (not the libraries' leftovers at
  exit, see `leaks.py`), unclean exits, or requests the server never got.
  `check.sh --no-build mentions upload` reruns some without building.
- `run.sh <name> <ms> [VAR=value...]`: one run with a screenshot, for looking
  at things; `STEPS` drives it with xdotool. Output in `build/test/<name>/`.
- `server.sh start|stop|restart`: the fake server (`build/test/fake.log`).
- `build-image.sh`: makes the test image, after a first
  `clickable build --arch amd64`.

The app is driven through test hooks (`DISPORTS_OPEN_CHANNEL`,
`DISPORTS_SEND_FILE`, `DISPORTS_SCREENSHOT`, ...), listed in
`src/testing/TestHooks.h`. They are only built with
`-DDISPORTS_TEST_HOOKS=ON`, which `check.sh` sets; release builds don't
have them.
