#!/usr/bin/env bash
# PreToolUse hook on Claude's Bash and PowerShell calls: one that launches the game through
# `just run` or `just start` leaves a flag, and the recipe then launches muted (`_mute-for-agent` in
# the justfile, native/src/speech.h). Claude Code runs hooks for its own tool calls only, never for
# a command the developer types with `!`, and both have the same environment, so this is what
# tells them apart. Decides nothing: the call goes on to the usual permission check. Bash rather
# than pwsh because it runs before every command, and pwsh takes half a second to start.
if grep -Eq '(^|[^[:alnum:]_-])just( +-[^ ]+( +[^ -][^ ]*)?)* +(run|start)([^[:alnum:]_-]|$)'; then
   dir="$(cygpath -u "$LOCALAPPDATA")/FactorioAccess"
   mkdir -p "$dir" && touch "$dir/agent-launch"
fi
exit 0
