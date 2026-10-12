#requires -Version 7
# Claude Code worktree hooks for this repo, and the same setup by hand.
#
#   worktree.ps1 create          WorktreeCreate hook: JSON with `name` on stdin; makes
#                                .claude/worktrees/<name> on branch <name> from the session's HEAD,
#                                sets it up, prints its path (the only stdout the hook may write)
#   worktree.ps1 remove          WorktreeRemove hook: JSON with `worktree_path` on stdin
#   worktree.ps1 setup [<path>]  set up a worktree made with a plain `git worktree add`
#
# Setting up links what a worktree lacks to the main checkout: the native/vendor submodules
# (junctions; a worktree's are empty), .tools (EmmyLua and the Factorio API library) and .env
# (a hard link). Removing unlinks them first, so nothing deletes through a link into the main
# checkout, and refuses a worktree with uncommitted work.
param(
    [Parameter(Mandatory, Position = 0)][ValidateSet('create', 'remove', 'setup')][string]$Action,
    [Parameter(Position = 1)][string]$Path
)
$ErrorActionPreference = 'Stop'

function Say([string]$Text) { [Console]::Error.WriteLine("[worktree] $Text") }

# Runs git, its output to stderr, failing on a non-zero exit.
function RunGit {
    $out = & git @args 2>&1
    $out | ForEach-Object { [Console]::Error.WriteLine("$_") }
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed" }
}

function MainCheckout([string]$From) {
    $common = git -C $From rev-parse --path-format=absolute --git-common-dir
    if ($LASTEXITCODE -ne 0) { throw "$From is not in a git repository" }
    [IO.Path]::GetFullPath((Split-Path -Parent $common))
}

