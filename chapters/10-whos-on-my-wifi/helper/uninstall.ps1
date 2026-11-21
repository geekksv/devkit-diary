# devkit-diary, chapter 10 — takes the inbox helper off this computer (Windows).
#   irm http://scan.local/u.ps1 | iex
& {
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-inbox'
  Remove-Item (Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit inbox.lnk') -ErrorAction SilentlyContinue
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_inbox.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Start-Sleep -Seconds 1
  Remove-Item $Dir -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host 'The inbox helper is gone from this computer. (The scanner on the board is untouched.)'
}
