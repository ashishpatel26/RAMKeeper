$ErrorActionPreference = 'Stop'

$packageName = 'ramkeeper'
$toolsDir    = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$installDir  = "$(Get-ToolsLocation)\RAMKeeper"

$packageArgs = @{
  packageName    = $packageName
  fileType       = 'exe'
  url            = 'https://github.com/ashishpatel26/RAMKeeper/releases/download/v1.0.0/RAMKeeper.exe'

  # TODO: Replace 1DDA51E9A7A29BF7C1F4D41619B8CB3F7F173A96EC1FB236A4019839E33791A7 with the actual SHA-256 hash of RAMKeeper.exe before submitting.
  # Run: (Get-FileHash -Algorithm SHA256 "RAMKeeper.exe").Hash
  checksum       = '1DDA51E9A7A29BF7C1F4D41619B8CB3F7F173A96EC1FB236A4019839E33791A7'
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
