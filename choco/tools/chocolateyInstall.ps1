$ErrorActionPreference = 'Stop'

$packageName = 'ramkeeper'
$installDir  = "$(Get-ToolsLocation)\RAMKeeper"

$packageArgs = @{
  packageName    = $packageName
  fileType       = 'exe'
  url            = 'https://github.com/ashishpatel26/RAMKeeper/releases/download/v1.1.0/RAMKeeper.exe'

  # SHA-256 computed from CI-built release asset
  # (Get-FileHash -Algorithm SHA256 "RAMKeeper.exe").Hash
  checksum       = '1c410ba742c4b109031a31bf303a26a8f93123f37e1ab3b2546f2807a30ab680'
  checksumType   = 'sha256'

  # Destination path for the portable exe
  destination    = "$installDir\RAMKeeper.exe"
}

# Ensure install directory exists
if (-not (Test-Path $installDir)) {
  New-Item -ItemType Directory -Path $installDir | Out-Null
}

# Download the exe to the install directory
Get-ChocolateyWebFile `
  -PackageName  $packageArgs.packageName `
  -FileFullPath $packageArgs.destination `
  -Url          $packageArgs.url `
  -Checksum     $packageArgs.checksum `
  -ChecksumType $packageArgs.checksumType

Write-Host "RAMKeeper installed to: $installDir"

# Create a shim so 'RAMKeeper' is available on PATH from any terminal.
# Chocolatey automatically shims every .exe placed in the tools dir;
# because we installed to a sub-directory we install the shim explicitly.
Install-BinFile -Name 'RAMKeeper' -Path "$installDir\RAMKeeper.exe"

Write-Host ""
Write-Host "RAMKeeper requires administrator privileges to purge standby memory."
Write-Host "Launch it elevated or run:  Start-Process RAMKeeper -Verb RunAs"
