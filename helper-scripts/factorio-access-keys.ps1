<#
.SYNOPSIS
Moves Factorio's controls to the keys FactorioAccess expects: walking to the arrow keys, connect and
disconnect train to CONTROL + J and CONTROL + K, research to CONTROL + T, zoom to = and -, and every
mouse button control onto a key: left button [, right button ], middle button \.

.DESCRIPTION
Edits the [controls] section of Factorio's config.ini. Close Factorio first: it rewrites config.ini
when it exits. A copy of the old file is kept beside it. Controller bindings are left alone.

Fixed bindings come from $fixedBindings below:
- move-up, move-down, move-left and move-right become UP, DOWN, LEFT and RIGHT, leaving WASD to the
  cursor.
- connect-train and disconnect-train become CONTROL + J and CONTROL + K, leaving J (cursor to the
  character) and K (cursor coordinates) to the mod, where pressing K otherwise has the game answer
  that it cannot disconnect rolling stock.
- open-technology-gui (research) becomes CONTROL + T, leaving T (time of day) to the mod.
- Zoom in and zoom out are bound on the wheel in both slots (plain and with SHIFT). Their
  alternative slot gets EQUALS and MINUS, so the plain wheel keeps working. FactorioAccess speaks
  each zoom change.

Every control bound to mouse button 1, 2 or 3 (with any modifiers) gets the same binding on
LEFTBRACKET, RIGHTBRACKET or BACKSLASH. The key goes into the control's free alternative slot, so the
mouse keeps working. A control whose two slots are both taken has its mouse buttons replaced by the
keys instead. The game itself accepts these: only its settings screen refuses a key for a mouse-only
control, config.ini is read without that check, and a key binding triggers a control like a mouse
binding. Inventory and crafting slots are the exception: the game turns their controls into the
mouse buttons slot buttons react to, so their keys do nothing there. They are bound anyway.

With -Revert the keys come off again. A fixed binding that still holds its key goes back to the
game's default. Added mouse alternatives are cleared and replaced bindings go back to their mouse
buttons. A bracket or backslash binding of your own on a control that also has the matching mouse
binding is treated as one of ours. Bindings left at their defaults, the commented lines, are never
touched: a mod's own default on a bracket key stays.

.PARAMETER GameDir
The game folder, the one holding bin and config-path.cfg. Its config-path.cfg says where its
config.ini is, as the game reads it: inside the folder for the portable (zip) version, in
%APPDATA%\Factorio\config for Steam and the installer. Defaults to the script's own folder when that
is a game folder; otherwise %APPDATA%\Factorio\config\config.ini is edited, which suits Steam and the
installer but not a portable copy.

.PARAMETER ConfigPath
config.ini to edit, in place of finding it through -GameDir.

.EXAMPLE
factorio-access-keys.cmd

.EXAMPLE
factorio-access-keys.cmd -GameDir "D:\Games\Factorio"

.EXAMPLE
powershell -ExecutionPolicy Bypass -File factorio-access-keys.ps1 -Revert
#>
[CmdletBinding()]
param(
   [string]$GameDir,
   [string]$ConfigPath,
   [switch]$Revert
)

$ErrorActionPreference = 'Stop'

# Name, the key it gets, and the game's default it goes back to on -Revert.
$fixedBindings = @(
   @{ Name = 'move-up'; Key = 'UP'; Default = 'W' },
   @{ Name = 'move-down'; Key = 'DOWN'; Default = 'S' },
   @{ Name = 'move-left'; Key = 'LEFT'; Default = 'A' },
   @{ Name = 'move-right'; Key = 'RIGHT'; Default = 'D' },
   @{ Name = 'connect-train'; Key = 'CONTROL + J'; Default = 'J' },
   @{ Name = 'disconnect-train'; Key = 'CONTROL + K'; Default = 'K' },
   @{ Name = 'open-technology-gui'; Key = 'CONTROL + T'; Default = 'T' },
   @{ Name = 'zoom-in-alternative'; Key = 'EQUALS'; Default = 'SHIFT + mouse-wheel-up' },
   @{ Name = 'zoom-out-alternative'; Key = 'MINUS'; Default = 'SHIFT + mouse-wheel-down' }
)

