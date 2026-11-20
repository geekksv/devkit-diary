# devkit-diary, chapter 9 — takes the pet helper off this computer (Windows).
#   irm http://pet.local/u.ps1 | iex
& {
  $Dir = Join-Path $env:LOCALAPPDATA 'devkit-pet'
  Remove-Item (Join-Path ([Environment]::GetFolderPath('Startup')) 'devkit pet.lnk') -ErrorAction SilentlyContinue
  Get-CimInstance Win32_Process -Filter "Name='pythonw.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*devkit_pet.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Start-Sleep -Seconds 1
  Remove-Item $Dir -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host 'The pet helper is gone from this computer. (The pet itself still lives on the board.)'
}
