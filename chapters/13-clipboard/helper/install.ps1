# devkit-diary, chapter 13: puts this computer on your shared clipboard (Windows).
#
#   irm http://clip.local/i.ps1 | iex
#
# Run once in PowerShell. It fetches the clipboard helper from the board into
# %LOCALAPPDATA%\devkit-clip, asks this computer's name and your passphrase in a small window,
# and adds the helper to Startup so it runs by itself at login. It needs only Python's standard
# library; if Python isn't installed, it installs it for this user with winget.
#
# The board fills in its own addresses below when it serves this file.

& {
  $ErrorActionPreference = 'Stop'
  $ProgressPreference = 'SilentlyContinue'
  $Hosts = '%%HOSTS%%'.Split(' ')
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-clip'
  Write-Host 'Putting this computer on your shared clipboard' -ForegroundColor Cyan
  New-Item -ItemType Directory -Force -Path $Dir | Out-Null

  function Find-Python {
    $code = "import sys; print(sys.executable if sys.version_info >= (3, 8) else '')"
    foreach ($c in 'py', 'python') {
      try {
        if ($c -eq 'py') { $exe = & py -3 -c $code 2>$null } else { $exe = & python -c $code 2>$null }
        if ($LASTEXITCODE -eq 0 -and $exe -and (Test-Path "$exe".Trim())) { return "$exe".Trim() }
      } catch {}
    }
    $guess = Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
             Sort-Object FullName -Descending | Select-Object -First 1
    if ($guess) { return $guess.FullName }
    return $null
  }

  $py = Find-Python
  if (-not $py) {
    Write-Host '  installing Python (one-off, for this user only)'
    winget install -e --id Python.Python.3.12 --scope user --silent --accept-package-agreements --accept-source-agreements | Out-Null
    $py = Find-Python
    if (-not $py) { throw 'Python could not be installed automatically. Install it from python.org, then run this again.' }
  }
  $pyw = Join-Path (Split-Path $py) 'pythonw.exe'
  if (-not (Test-Path $pyw)) { $pyw = $py }

  Write-Host '  fetching the helper from the board'
  $got = $null
  foreach ($h in $Hosts) {
    try {
      # The board keeps these gzipped, to fit in its flash.
      foreach ($f in 'devkit_clip.py', 'clipcrypto.py') {
        $gz = Join-Path $Dir "$f.gz"
        Invoke-WebRequest "http://$h/$f.gz" -OutFile $gz -UseBasicParsing -TimeoutSec 8
        $in = [IO.File]::OpenRead($gz); $out = [IO.File]::Create((Join-Path $Dir $f))
        $z = New-Object IO.Compression.GZipStream($in, [IO.Compression.CompressionMode]::Decompress)
        $z.CopyTo($out); $z.Close(); $out.Close(); $in.Close(); Remove-Item $gz
      }
      $got = $h; break
    } catch {}
  }
  if (-not $got) { throw "Couldn't reach the board at: $($Hosts -join ', '). Is this computer on the same WiFi?" }
  Set-Content -Path (Join-Path $Dir 'board') -Value ($Hosts -join ' ')
  $helper = Join-Path $Dir 'devkit_clip.py'

  if (-not (Test-Path (Join-Path $Dir 'config.json'))) {
    Write-Host '  joining your clipboard group (a small window opens)'
    & $py $helper --setup
    if ($LASTEXITCODE -ne 0) { throw "Setup didn't finish. Run this command again to try again." }
  }

  # start at login: a shortcut in this user's Startup folder, run without a console window
  $lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit clipboard.lnk'
  $s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
  $s.TargetPath = $pyw; $s.Arguments = "`"$helper`""; $s.WorkingDirectory = $Dir; $s.Save()

  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_clip.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Start-Sleep -Seconds 1
  Start-Process -FilePath $pyw -ArgumentList "`"$helper`"" -WorkingDirectory $Dir
  Start-Sleep -Seconds 3

  $key = 'Win+Alt+V'
  try { $cfg = Get-Content (Join-Path $Dir 'config.json') -Raw | ConvertFrom-Json; if ($cfg.hotkey) { $key = $cfg.hotkey } } catch {}
  Write-Host "Done. Copy something here, and it's on your other devices a moment later." -ForegroundColor Green
  Write-Host "Press $key for the quick picker: your history, search, Enter to paste."
  Write-Host ''
  Write-Host 'Nothing else to do. (Only if you ever want to take it off this computer, run:'
  Write-Host "  irm http://$got/u.ps1 | iex )"
}