$keyForButton = @{ '1' = 'LEFTBRACKET'; '2' = 'RIGHTBRACKET'; '3' = 'BACKSLASH' }
$buttonForKey = @{ 'LEFTBRACKET' = '1'; 'RIGHTBRACKET' = '2'; 'BACKSLASH' = '3' }

# "SHIFT + mouse-button-1" -> "SHIFT + LEFTBRACKET", or $null for anything else.
function ConvertTo-KeyBinding([string]$binding) {
   if ($binding -match '^(.*)mouse-button-([123])$') { return $Matches[1] + $keyForButton[$Matches[2]] }
   return $null
}

function ConvertTo-MouseBinding([string]$binding) {
   if ($binding -match '^(.*)(LEFTBRACKET|RIGHTBRACKET|BACKSLASH)$') {
      return $Matches[1] + 'mouse-button-' + $buttonForKey[$Matches[2]]
   }
   return $null
}

# The config.ini the game in $gameDir uses, as config-path.cfg names it, or $null when $gameDir is
# not a game folder.
function Find-GameConfig([string]$gameDir) {
   $cfg = Join-Path $gameDir 'config-path.cfg'
   if (-not (Test-Path -LiteralPath $cfg)) { return $null }
   foreach ($line in [IO.File]::ReadAllLines($cfg)) {
      if ($line -match '^config-path=(.*)$') {
         $dir = $Matches[1].Replace('__PATH__executable__', (Join-Path $gameDir 'bin\x64'))
         $dir = $dir.Replace('__PATH__system-write-data__', (Join-Path $env:APPDATA 'Factorio'))
         return [IO.Path]::GetFullPath((Join-Path $dir 'config.ini'))
      }
   }
   return $null
}

if (-not $ConfigPath -and $GameDir) {
   $ConfigPath = Find-GameConfig ([IO.Path]::GetFullPath($GameDir))
   if (-not $ConfigPath) { throw "$GameDir is not a game folder: it has no config-path.cfg." }
}
if (-not $ConfigPath) {
   $ConfigPath = Find-GameConfig $PSScriptRoot
   if (-not $ConfigPath) { $ConfigPath = Join-Path $env:APPDATA 'Factorio\config\config.ini' }
}

if (Get-Process -Name factorio -ErrorAction SilentlyContinue) {
   throw 'Factorio is running. Close it first: it rewrites config.ini when it exits.'
}
if (-not (Test-Path -LiteralPath $ConfigPath)) {
   throw "No config.ini at $ConfigPath. Start Factorio once to create it, or pass -GameDir with the game folder."
}
Write-Host "Editing $ConfigPath"

# The game writes some lines with LF and the rest with CRLF. Splitting with the endings captured
# gives line, ending, line, ending, ...; only lines (even indices) are edited, so every other byte
# of the file stays as it was.
$text = [IO.File]::ReadAllText($ConfigPath)
$lines = [Collections.Generic.List[string]]::new([regex]::Split($text, '(\r?\n)'))
$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }

$start = -1
for ($i = 0; $i -lt $lines.Count; $i += 2) {
   if ($lines[$i] -eq '[controls]') { $start = $i; break }
}
if ($start -lt 0) {
   throw "$ConfigPath has no [controls] section. Start Factorio, reach the main menu and exit, then run this again."
}
$end = $lines.Count
for ($i = $start + 2; $i -lt $lines.Count; $i += 2) {
   if ($lines[$i].StartsWith('[')) { $end = $i; break }
}

# A set binding reads "build=mouse-button-1"; one left at its default is commented out as
# "; build=mouse-button-1", with the default as its value. Either way the value is what applies.
$entries = [ordered]@{}
for ($i = $start + 2; $i -lt $end; $i += 2) {
   if ($lines[$i] -match '^(; )?([A-Za-z0-9_.-]+)=(.*)$') {
      $name = $Matches[2]
      $set = -not $Matches[1]
      if (-not $entries.Contains($name) -or $set) { $entries[$name] = @{ Index = $i; Value = $Matches[3]; Set = $set } }
   }
}

