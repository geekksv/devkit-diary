#!/bin/sh
# devkit-diary, chapter 13: puts this computer on your shared clipboard (macOS or Linux).
#
#   curl -fsSL http://clip.local/i | sh
#
# Run once. It fetches the clipboard helper from the board into ~/.devkit-clip, asks this
# computer's name and your passphrase (in a small window, or here), and sets the helper to start
# by itself at login. It uses the Python already on this computer, standard library only: nothing
# is downloaded from the internet and nothing is installed system-wide.
#
# The board fills in its own addresses below when it serves this file.

main() {
  set -e
  HOSTS="%%HOSTS%%"
  DIR="$HOME/.devkit-clip"
  bold() { printf '\033[1m%s\033[0m\n' "$1"; }

  bold "Putting this computer on your shared clipboard"
  mkdir -p "$DIR"

  if ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
    if [ "$(uname)" = "Darwin" ]; then
      echo "Python 3 isn't set up yet. macOS will offer to install the developer tools:"
      echo "click Install, wait for it to finish, then run this same command again."
      xcode-select --install >/dev/null 2>&1 || true
    else
      echo "Please install Python 3.8 or newer (for example: sudo apt install python3), then run this again."
    fi
    exit 1
  fi
  PY="$(command -v python3)"
  if [ "$(uname)" != "Darwin" ] && ! command -v wl-paste >/dev/null 2>&1 && ! command -v xclip >/dev/null 2>&1; then
    echo "The helper needs wl-clipboard (Wayland) or xclip (X11) to reach the clipboard, for example:"
    echo "  sudo apt install wl-clipboard xclip"
    exit 1
  fi

  echo "  fetching the helper from the board"
  GOT=""
  for H in $HOSTS; do
    # The board keeps these gzipped, to fit in its flash.
    if curl -fsS --max-time 8 "http://$H/devkit_clip.py.gz" | gunzip > "$DIR/devkit_clip.py.new" 2>/dev/null &&
       curl -fsS --max-time 8 "http://$H/clipcrypto.py.gz" | gunzip > "$DIR/clipcrypto.py.new" 2>/dev/null &&
       [ -s "$DIR/devkit_clip.py.new" ] && [ -s "$DIR/clipcrypto.py.new" ]; then GOT="$H"; break; fi
  done
  [ -n "$GOT" ] || { echo "Couldn't reach the board at: $HOSTS. Is this computer on the same WiFi?"; exit 1; }
  mv "$DIR/devkit_clip.py.new" "$DIR/devkit_clip.py"
  mv "$DIR/clipcrypto.py.new" "$DIR/clipcrypto.py"
  echo "$HOSTS" > "$DIR/board"

  if [ ! -f "$DIR/config.json" ]; then
    echo "  joining your clipboard group"
    "$PY" "$DIR/devkit_clip.py" --setup </dev/tty || { echo "Setup didn't finish. Run this command again to try again."; exit 1; }
  fi

  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.clip.plist"
    mkdir -p "$HOME/Library/LaunchAgents"
    cat > "$PL" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>diary.devkit.clip</string>
  <key>ProgramArguments</key><array><string>$PY</string><string>$DIR/devkit_clip.py</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><dict><key>SuccessfulExit</key><false/></dict>
  <key>ProcessType</key><string>Interactive</string>
  <key>StandardOutPath</key><string>$DIR/clip.out</string>
  <key>StandardErrorPath</key><string>$DIR/clip.out</string>
</dict></plist>
PLIST
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    sleep 1
    launchctl bootstrap "gui/$(id -u)" "$PL"
  else
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/devkit-clip.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=devkit clipboard
Exec=$PY $DIR/devkit_clip.py
X-GNOME-Autostart-enabled=true
NoDisplay=true
DESK
    pkill -f "$DIR/devkit_clip.py" >/dev/null 2>&1 || true
    sleep 1
    nohup "$PY" "$DIR/devkit_clip.py" >"$DIR/clip.out" 2>&1 &
  fi

  bold "Done. Copy something here, and it's on your other devices a moment later."
  echo
  echo "The quick picker (your history, search, Enter to paste):"
  if [ "$(uname)" = "Darwin" ]; then
    echo "  give it a key: Shortcuts app > new shortcut > Run Shell Script:"
    echo "    $PY $DIR/devkit_clip.py --pick"
    echo "  then Shortcut Details > Add Keyboard Shortcut (for example Ctrl+Option+V)."
    echo "  If macOS asks to let Python find devices on your local network, click Allow."
  else
    echo "  give it a key in your desktop's keyboard settings (custom shortcut) running:"
    echo "    $PY $DIR/devkit_clip.py --pick"
  fi
  echo
  echo "Nothing else to do. (Only if you ever want to take it off this computer, run:"
  echo "  curl -fsSL http://$GOT/u | sh )"
}

main "$@"
