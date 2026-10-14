<#
.SYNOPSIS
Installs the latest FactorioAccess dev build, both the native DLLs and the Lua mod.
.DESCRIPTION
Close Factorio first. Run from the game folder, or let the script find Steam's installation.
Downloads both archives from alex19EP/FactorioAccess's dev release before replacing any files.
Existing saves, settings and other mods are preserved. Requires Windows PowerShell 5.1 or newer.
.PARAMETER GameDir
Factorio's installation folder, containing bin\x64\factorio.exe. Defaults to the script's folder
when installed there, otherwise the Steam installation (including other Steam libraries).
.PARAMETER ModsDir
Overrides the mods folder found through config-path.cfg and config.ini's path.write-data.
Use this if you launch Factorio with --mod-directory or a different --config.
.EXAMPLE
factorio-access-update.cmd
.EXAMPLE
factorio-access-update.cmd -GameDir "D:\Games\Factorio"
#>
#requires -Version 5.1
[CmdletBinding()]
param(
   [string]$GameDir,
   [string]$ModsDir
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Find-SteamGame {
   $steam = Get-ItemPropertyValue -Path 'HKCU:\Software\Valve\Steam' -Name SteamPath -ErrorAction SilentlyContinue
   if (-not $steam) { $steam = Join-Path ${env:ProgramFiles(x86)} 'Steam' }
   $libraries = @($steam)
   $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
   if (Test-Path -LiteralPath $vdf) {
      foreach ($match in [regex]::Matches([IO.File]::ReadAllText($vdf), '"path"\s+"([^"]+)"')) {
         $libraries += $match.Groups[1].Value.Replace('\\', '\')
      }
   }
   foreach ($library in ($libraries | Select-Object -Unique)) {
      $manifest = Join-Path $library 'steamapps\appmanifest_427520.acf'
      if (-not (Test-Path -LiteralPath $manifest)) { continue }
      if ([IO.File]::ReadAllText($manifest) -match '"installdir"\s+"([^"]+)"') {
         $candidate = Join-Path $library "steamapps\common\$($Matches[1])"
         if (Test-Path -LiteralPath (Join-Path $candidate 'bin\x64\factorio.exe')) { return $candidate }
      }
   }
   throw 'Cannot find Factorio. Pass -GameDir with the folder containing bin\x64\factorio.exe.'
}

function Read-IniValue([string]$path, [string]$section, [string]$key) {
   $current = ''
   foreach ($line in [IO.File]::ReadAllLines($path)) {
      if ($line -match '^\s*\[([^]]+)\]\s*$') { $current = $Matches[1] }
      elseif ($current -eq $section -and $line -match ('^\s*' + [regex]::Escape($key) + '\s*=\s*(.*?)\s*$')) {
         return $Matches[1]
      }
   }
}

function Resolve-GamePath([string]$path) {
   $path = $path.Replace('__PATH__executable__', (Join-Path $GameDir 'bin\x64'))
   $path = $path.Replace('__PATH__system-write-data__', (Join-Path $env:APPDATA 'Factorio'))
   if (-not [IO.Path]::IsPathRooted($path)) { $path = Join-Path $GameDir $path }
   return [IO.Path]::GetFullPath($path)
}

function Assert-GameClosed {
   if (Get-Process -Name factorio -ErrorAction SilentlyContinue) {
      throw 'Factorio is running. Close it, then run the updater again.'
   }
}

Assert-GameClosed
if (-not $GameDir) {
   $GameDir = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'bin\x64\factorio.exe')) {
      $PSScriptRoot
   } else { Find-SteamGame }
}
$GameDir = [IO.Path]::GetFullPath($GameDir)
if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'bin\x64\factorio.exe'))) {
   throw "No bin\x64\factorio.exe in $GameDir. Pass -GameDir with the game folder."
}
if (-not $ModsDir) {
   $cfg = Join-Path $GameDir 'config-path.cfg'
   $configDir = Read-IniValue $cfg '' 'config-path'
   if (-not $configDir) { throw "No config-path in $cfg. Pass -ModsDir with your mods folder." }
   $config = Join-Path (Resolve-GamePath $configDir) 'config.ini'
   $writeData = if (Test-Path -LiteralPath $config) { Read-IniValue $config 'path' 'write-data' }
   if (-not $writeData) {
      $writeData = if ((Read-IniValue $cfg '' 'use-system-read-write-data-directories') -eq 'true') {
         '__PATH__system-write-data__'
      } else { $GameDir }
   }
   $ModsDir = Join-Path (Resolve-GamePath $writeData) 'mods'
}
$ModsDir = [IO.Path]::GetFullPath($ModsDir)
# An unpacked mod, including a developer's junction, takes precedence over an installed zip.
$unpacked = @(Get-ChildItem -LiteralPath $ModsDir -Directory -ErrorAction SilentlyContinue | Where-Object {
   $_.Name -eq 'FactorioAccess' -or $_.Name -match '^FactorioAccess_\d+\.\d+\.\d+$'
})
if ($unpacked.Count) { throw "Unpacked FactorioAccess in $ModsDir. Move it out of mods before using the updater." }

