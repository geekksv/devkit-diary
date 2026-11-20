# devkit-diary, chapter 9 — puts the pet on this computer (Windows).
#
#   irm http://pet.local/i.ps1 | iex
#
# Run once in PowerShell. It makes a private Python environment in %LOCALAPPDATA%\devkit-pet,
# installs the window toolkit (PyQt6) there, fetches the helper from the board, and adds it to
# Startup so it runs by itself at login. If Python isn't installed, it installs it for you
# (for this user only) with winget. To remove it all:  irm http://pet.local/u.ps1 | iex
#
# The board fills in its own addresses below when it serves this file.

& {
  $ErrorActionPreference = 'Stop'
  $ProgressPreference = 'SilentlyContinue'
  $Hosts = '%%HOSTS%%'.Split(' ')
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-pet'
  Write-Host 'Putting the pet on this computer' -ForegroundColor Cyan
  New-Item -ItemType Directory -Force -Path $Dir | Out-Null

  function Find-Python {
    $code = "import sys; print(sys.executable if sys.version_info >= (3, 9) else '')"
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

  $venvPy = Join-Path $Dir 'venv\Scripts\python.exe'
  if (-not (Test-Path $venvPy)) {
    Write-Host '  making a private Python environment'
    & $py -m venv (Join-Path $Dir 'venv')
  }
  Write-Host '  installing the window toolkit (one-off download, a minute or two)'
  & $venvPy -m pip install -q --disable-pip-version-check --upgrade pip 2>$null | Out-Null
  & $venvPy -m pip install -q --disable-pip-version-check 'PyQt6>=6.4'
  if ($LASTEXITCODE -ne 0) { throw 'Installing PyQt6 failed.' }

  Write-Host '  fetching the helper from the board'
  $got = $null
  foreach ($h in $Hosts) {
    try { Invoke-WebRequest "http://$h/devkit_pet.py" -OutFile (Join-Path $Dir 'devkit_pet.py') -UseBasicParsing -TimeoutSec 8; $got = $h; break } catch {}
  }
  if (-not $got) { throw "Couldn't reach the board at: $($Hosts -join ', ') - is this computer on the same WiFi?" }
  Set-Content -Path (Join-Path $Dir 'board') -Value ($Hosts -join ' ')

  # start at login: a shortcut in this user's Startup folder, run without a console window
  $pyw = Join-Path $Dir 'venv\Scripts\pythonw.exe'
  $helper = Join-Path $Dir 'devkit_pet.py'
  $lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit pet.lnk'
  $s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
  $s.TargetPath = $pyw; $s.Arguments = "`"$helper`""; $s.WorkingDirectory = $Dir; $s.Save()

  Get-CimInstance Win32_Process -Filter "Name='pythonw.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_pet.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Start-Sleep -Seconds 1
  Start-Process -FilePath $pyw -ArgumentList "`"$helper`"" -WorkingDirectory $Dir

  Write-Host 'Done. The pet can visit this computer now - and the helper starts by itself from now on.' -ForegroundColor Green
  Write-Host "If Windows asks whether Python may use your network, allow it on private networks."
}
