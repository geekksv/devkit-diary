#!/bin/sh
# devkit-diary, chapter 10 — lets the WiFi scanner send this computer messages (macOS or Linux).
#
#   curl -fsSL http://scan.local/i | sh
#
# Run once. It fetches the small inbox helper from the board into ~/.devkit-inbox and sets it to
# start by itself at login. It uses the Python already on this computer, standard library only:
# nothing is downloaded from the internet and nothing is installed system-wide.
#
# The board fills in its own addresses below when it serves this file.

main() {
  set -e
  HOSTS="%%HOSTS%%"
  DIR="$HOME/.devkit-inbox"
  bold() { printf '\033[1m%s\033[0m\n' "$1"; }

  bold "Letting the scanner send messages to this computer"
  mkdir -p "$DIR"

  if ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
    if [ "$(uname)" = "Darwin" ]; then
      echo "Python 3 isn't set up yet. macOS will offer to install the developer tools —"
      echo "click Install, wait for it to finish, then run this same command again."
      xcode-select --install >/dev/null 2>&1 || true
    else
      echo "Please install Python 3.8 or newer (for example: sudo apt install python3), then run this again."
    fi
    exit 1
  fi
  PY="$(command -v python3)"

  echo "  fetching the inbox helper from the board"
  GOT=""
  for H in $HOSTS; do
    if curl -fsS --max-time 8 "http://$H/devkit_inbox.py" -o "$DIR/devkit_inbox.py.new" 2>/dev/null; then GOT="$H"; break; fi
  done
  [ -n "$GOT" ] || { echo "Couldn't reach the board at: $HOSTS — is this computer on the same WiFi?"; exit 1; }
  mv "$DIR/devkit_inbox.py.new" "$DIR/devkit_inbox.py"
  echo "$HOSTS" > "$DIR/board"

  if [ "$(uname)" = "Darwin" ]; then
    PL="$HOME/Library/LaunchAgents/diary.devkit.inbox.plist"
    mkdir -p "$HOME/Library/LaunchAgents"
    cat > "$PL" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>diary.devkit.inbox</string>
  <key>ProgramArguments</key><array><string>$PY</string><string>$DIR/devkit_inbox.py</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><dict><key>SuccessfulExit</key><false/></dict>
  <key>ProcessType</key><string>Interactive</string>
  <key>StandardOutPath</key><string>$DIR/inbox.log</string>
  <key>StandardErrorPath</key><string>$DIR/inbox.log</string>
</dict></plist>
PLIST
    launchctl bootout "gui/$(id -u)" "$PL" >/dev/null 2>&1 || true
    sleep 1
    launchctl bootstrap "gui/$(id -u)" "$PL"
  else
    mkdir -p "$HOME/.config/autostart"
    cat > "$HOME/.config/autostart/devkit-inbox.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=devkit inbox
Exec=$PY $DIR/devkit_inbox.py
X-GNOME-Autostart-enabled=true
NoDisplay=true
DESK
    pkill -f "$DIR/devkit_inbox.py" >/dev/null 2>&1 || true
    sleep 1
    nohup "$PY" "$DIR/devkit_inbox.py" >"$DIR/inbox.log" 2>&1 &
  fi

  bold "Done. This computer now shows a 📨 Message button in the scanner's list."
  if [ "$(uname)" = "Darwin" ]; then
    echo "If macOS asks to let Python find devices on your local network, click Allow."
  else
    echo "(Messages show with zenity or kdialog if you have one; otherwise in a small Python window.)"
  fi
  echo
  echo "Nothing else to do. (Only if you ever want to remove it from this computer, run:"
  echo "  curl -fsSL http://$GOT/u | sh )"
}

main "$@"
