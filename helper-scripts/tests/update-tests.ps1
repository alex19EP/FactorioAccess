# Offline updater tests. All game folders, archives and downloads are isolated fixtures.
#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$updater = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\factorio-access-update.ps1'))
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$testRoot = Join-Path $tempRoot ('FactorioAccess-update-tests-' + [Guid]::NewGuid().ToString('N'))
$originalAppData = $env:APPDATA
$failures = 0
$cases = 0
$fixtureState = @{}

function Write-Fixture([string]$path, [string]$text) {
   [IO.Directory]::CreateDirectory((Split-Path $path)) | Out-Null
   [IO.File]::WriteAllText($path, $text)
}

function Write-Zip([string]$path, [hashtable]$entries) {
   if (Test-Path -LiteralPath $path) { [IO.File]::Delete($path) }
   $zip = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
   try {
      foreach ($key in $entries.Keys) {
         $writer = [IO.StreamWriter]::new($zip.CreateEntry($key).Open())
         try { $writer.Write($entries[$key]) } finally { $writer.Dispose() }
      }
   } finally { $zip.Dispose() }
}

function Assert-Equal($actual, $expected) {
   if ($actual -ne $expected) { throw "Expected '$expected', got '$actual'." }
}

function Assert-Fails([scriptblock]$action, [string]$pattern) {
   $caught = $false
   try { & $action } catch {
      $caught = $true
      if ($_.Exception.Message -notmatch $pattern) { throw }
   }
   if (-not $caught) { throw "Expected failure matching $pattern." }
}

function Assert-OldFiles {
   Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'bin\x64\winmm.dll'))) 'old stub'
   Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'bin\x64\fa_native.dll'))) 'old core'
   Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'FactorioAccess_0.16.0.zip'))) 'old mod'
   Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip')) $false
}

# These commands shadow network, registry and process access only inside the test script.
function Invoke-RestMethod($Uri, $Headers) {
   Assert-Equal $Uri 'https://api.github.com/repos/alex19EP/FactorioAccess/releases/tags/dev'
   $fixtureState.apiCalls++
   return $fixtureState.release
}

function Invoke-WebRequest($Uri, $Headers, $OutFile, [switch]$UseBasicParsing) {
   $fixtureState.downloads++
   if ($fixtureState.failDownload -and $fixtureState.downloads -eq 2) { throw 'Simulated download failure' }
   [IO.File]::Copy((Join-Path $fixtureState.fixture $Uri), $OutFile)
}

function Get-Process($Name, $ErrorAction) {
   if ($fixtureState.running -or ($fixtureState.lateRunning -and $fixtureState.downloads -eq 2)) { return @{ Id = 123 } }
}

function Get-ItemPropertyValue($Path, $Name, $ErrorAction) { return $fixtureState.steam }

function Copy-Item($LiteralPath, $Destination, [switch]$Force) {
   if ($fixtureState.failCopy -and $Destination -eq (Join-Path $fixtureState.game 'bin\x64\fa_native.dll')) {
      $fixtureState.failCopy = $false
      throw 'Simulated copy failure'
   }
   Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force
}

function Remove-Item($LiteralPath, [switch]$Recurse, [switch]$Force) {
   if ($fixtureState.failRemove -and $LiteralPath -eq (Join-Path $fixtureState.mods 'FactorioAccess_0.16.1.zip')) {
      $fixtureState.failRemove = $false
      throw 'Simulated removal failure'
   }
   Microsoft.PowerShell.Management\Remove-Item -LiteralPath $LiteralPath -Recurse:$Recurse -Force:$Force
}

function Run-Case([string]$name, [scriptblock]$action) {
   $script:cases++
   $fixtureState.fixture = Join-Path $testRoot "case-$script:cases"
   $fixtureState.game = Join-Path $fixtureState.fixture 'Game folder'
   $fixtureState.mods = Join-Path $fixtureState.game 'mods'
   $fixtureState.steam = Join-Path $fixtureState.fixture 'Steam'
   $env:APPDATA = Join-Path $fixtureState.fixture 'Roaming'
   $fixtureState.apiCalls = 0
   $fixtureState.downloads = 0
   $fixtureState.running = $false
   $fixtureState.lateRunning = $false
   $fixtureState.failDownload = $false
   $fixtureState.failCopy = $false
   $fixtureState.failRemove = $false
   Write-Fixture (Join-Path $fixtureState.game 'bin\x64\factorio.exe') 'fixture, never executed'
   Write-Fixture (Join-Path $fixtureState.game 'bin\x64\winmm.dll') 'old stub'
   Write-Fixture (Join-Path $fixtureState.game 'bin\x64\fa_native.dll') 'old core'
   Write-Fixture (Join-Path $fixtureState.game 'config-path.cfg') "config-path=__PATH__executable__/../../config`nuse-system-read-write-data-directories=false"
   Write-Fixture (Join-Path $fixtureState.game 'config\config.ini') "[path]`nwrite-data=__PATH__executable__/../..`n"
   Write-Fixture (Join-Path $fixtureState.mods 'FactorioAccess_0.16.0.zip') 'old mod'
   $fixtureState.nativeEntries = @{
      'bin/x64/winmm.dll' = 'new stub'; 'bin/x64/fa_native.dll' = 'new core'
      'bin/x64/winmm.pdb' = 'stub symbols'; 'bin/x64/fa_native.pdb' = 'core symbols'
      'factorio-access-keys.cmd' = 'keys wrapper'; 'factorio-access-keys.ps1' = 'keys script'
      'factorio-access-update.cmd' = 'update wrapper'; 'factorio-access-update.ps1' = 'update script'
      '../outside.txt' = 'must never be extracted'
   }
   Write-Zip (Join-Path $fixtureState.fixture 'native.zip') $fixtureState.nativeEntries
   Write-Zip (Join-Path $fixtureState.fixture 'mod.zip') @{
      'FactorioAccess_0.17.0/info.json' = '{"name":"FactorioAccess","version":"0.17.0"}'
   }
   $fixtureState.release = [pscustomobject]@{ name = 'Development build fixture'; assets = @(
      [pscustomobject]@{ name = 'FactorioAccess-native.zip'; browser_download_url = 'native.zip' }
      [pscustomobject]@{ name = 'FactorioAccess_0.17.0.zip'; browser_download_url = 'mod.zip' }
   ) }
   try { & $action; Write-Host "PASS $name" } catch {
      $script:failures++
      Write-Host "FAIL $name`: $_"
   }
}

