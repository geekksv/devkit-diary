#!/bin/sh
# devkit-diary, chapter 9 — puts the pet on this computer (macOS or Linux).
#
#   curl -fsSL http://pet.local/i | sh
#
# Run once. It makes a private Python environment in ~/.devkit-pet, installs the window toolkit
# (PyQt6) there, fetches the helper from the board, and sets it to start by itself at login.
# Nothing is installed system-wide. To remove it all:  curl -fsSL http://pet.local/u | sh
#
# The board fills in its own addresses below when it serves this file.

main() {
  set -e
  HOSTS="%%HOSTS%%"
  DIR="$HOME/.devkit-pet"
  bold() { printf '\033[1m%s\033[0m\n' "$1"; }

  bold "Putting the pet on this computer"
  mkdir -p "$DIR"

  if ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)' >/dev/null 2>&1; then
    if [ "$(uname)" = "Darwin" ]; then
      echo "Python 3 isn't set up yet. macOS will offer to install the developer tools —"
      echo "click Install, wait for it to finish, then run this same command again."
      xcode-select --install >/dev/null 2>&1 || true
    else
      echo "Please install Python 3.9 or newer (for example: sudo apt install python3 python3-venv),"
      echo "then run this same command again."
    fi
    exit 1
  fi

  if [ ! -x "$DIR/venv/bin/python" ]; then
    echo "  making a private Python environment"
    python3 -m venv "$DIR/venv" || { echo "Couldn't make it — on Debian/Ubuntu: sudo apt install python3-venv"; exit 1; }
  fi
  echo "  installing the window toolkit (one-off download, a minute or two)"
  "$DIR/venv/bin/python" -m pip install -q --disable-pip-version-check --upgrade pip >/dev/null 2>&1 || true
  "$DIR/venv/bin/python" -m pip install -q --disable-pip-version-check "PyQt6>=6.4"

  echo "  fetching the helper from the board"
  GOT=""
  for H in $HOSTS; do
    if curl -fsS --max-time 8 "http://$H/devkit_pet.py" -o "$DIR/devkit_pet.py.new" 2>/dev/null; then GOT="$H"; break; fi
  done
  [ -n "$GOT" ] || { echo "Couldn't reach the board at: $HOSTS — is this computer on the same WiFi?"; exit 1; }
  mv "$DIR/devkit_pet.py.new" "$DIR/devkit_pet.py"
  echo "$HOSTS" > "$DIR/board"

  RUN="$DIR/venv/bin/python"
  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.pet.plist"
    mkdir -p "$HOME/Library/LaunchAgents"
    cat > "$PL" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>diary.devkit.pet</string>
  <key>ProgramArguments</key><array><string>$RUN</string><string>$DIR/devkit_pet.py</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><dict><key>SuccessfulExit</key><false/></dict>
  <key>ProcessType</key><string>Interactive</string>
  <key>StandardOutPath</key><string>$DIR/pet.log</string>
  <key>StandardErrorPath</key><string>$DIR/pet.log</string>
</dict></plist>
PLIST
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    sleep 1
    launchctl bootstrap "gui/$(id -u)" "$PL"
  else
    # Linux: X11 lets a window place itself; under Wayland the helper runs through XWayland.
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/devkit-pet.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=devkit pet
Exec=env QT_QPA_PLATFORM=xcb $RUN $DIR/devkit_pet.py
X-GNOME-Autostart-enabled=true
NoDisplay=true
DESK
    pkill -f "$DIR/devkit_pet.py" >/dev/null 2>&1 || true
    sleep 1
    QT_QPA_PLATFORM=xcb nohup "$RUN" "$DIR/devkit_pet.py" >"$DIR/pet.log" 2>&1 &
  fi

  bold "Done. The pet can visit this computer now — and the helper starts by itself from now on."
  if [ "$(uname)" = "Darwin" ]; then
    echo "If macOS asks to let Python find devices on your local network, click Allow."
  fi
  echo "To remove it later:  curl -fsSL http://$GOT/u | sh"
}

main "$@"