function Same([string]$A, [string]$B) {
    [IO.Path]::GetFullPath($A).TrimEnd('\', '/') -eq [IO.Path]::GetFullPath($B).TrimEnd('\', '/')
}

# Whether $Dir is a worktree's own root. In a folder that is not one (a removed worktree's empty
# leftover), git would run in whatever repository holds the folder: the main checkout.
function IsWorktree([string]$Dir) {
    if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { return $false }
    $top = git -C $Dir rev-parse --show-toplevel 2>$null
    $LASTEXITCODE -eq 0 -and (Same $top $Dir)
}

function IsLink([string]$Item) {
    $i = Get-Item -LiteralPath $Item -Force -ErrorAction SilentlyContinue
    $i -and $i.LinkType
}

# Deletes a junction or symbolic link itself, never what it points at.
function Unlink([string]$Item) {
    if (IsLink $Item) { [IO.Directory]::Delete($Item, $false) }
}

function Setup([string]$Worktree) {
    if (-not (IsWorktree $Worktree)) { throw "$Worktree is not a worktree" }
    $main = MainCheckout $Worktree
    if (Same $Worktree $main) { throw "$Worktree is the main checkout" }

    $paths = git -C $main config -f .gitmodules --get-regexp 'path$' | ForEach-Object { ($_ -split ' ', 2)[1] }
    foreach ($rel in $paths) {
        $link = Join-Path $Worktree $rel
        $target = Join-Path $main $rel
        if ((IsLink $link) -or -not (Test-Path -LiteralPath $target)) { continue }
        if (Test-Path -LiteralPath $link) {
            if (Get-ChildItem -LiteralPath $link -Force) { continue } # checked out for real
            Remove-Item -LiteralPath $link -Force
        }
        New-Item -ItemType Junction -Path $link -Target $target | Out-Null
        Say "linked $rel"
    }

    $tools = Join-Path $Worktree '.tools'
    if (-not (Test-Path -LiteralPath $tools) -and (Test-Path -LiteralPath (Join-Path $main '.tools'))) {
        New-Item -ItemType Junction -Path $tools -Target (Join-Path $main '.tools') | Out-Null
        Say 'linked .tools'
    }

    $envFile = Join-Path $Worktree '.env'
    if (-not (Test-Path -LiteralPath $envFile) -and (Test-Path -LiteralPath (Join-Path $main '.env'))) {
        New-Item -ItemType HardLink -Path $envFile -Target (Join-Path $main '.env') | Out-Null
        Say 'linked .env'
    }
}

function Create([string]$Name, [string]$From) {
    if ($Name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw "unusable worktree name: $Name" }
    $main = MainCheckout $From
    $worktree = Join-Path $main ".claude\worktrees\$Name"
    # An empty folder a removed worktree left behind
    if ((Test-Path -LiteralPath $worktree) -and -not (IsWorktree $worktree) -and
        -not (Get-ChildItem -LiteralPath $worktree -Force)) {
        Remove-Item -LiteralPath $worktree -Force
    }
    if (-not (Test-Path -LiteralPath $worktree)) {
        git -C $main show-ref --verify --quiet "refs/heads/$Name"
        if ($LASTEXITCODE -eq 0) {
            RunGit -C $main worktree add $worktree $Name
        }
        else {
            $base = git -C $From rev-parse HEAD
            RunGit -C $main worktree add -b $Name $worktree $base
        }
    }
    Setup $worktree
    $worktree
}

function Remove([string]$Worktree) {
    if (-not (IsWorktree $Worktree)) {
        # Already gone; drop an empty leftover folder and the stale registration
        if ((Test-Path -LiteralPath $Worktree) -and -not (Get-ChildItem -LiteralPath $Worktree -Force)) {
            Remove-Item -LiteralPath $Worktree -Force
        }
        $main = MainCheckout (Split-Path -Parent $Worktree)
        RunGit -C $main worktree prune
        Say "$Worktree is not a worktree; nothing to remove"
        return
    }
    $main = MainCheckout $Worktree
    $branch = git -C $Worktree branch --show-current
    # git removes a worktree with submodules only with --force, which would also drop uncommitted
    # work: refuse that here instead.
    $dirty = git -C $Worktree status --porcelain --ignore-submodules=all
    if ($LASTEXITCODE -ne 0) { throw "git status failed in $Worktree" }
    if ($dirty) { throw "$Worktree has uncommitted changes; commit or discard them first:`n$($dirty -join "`n")" }
    $paths = git -C $main config -f .gitmodules --get-regexp 'path$' | ForEach-Object { ($_ -split ' ', 2)[1] }
    foreach ($rel in $paths) { Unlink (Join-Path $Worktree $rel) }
    Unlink (Join-Path $Worktree '.tools')
    # A hard link: deleting it leaves the main checkout's .env
    $envFile = Join-Path $Worktree '.env'
    if (Test-Path -LiteralPath $envFile) { Remove-Item -LiteralPath $envFile -Force }
    git -C $main worktree remove --force $Worktree 2>&1 | ForEach-Object { [Console]::Error.WriteLine("$_") }
    # Windows keeps a folder a process still has as its working directory (the session's shell):
    # git then fails after emptying it. Gone as a worktree is removed; the folder can stay.
    if (IsWorktree $Worktree) { throw "git worktree remove failed for $Worktree" }
    RunGit -C $main worktree prune
    if (Test-Path -LiteralPath $Worktree) { Say "the folder is in use and stays, empty: $Worktree" }
    Say "removed $Worktree"
    # The branch goes too once its work is merged; unmerged work stays on it
    if ($branch) {
        git -C $main branch -d $branch 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) { Say "deleted branch $branch" } else { Say "kept branch ${branch}, not merged" }
    }
}

try {
    switch ($Action) {
        'create' {
            $hook = [Console]::In.ReadToEnd() | ConvertFrom-Json
            $from = if ($hook.cwd) { $hook.cwd } else { (Get-Location).Path }
            [Console]::Out.WriteLine((Create $hook.name $from))
        }
        'remove' {
            $hook = [Console]::In.ReadToEnd() | ConvertFrom-Json
            Remove $hook.worktree_path
        }
        'setup' {
            Setup ($(if ($Path) { $Path } else { (Get-Location).Path }))
        }
    }
    exit 0
}
catch {
    Say $_.Exception.Message
    exit 1
}
