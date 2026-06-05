$ErrorActionPreference = 'Stop'

$packageName = 'ramkeeper'
$installDir  = "$(Get-ToolsLocation)\RAMKeeper"

# Remove the shim created during install
Uninstall-BinFile -Name 'RAMKeeper'

# Remove the install directory and its contents
if (Test-Path $installDir) {
  Remove-Item -Recurse -Force $installDir
  Write-Host "Removed: $installDir"
} else {
  Write-Warning "Install directory not found (already removed?): $installDir"
}

Write-Host "$packageName has been uninstalled."
