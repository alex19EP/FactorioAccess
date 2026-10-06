<#
.SYNOPSIS
Moves vanilla walking from WASD to the arrow keys, leaving WASD to the FactorioAccess cursor.

.DESCRIPTION
Edits the [controls] section of Factorio's config.ini: move-up, move-down, move-left and
move-right become UP, DOWN, LEFT and RIGHT. Their -alternative and -controller bindings are left
alone. A copy of the old file is kept beside it. Close Factorio first: it rewrites config.ini when
it exits.

With -Revert the four controls go back to the game's defaults (W, S, A, D).

.PARAMETER ConfigPath
config.ini to edit. Defaults to the one in %APPDATA%\Factorio\config, the location of the
installer and Steam builds.

.EXAMPLE
pwsh helper-scripts/rebind-walking.ps1
#>
[CmdletBinding()]
param(
   [string]$ConfigPath = (Join-Path $env:APPDATA 'Factorio\config\config.ini'),
   [switch]$Revert
)

$ErrorActionPreference = 'Stop'

$walking = [ordered]@{
   'move-up'    = @{ Arrow = 'UP'; Default = 'W' }
   'move-down'  = @{ Arrow = 'DOWN'; Default = 'S' }
   'move-left'  = @{ Arrow = 'LEFT'; Default = 'A' }
   'move-right' = @{ Arrow = 'RIGHT'; Default = 'D' }
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

# Missing lines go right after the section header, in order.
$insertAt = $start + 2
foreach ($control in $walking.Keys) {
   # A set binding reads "move-up=UP"; one left at its default is commented out as "; move-up=W".
   $pattern = '^(; )?' + [regex]::Escape($control) + '='
   $wanted = if ($Revert) { "; $control=$($walking[$control].Default)" } else { "$control=$($walking[$control].Arrow)" }
   $found = $false
   for ($i = $start + 2; $i -lt $end; $i += 2) {
      if ($lines[$i] -match $pattern) {
         if ($lines[$i] -ne $wanted) { Write-Host "$($lines[$i].PadRight(20)) -> $wanted" }
         $lines[$i] = $wanted
         $found = $true
         break
      }
   }
   if (-not $found) {
      $lines.InsertRange($insertAt, [string[]]@($wanted, $newline))
      $insertAt += 2
      $end += 2
      Write-Host "added $wanted"
   }
}

$backup = "$ConfigPath.$(Get-Date -Format 'yyyyMMdd-HHmmss').bak"
Copy-Item -LiteralPath $ConfigPath -Destination $backup
[IO.File]::WriteAllText($ConfigPath, ($lines -join ''), [Text.UTF8Encoding]::new($false))
Write-Host "Saved $ConfigPath (the old file is $backup)"