Write-Host "Game folder: $GameDir"
Write-Host "Mods folder: $ModsDir"
Write-Host 'Checking the latest development release...'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$headers = @{ 'User-Agent' = 'FactorioAccess-Updater'; Accept = 'application/vnd.github+json' }
$release = Invoke-RestMethod -Uri 'https://api.github.com/repos/alex19EP/FactorioAccess/releases/tags/dev' -Headers $headers
$native = @($release.assets | Where-Object { $_.name -eq 'FactorioAccess-native.zip' })
$mod = @($release.assets | Where-Object { $_.name -match '^FactorioAccess_\d+\.\d+\.\d+\.zip$' })
if ($native.Count -ne 1 -or $mod.Count -ne 1) { throw 'The dev release must contain one native archive and one mod archive. Try again after the build finishes.' }

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$stage = Join-Path $tempRoot ('FactorioAccess-update-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$backups = @()
$installing = $false
$keepBackups = $false
try {
   foreach ($asset in @($native[0], $mod[0])) {
      Write-Host "Downloading $($asset.name)..."
      $download = Join-Path $stage $asset.name
      Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -Headers $headers -OutFile $download
      $digest = $asset.PSObject.Properties['digest']
      if ($digest -and $digest.Value -match '^sha256:(.+)$') {
         if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $Matches[1]) {
            throw "Checksum failed for $($asset.name). Nothing was installed."
         }
      }
   }
   Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
   $modZip = [IO.Compression.ZipFile]::OpenRead((Join-Path $stage $mod[0].name))
   try {
      $info = $modZip.GetEntry(($mod[0].name -replace '\.zip$', '') + '/info.json')
      if (-not $info) { throw 'The mod archive has no info.json.' }
      $reader = [IO.StreamReader]::new($info.Open())
      try { $metadata = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
      if ($metadata.name -ne 'FactorioAccess' -or "FactorioAccess_$($metadata.version).zip" -ne $mod[0].name) {
         throw 'The mod archive name and info.json do not match.'
      }
   } finally { $modZip.Dispose() }

   # Extract only our known files, never arbitrary paths supplied by an archive.
   $files = @('bin/x64/winmm.dll', 'bin/x64/fa_native.dll', 'bin/x64/winmm.pdb', 'bin/x64/fa_native.pdb',
      'factorio-access-keys.cmd', 'factorio-access-keys.ps1', 'factorio-access-update.cmd', 'factorio-access-update.ps1')
   $copies = @()
   $nativeZip = [IO.Compression.ZipFile]::OpenRead((Join-Path $stage $native[0].name))
   try {
      foreach ($file in $files) {
         $entry = $nativeZip.GetEntry($file)
         if (-not $entry) {
            if ($file.EndsWith('.dll')) { throw "The native archive is missing $file." }
            continue
         }
         $source = Join-Path $stage ('unpacked\' + $file)
         New-Item -ItemType Directory -Force -Path (Split-Path $source) | Out-Null
         [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $source)
         $copies += @{ Source = $source; Target = Join-Path $GameDir $file }
      }
   } finally { $nativeZip.Dispose() }
   $modTarget = Join-Path $ModsDir $mod[0].name
   $copies += @{ Source = Join-Path $stage $mod[0].name; Target = $modTarget }
   $oldMods = @(Get-ChildItem -LiteralPath $ModsDir -File -ErrorAction SilentlyContinue | Where-Object {
      $_.Name -match '^FactorioAccess_\d+\.\d+\.\d+\.zip$' -and $_.FullName -ne $modTarget
   })
   foreach ($target in @($copies.Target) + @($oldMods | ForEach-Object { $_.FullName })) {
      $exists = Test-Path -LiteralPath $target
      $backup = Join-Path $stage ("backup-$($backups.Count)")
      if ($exists) { Copy-Item -LiteralPath $target -Destination $backup }
      $backups += @{ Target = $target; Exists = $exists; Backup = $backup }
   }
   Assert-GameClosed
   $installing = $true
   foreach ($copy in $copies) {
      New-Item -ItemType Directory -Force -Path (Split-Path $copy.Target) | Out-Null
      Copy-Item -LiteralPath $copy.Source -Destination $copy.Target -Force
   }
   foreach ($old in $oldMods) { Remove-Item -LiteralPath $old.FullName -Force }
   $installing = $false
   Write-Host "Installed $($release.name), $($mod[0].name)."
   Write-Host 'Start Factorio normally, exit from the main menu, then run factorio-access-keys.cmd to bind any new controls.'
} catch {
   if ($installing) {
      Write-Warning 'Installation failed. Restoring the previous files...'
      foreach ($backup in $backups) {
         try {
            if ($backup.Exists) { Copy-Item -LiteralPath $backup.Backup -Destination $backup.Target -Force }
            elseif (Test-Path -LiteralPath $backup.Target) { Remove-Item -LiteralPath $backup.Target -Force }
         } catch {
            $keepBackups = $true
            Write-Warning "Could not restore $($backup.Target): $_"
         }
      }
      if ($keepBackups) { Write-Warning "Recovery copies are in $stage. The backup index follows."; $backups | Format-Table Target, Backup -AutoSize }
   }
   throw
} finally {
   $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
   if (-not $keepBackups -and [IO.Path]::GetDirectoryName($resolvedStage) -eq $tempRoot.TrimEnd('\', '/')) {
      Remove-Item -LiteralPath $resolvedStage -Recurse -Force
   }
}
