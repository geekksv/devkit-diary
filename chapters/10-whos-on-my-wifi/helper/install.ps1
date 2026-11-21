# devkit-diary, chapter 10 — lets the WiFi scanner send this computer messages (Windows).
#
#   irm http://scan.local/i.ps1 | iex
#
# Run once in PowerShell. It fetches the small inbox helper from the board into
# %LOCALAPPDATA%\devkit-inbox and adds it to Startup so it runs by itself at login. It needs only
# Python's standard library; if Python isn't installed, it installs it for this user with winget.
#
# The board fills in its own addresses below when it serves this file.

& {
  $ErrorActionPreference = 'Stop'
  $ProgressPreference = 'SilentlyContinue'
  $Hosts = '%%HOSTS%%'.Split(' ')
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-inbox'
  Write-Host 'Letting the scanner send messages to this computer' -ForegroundColor Cyan
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

  Write-Host '  fetching the inbox helper from the board'
  $got = $null
  foreach ($h in $Hosts) {
    try { Invoke-WebRequest "http://$h/devkit_inbox.py" -OutFile (Join-Path $Dir 'devkit_inbox.py') -UseBasicParsing -TimeoutSec 8; $got = $h; break } catch {}
  }
  if (-not $got) { throw "Couldn't reach the board at: $($Hosts -join ', ') - is this computer on the same WiFi?" }
  Set-Content -Path (Join-Path $Dir 'board') -Value ($Hosts -join ' ')

  # start at login: a shortcut in this user's Startup folder, run without a console window
  $helper = Join-Path $Dir 'devkit_inbox.py'
  $lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit inbox.lnk'
  $s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
  $s.TargetPath = $pyw; $s.Arguments = "`"$helper`""; $s.WorkingDirectory = $Dir; $s.Save()

  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_inbox.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Start-Sleep -Seconds 1
  Start-Process -FilePath $pyw -ArgumentList "`"$helper`"" -WorkingDirectory $Dir

  Write-Host 'Done. This computer now shows a Message button in the scanner''s list.' -ForegroundColor Green
  Write-Host ""
  Write-Host "Nothing else to do. (Only if you ever want to remove it from this computer, run:" -ForegroundColor DarkGray
  Write-Host "  irm http://$got/u.ps1 | iex )" -ForegroundColor DarkGray
}
