$packageName = 'zerotier-one'
$installerType = 'msi'
$url = 'https://download.zerotier.com/RELEASES/1.1.14/dist/ZeroTier%20One.msi'
$url64 = 'https://download.zerotier.com/RELEASES/1.1.14/dist/ZeroTier%20One.msi'
$silentArgs = '/quiet'
$validExitCodes = @(0,3010)

Install-ChocolateyPackage $packageName $installerType $silentArgs $url $url64  -validExitCodes $validExitCodes
