# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Function returns 4-part 'classic' version string from SemVer 2.0 string
function GenVer4Part([String] $Version)
{
  if ($Version[0] -eq 'v')
  {
    # If tag contains 'v' then strip it
    $Version = $Version.substring(1)
  }
  # Converts from 1.2.3-build4 to 1.2.3.4
  $Version = $Version -replace "-build", "."
  $Version = $Version -replace "-", "."

  $parts = $Version.Split('.');
  # Add missing tuples
  $i = $parts.Count
  while($i -lt 4)
  {
    $parts += "0"
    $i++
  }

  ,$parts
}

function New-TemporaryDirectory
{
  $parent = [System.IO.Path]::GetTempPath()
  [string] $name = [System.Guid]::NewGuid()
  New-Item -ItemType Directory -Path (Join-Path $parent $name)
}

function GetGitWorkTree()
{
  # TODO: presently we assume that GIT_WORK_TREE is 1-level up.
  # Uncomment the following line if this is not the case:
  # $result = (git rev-parse --show-toplevel) -join ''
  $result = ( Get-Item -Path .. ).Fullname
  $result = $result -replace '[\\/]', '\'
  return $result
}

function CopyAll([String] $src, [String] $dest)
{
  $what = @("/COPYALL","/B","/SEC","/MIR")
  $options = @("/R:0","/W:0","/NFL","/NDL","/NJH","/NJS","/NC","/NS")
  $cmdArgs = @("$src","$dest",$what,$options)
  robocopy @cmdArgs
}

$tempDir = New-TemporaryDirectory
$gitWorktree = GetGitWorkTree

$items = Get-ChildItem .\nuget -include *.nuspec -Recurse
foreach ($item in $items)
{
  $nugetName = $item.Basename
  $v = GenVer4Part $env:PackageVersion
  $version = [string]::Join(".", $v, 0, 3)

  Write-Output "Creating nuget $nugetName-$version ..."
  # Copy all files from Git
  CopyAll $gitWorkTree $tempDir.Fullname
  # Append extra nuget package file
  Copy-Item -Path ".\nuget\$nugetName.nuspec" -Destination $tempDir.Fullname
  Copy-Item -Path ".\nuget\opentelemetry-icon-color.png" -Destination $tempDir.Fullname
  # Change to temporary directory
  Push-Location -Path $tempDir.Fullname
  # Pack the nuget package
  nuget pack $nugetName.nuspec -Version $version -NoDefaultExcludes
  Pop-Location
  $nupkgFileName = "$tempDir\$nugetName.$version.nupkg"
  Get-Item $nupkgFileName | Copy-Item -Destination "..\packages"
}
