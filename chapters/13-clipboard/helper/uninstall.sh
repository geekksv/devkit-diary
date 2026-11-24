#!/bin/sh
# devkit-diary, chapter 13: takes the clipboard helper off this computer (macOS or Linux).
#
#   curl -fsSL http://clip.local/u | sh
#
# It tells the board this computer is leaving the group, stops the helper, and deletes
# ~/.devkit-clip (the helper and its keys) and its start-at-login entry.

main() {
  DIR="$HOME/.devkit-clip"
  if [ -f "$DIR/devkit_clip.py" ]; then
    python3 "$DIR/devkit_clip.py" --leave >/dev/null 2>&1 || true
  fi
  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.clip.plist"
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    rm -f "$PL"
  else
    rm -f "$HOME/.config/autostart/devkit-clip.desktop"
  fi
  pkill -f "$DIR/devkit_clip.py" >/dev/null 2>&1 || true
  rm -rf "$DIR"
  printf '\033[1m%s\033[0m\n' "The clipboard helper is gone from this computer, with its keys."
}

main "$@"
