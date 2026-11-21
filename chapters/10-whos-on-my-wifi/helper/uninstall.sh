#!/bin/sh
# devkit-diary, chapter 10 — takes the inbox helper off this computer (macOS or Linux).
#   curl -fsSL http://scan.local/u | sh
main() {
  DIR="$HOME/.devkit-inbox"
  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.inbox.plist"
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    rm -f "$PL"
  else
    rm -f "$HOME/.config/autostart/devkit-inbox.desktop"
  fi
  pkill -f "$DIR/devkit_inbox.py" >/dev/null 2>&1 || true
  rm -rf "$DIR"
  echo "The inbox helper is gone from this computer. (The scanner on the board is untouched.)"
}
main "$@"