try {
   Run-Case 'CMD wrapper forwards paths with spaces and preserves the exit code' {
      $wrapper = Join-Path $fixtureState.fixture 'factorio-access-update.cmd'
      [IO.File]::Copy((Join-Path (Split-Path $updater) 'factorio-access-update.cmd'), $wrapper)
      $dummy = @'
param([string]$GameDir, [string]$ModsDir, [int]$Result)
[IO.File]::WriteAllLines((Join-Path $PSScriptRoot 'arguments.txt'), @($GameDir, $ModsDir))
exit $Result
'@
      Write-Fixture (Join-Path $fixtureState.fixture 'factorio-access-update.ps1') $dummy
      foreach ($result in @(0, 7)) {
         & $env:ComSpec /d /c "call `"$wrapper`" -GameDir `"$($fixtureState.game)`" -ModsDir `"$($fixtureState.mods)`" -Result $result <nul" | Out-Null
         Assert-Equal $LASTEXITCODE $result
         $arguments = [IO.File]::ReadAllLines((Join-Path $fixtureState.fixture 'arguments.txt'))
         Assert-Equal $arguments[0] $fixtureState.game
         Assert-Equal $arguments[1] $fixtureState.mods
      }
   }
   Run-Case 'Portable update preserves saves, settings and other mods' {
      Write-Fixture (Join-Path $fixtureState.mods 'OtherMod_1.0.0.zip') 'other mod'
      Write-Fixture (Join-Path $fixtureState.mods 'mod-list.json') '{"mods":[]}'
      Write-Fixture (Join-Path $fixtureState.game 'saves\save.zip') 'save'
      & $updater -GameDir $fixtureState.game
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'bin\x64\fa_native.dll'))) 'new core'
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.16.0.zip')) $false
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip')) $true
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'OtherMod_1.0.0.zip'))) 'other mod'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'mod-list.json'))) '{"mods":[]}'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'saves\save.zip'))) 'save'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'config\config.ini'))) "[path]`nwrite-data=__PATH__executable__/../..`n"
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.game 'outside.txt')) $false
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'factorio-access-update.cmd'))) 'update wrapper'
   }
   Run-Case 'Updater uses its own game folder without arguments' {
      $localUpdater = Join-Path $fixtureState.game 'factorio-access-update.ps1'
      [IO.File]::Copy($updater, $localUpdater)
      & $localUpdater
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Steam detection follows libraryfolders and the app manifest' {
      Write-Fixture (Join-Path $fixtureState.steam 'steamapps\libraryfolders.vdf') ('"path" "' + $fixtureState.fixture.Replace('\', '\\') + '"')
      Write-Fixture (Join-Path $fixtureState.fixture 'steamapps\appmanifest_427520.acf') '"installdir" "Custom Factorio"'
      $fixtureState.game = Join-Path $fixtureState.fixture 'steamapps\common\Custom Factorio'
      Write-Fixture (Join-Path $fixtureState.game 'bin\x64\factorio.exe') 'fixture'
      Write-Fixture (Join-Path $fixtureState.game 'config-path.cfg') "config-path=__PATH__system-write-data__/config`nuse-system-read-write-data-directories=true"
      Write-Fixture (Join-Path $env:APPDATA 'Factorio\config\config.ini') "[path]`n; write-data=wrong`nwrite-data=__PATH__system-write-data__"
      & $updater
      Assert-Equal (Test-Path -LiteralPath (Join-Path $env:APPDATA 'Factorio\mods\FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Custom write-data path is respected' {
      $custom = Join-Path $fixtureState.fixture 'Custom user data'
      Write-Fixture (Join-Path $fixtureState.game 'config\config.ini') "[other]`nwrite-data=wrong`n[path]`nwrite-data=$custom"
      & $updater -GameDir $fixtureState.game
      Assert-Equal (Test-Path -LiteralPath (Join-Path $custom 'mods\FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Portable default works before config.ini exists' {
      [IO.File]::Delete((Join-Path $fixtureState.game 'config\config.ini'))
      & $updater -GameDir $fixtureState.game
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Steam default works before config.ini exists' {
      Write-Fixture (Join-Path $fixtureState.game 'config-path.cfg') "config-path=__PATH__system-write-data__/config`nuse-system-read-write-data-directories=true"
      & $updater -GameDir $fixtureState.game
      Assert-Equal (Test-Path -LiteralPath (Join-Path $env:APPDATA 'Factorio\mods\FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'ModsDir overrides configuration' {
      [IO.File]::Delete((Join-Path $fixtureState.game 'config-path.cfg'))
      $custom = Join-Path $fixtureState.fixture 'Explicit mods'
      & $updater -GameDir $fixtureState.game -ModsDir $custom
      Assert-Equal (Test-Path -LiteralPath (Join-Path $custom 'FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Running game is rejected before downloading' {
      $fixtureState.running = $true
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Factorio is running'
      Assert-Equal $fixtureState.apiCalls 0
      Assert-OldFiles
   }
   Run-Case 'Game started during downloads is rejected before installation' {
      $fixtureState.lateRunning = $true
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Factorio is running'
      Assert-OldFiles
   }
   Run-Case 'Failed second download leaves installed files alone' {
      $fixtureState.failDownload = $true
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Simulated download failure'
      Assert-OldFiles
   }
   Run-Case 'Incomplete native archive leaves installed files alone' {
      $fixtureState.nativeEntries.Remove('bin/x64/fa_native.dll')
      Write-Zip (Join-Path $fixtureState.fixture 'native.zip') $fixtureState.nativeEntries
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'missing bin/x64/fa_native.dll'
      Assert-OldFiles
   }
   Run-Case 'Invalid mod metadata leaves installed files alone' {
      Write-Zip (Join-Path $fixtureState.fixture 'mod.zip') @{
         'FactorioAccess_0.17.0/info.json' = '{"name":"OtherMod","version":"0.17.0"}'
      }
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'do not match'
      Assert-OldFiles
   }
   Run-Case 'Checksum mismatch leaves installed files alone' {
      $fixtureState.release.assets[0] | Add-Member -NotePropertyName digest -NotePropertyValue ('sha256:' + ('0' * 64))
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Checksum failed'
      Assert-OldFiles
   }
   Run-Case 'Correct GitHub checksums are accepted' {
      foreach ($asset in $fixtureState.release.assets) {
         $hash = (Get-FileHash -LiteralPath (Join-Path $fixtureState.fixture $asset.browser_download_url)).Hash.ToLowerInvariant()
         $asset | Add-Member -NotePropertyName digest -NotePropertyValue "sha256:$hash"
      }
      & $updater -GameDir $fixtureState.game
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip')) $true
   }
   Run-Case 'Incomplete release is rejected before downloading' {
      $fixtureState.release.assets = @($fixtureState.release.assets[0])
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'one native archive and one mod archive'
      Assert-Equal $fixtureState.downloads 0
      Assert-OldFiles
   }
   Run-Case 'Unpacked mod is preserved and update refused' {
      Write-Fixture (Join-Path $fixtureState.mods 'FactorioAccess\info.json') 'developer checkout'
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Unpacked FactorioAccess'
      Assert-Equal $fixtureState.apiCalls 0
      Assert-OldFiles
   }
   Run-Case 'Copy failure rolls back native files' {
      $fixtureState.failCopy = $true
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Simulated copy failure'
      Assert-OldFiles
   }
   Run-Case 'Removal failure restores old mod archives and overwritten same-version zip' {
      Write-Fixture (Join-Path $fixtureState.mods 'FactorioAccess_0.16.1.zip') 'second old mod'
      Write-Fixture (Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip') 'same version, previous dev build'
      $fixtureState.failRemove = $true
      Assert-Fails { & $updater -GameDir $fixtureState.game } 'Simulated removal failure'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'FactorioAccess_0.16.0.zip'))) 'old mod'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'FactorioAccess_0.16.1.zip'))) 'second old mod'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.mods 'FactorioAccess_0.17.0.zip'))) 'same version, previous dev build'
      Assert-Equal ([IO.File]::ReadAllText((Join-Path $fixtureState.game 'bin\x64\winmm.dll'))) 'old stub'
      Assert-Equal (Test-Path -LiteralPath (Join-Path $fixtureState.game 'factorio-access-update.cmd')) $false
   }
} finally {
   $env:APPDATA = $originalAppData
   $resolvedRoot = (Resolve-Path -LiteralPath $testRoot).Path
   if ([IO.Path]::GetDirectoryName($resolvedRoot) -eq $tempRoot.TrimEnd('\', '/')) {
      Microsoft.PowerShell.Management\Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
   }
}
Write-Host "$($cases - $failures)/$cases updater tests passed."
if ($failures) { exit 1 }
# Explicitly, or the exit code a test left in $LASTEXITCODE (the wrapper's 7) fails a CI step
exit 0