$changes = 0
$replaced = [Collections.Generic.List[string]]::new()
$inserts = [Collections.Generic.List[object]]::new()

function Set-Binding([string]$name, [string]$value, [bool]$asDefault = $false) {
   $line = if ($asDefault) { "; $name=$value" } else { "$name=$value" }
   $entry = $entries[$name]
   if ($lines[$entry.Index] -eq $line) { return }
   Write-Host "$($lines[$entry.Index].PadRight(52)) -> $line"
   $lines[$entry.Index] = $line
   $entry.Value = $value
   $entry.Set = -not $asDefault
   $script:changes++
}

# A control with no line at all gets one after its primary binding, or else after the section header.
function Add-Binding([string]$name, [string]$value) {
   $primary = $name -replace '-alternative$', ''
   $after = if ($primary -ne $name -and $entries.Contains($primary)) { $entries[$primary].Index } else { $start }
   $inserts.Add(@{ After = $after; Line = "$name=$value" })
}

foreach ($binding in $fixedBindings) {
   $name = $binding.Name
   if ($Revert) {
      if ($entries.Contains($name) -and $entries[$name].Set -and $entries[$name].Value -eq $binding.Key) {
         Set-Binding $name $binding.Default $true
      }
   } elseif ($entries.Contains($name)) {
      Set-Binding $name $binding.Key
   } else {
      Add-Binding $name $binding.Key
   }
}

foreach ($name in @($entries.Keys)) {
   if ($name.EndsWith('-alternative')) { continue }
   $primary = $entries[$name].Value
   $altName = "$name-alternative"
   $alternative = if ($entries.Contains($altName)) { $entries[$altName].Value } else { $null }

   if ($Revert) {
      # Only set lines can be ours; a commented line is the game's or a mod's own default.
      $altMouse = if ($null -ne $alternative -and $entries[$altName].Set) { ConvertTo-MouseBinding $alternative } else { $null }
      if ($altMouse -and $altMouse -eq $primary) {
         Set-Binding $altName '' $true
         continue
      }
      $primaryMouse = if ($entries[$name].Set) { ConvertTo-MouseBinding $primary } else { $null }
      if ($primaryMouse) { Set-Binding $name $primaryMouse }
      if ($altMouse) { Set-Binding $altName $altMouse }
      continue
   }

   $primaryKey = ConvertTo-KeyBinding $primary
   $altKey = if ($null -ne $alternative) { ConvertTo-KeyBinding $alternative } else { $null }
   if (-not $primaryKey -and -not $altKey) { continue }
   if ($primaryKey -and $alternative -eq $primaryKey) { continue }

   if ($primaryKey -and $null -eq $alternative) {
      Add-Binding $altName $primaryKey
   } elseif ($primaryKey -and $alternative -eq '') {
      Set-Binding $altName $primaryKey
   } else {
      # No free slot: the keys take the mouse buttons' place.
      if ($primaryKey) { Set-Binding $name $primaryKey }
      if ($altKey) { Set-Binding $altName $altKey }
      $replaced.Add($name)
   }
}

foreach ($insert in ($inserts | Sort-Object { $_.After } -Descending)) {
   $lines.InsertRange($insert.After + 2, [string[]]@($insert.Line, $newline))
   Write-Host "added $($insert.Line)"
   $changes++
}

if ($changes -eq 0) {
   Write-Host 'Nothing to change.'
   return
}
$backup = "$ConfigPath.$(Get-Date -Format 'yyyyMMdd-HHmmss').bak"
Copy-Item -LiteralPath $ConfigPath -Destination $backup
[IO.File]::WriteAllText($ConfigPath, ($lines -join ''), [Text.UTF8Encoding]::new($false))
Write-Host "$changes bindings changed. Saved $ConfigPath (the old file is $backup)"
if ($replaced.Count) { Write-Host "No free slot, mouse buttons replaced by keys: $($replaced -join ', ')" }
