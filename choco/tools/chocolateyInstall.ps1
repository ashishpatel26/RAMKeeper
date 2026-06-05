$ErrorActionPreference = 'Stop'

$packageName = 'ramkeeper'
$toolsDir    = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$installDir  = "$(Get-ToolsLocation)\RAMKeeper"

$packageArgs = @{
  packageName    = $packageName
  fileType       = 'exe'
  url            = 'https://github.com/ashishpatel26/RAMKeeper/releases/download/v1.0.0/RAMKeeper.exe'

  # TODO: Replace F5C2DD91C0EC523267BCFBDEC9141069BCC97BE5CF13739EDAFA091A6FD5A586 with the actual SHA-256 hash of RAMKeeper.exe before submitting.
  # Run: (Get-FileHash -Algorithm SHA256 "RAMKeeper.exe").Hash
  checksum       = 'F5C2DD91C0EC523267BCFBDEC9141069BCC97BE5CF13739EDAFA091A6FD5A586'
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
