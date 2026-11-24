# devkit-diary, chapter 13: takes the clipboard helper off this computer (Windows).
#
#   irm http://clip.local/u.ps1 | iex
#
# It tells the board this computer is leaving the group, stops the helper, and deletes
# %LOCALAPPDATA%\devkit-clip (the helper and its keys) and its Startup shortcut.

& {
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-clip'
  $helper = Join-Path $Dir 'devkit_clip.py'
  if (Test-Path $helper) {
    foreach ($c in 'py', 'python') { try { if ($c -eq 'py') { & py -3 $helper --leave | Out-Null } else { & python $helper --leave | Out-Null }; break } catch {} }
  }
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_clip.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Remove-Item (Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit clipboard.lnk') -ErrorAction SilentlyContinue
  Remove-Item $Dir -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host 'The clipboard helper is gone from this computer, with its keys.' -ForegroundColor Green
}
