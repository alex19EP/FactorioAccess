<#
.SYNOPSIS
Gives every mouse button control in Factorio a key: left button [, right button ], middle button \.

.DESCRIPTION
Edits the [controls] section of Factorio's config.ini. Every control bound to mouse button 1, 2 or 3
(with any modifiers) gets the same binding on LEFTBRACKET, RIGHTBRACKET or BACKSLASH. The key goes
into the control's free alternative slot, so the mouse keeps working. A control whose two slots are
both taken has its mouse buttons replaced by the keys instead. Controller bindings are left alone.

The game itself accepts these: only its settings screen refuses a key for a mouse-only control,
config.ini is read without that check, and a key binding triggers a control like a mouse binding.

Inventory and crafting slots are the exception: the game turns their controls into the mouse
buttons slot buttons react to, so their keys do nothing there. They are bound anyway.

A copy of the old file is kept beside it. Close Factorio first: it rewrites config.ini when it
exits.

With -Revert the keys come off again: added alternatives are cleared and replaced bindings go back
to their mouse buttons. A bracket or backslash binding of your own on a control that also has the
matching mouse binding is treated as one of ours.

.PARAMETER ConfigPath
config.ini to edit. Defaults to the one in %APPDATA%\Factorio\config, the location of the
installer and Steam builds.

.EXAMPLE
pwsh helper-scripts/bind-mouse-keys.ps1
#>
[CmdletBinding()]
param(
   [string]$ConfigPath = (Join-Path $env:APPDATA 'Factorio\config\config.ini'),
   [switch]$Revert
)

$ErrorActionPreference = 'Stop'

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

if (Get-Process -Name factorio -ErrorAction SilentlyContinue) {
   throw 'Factorio is running. Close it first: it rewrites config.ini when it exits.'
}
if (-not (Test-Path -LiteralPath $ConfigPath)) {
   throw "No config.ini at $ConfigPath. Start Factorio once to create it, or pass -ConfigPath."
}

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
if ($start -lt 0) { throw "$ConfigPath has no [controls] section." }
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
      if (-not $entries.Contains($name) -or $set) { $entries[$name] = @{ Index = $i; Value = $Matches[3] } }
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
   $script:changes++
}

foreach ($name in @($entries.Keys)) {
   if ($name.EndsWith('-alternative')) { continue }
   $primary = $entries[$name].Value
   $altName = "$name-alternative"
   $alternative = if ($entries.Contains($altName)) { $entries[$altName].Value } else { $null }

   if ($Revert) {
      $altMouse = if ($null -ne $alternative) { ConvertTo-MouseBinding $alternative } else { $null }
      if ($altMouse -and $altMouse -eq $primary) {
         Set-Binding $altName '' $true
         continue
      }
      $primaryMouse = ConvertTo-MouseBinding $primary
      if ($primaryMouse) { Set-Binding $name $primaryMouse }
      if ($altMouse) { Set-Binding $altName $altMouse }
      continue
   }

   $primaryKey = ConvertTo-KeyBinding $primary
   $altKey = if ($null -ne $alternative) { ConvertTo-KeyBinding $alternative } else { $null }
   if (-not $primaryKey -and -not $altKey) { continue }
   if ($primaryKey -and $alternative -eq $primaryKey) { continue }

   if ($primaryKey -and $null -eq $alternative) {
      $inserts.Add(@{ After = $entries[$name].Index; Line = "$altName=$primaryKey" })
   } elseif ($primaryKey -and $alternative -eq '') {
      Set-Binding $altName $primaryKey
   } else {
      # No free slot: the keys take the mouse buttons' place.
      if ($primaryKey) { Set-Binding $name $primaryKey }
      if ($altKey) { Set-Binding $altName $altKey }
      $replaced.Add($name)
   }
}

# Controls with no alternative line at all (some mods' inputs) get one right after their binding.
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
