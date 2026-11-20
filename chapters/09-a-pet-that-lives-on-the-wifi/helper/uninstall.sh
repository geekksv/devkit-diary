#!/bin/sh
# devkit-diary, chapter 9 — takes the pet helper off this computer (macOS or Linux).
#   curl -fsSL http://pet.local/u | sh
main() {
  DIR="$HOME/.devkit-pet"
  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.pet.plist"
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    rm -f "$PL"
  else
    rm -f "$HOME/.config/autostart/devkit-pet.desktop"
  fi
  pkill -f "$DIR/devkit_pet.py" >/dev/null 2>&1 || true
  rm -rf "$DIR"
  echo "The pet helper is gone from this computer. (The pet itself still lives on the board.)"
}
main "$@"
