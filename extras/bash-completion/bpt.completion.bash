# A bash completion script for bpt

# Method:
# Keep a separate pointer $CWORDI (evaluated result stored in $CWORD)
#   which iterates over the $COMP_WORDS array.
# Use $CWORD to run the "state machine" that keeps track of where in
#   the command line we are. That is, we use the information to
#   call the appropriate _bpt_complete_* function.
# Subcommands are implemented as functions.
# Each command can have FLAGS, POSITIONAL, and SUBCOMMANDS.

# Completion for a single argument (assumes that this is the arg that needs completion)
# Input: ARG
# Output: COMPREPLY
# ARG can be:
#  - "directory" - completes as a directory
#  - "file" - completes as a file
#  - "file-or-list: some list" - completes as a file or as a value in the
#                                space-separated "some list"
#  - "some list" - Requires one of these enumerated values
#  - " " - Use no completion for this argument
_bpt_complete_single_arg()
{
  case $ARG in
  "directory")
    COMPREPLY+=( $(compgen -d -- $CWORD) )
    ;;
  "file")
    COMPREPLY+=( $(compgen -f -- $CWORD) )
    ;;
  "file-or-list: "*)
    COMPREPLY+=( $(compgen -f -- $CWORD) )
    COMPREPLY+=( $(compgen -W "${ARG//file-or-list: /}" -- $CWORD) )
    ;;
  *)
    COMPREPLY+=( $(compgen -W "$ARG" -- $CWORD) )
    ;;
  esac
} &&

# Evaluates the completion for a positional argument.
# Input: POS_ARG
# POS_ARG may have the same values as in _bpt_complete_single_arg, but also:
#  - "repeat:..." where the "..." is any of the valid _bpt_complete_single_arg specifiers.
#    This will consume all remaining commandline arguments to follow the specified schema.
_bpt_complete_positional_arg()
{
  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    local ARG
    case $POS_ARG in
    "repeat:"*)
      ARG="${POS_ARG//repeat:/}"
      while [[ $CWORDI -lt $COMP_CWORD ]]; do
        ((CWORDI=CWORDI+1))
        CWORD="${COMP_WORDS[$CWORDI]}"
      done
      ;;
    *)
      ARG="$POS_ARG"
      ;;
    esac

    _bpt_complete_single_arg
    return
  fi

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"
} &&

# Evaluates the completion for a flag argument
# If ARG is an empty string, assumes the flag takes no arguments.
# ARG can also be any of what is specified in _bpt_complete_single_arg
_bpt_complete_flags()
{
  local ARG
  ARG="${FLAGS[$CWORD]}"
  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ -n "$ARG" ]]; then
    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      _bpt_complete_single_arg
    else
      ((CWORDI=CWORDI+1))
      CWORD="${COMP_WORDS[$CWORDI]}"
    fi
  fi
} &&

# General completion function.
# Inputs:
#  - SUBCOMMANDS - Associative Array: SubcommandString -> Function
#  - FLAGS - Associative Array: FlagString -> ARG specifier (see _bpt_complete_flags)
#  - POSITIONAL - Array: List of POS_ARG specifiers (see _bpt_complete_positional_arg)
# Only one of SUBCOMMANDS or POSITIONAL can contain elements at the same time
# Output: COMPREPLY
_bpt_complete_command()
{
  local RESULT_WORDS SUBCOMMAND FLAG
  if [[ ${#POSITIONAL[@]} -eq 0 ]]; then
    # SUBCOMMANDS branch

    # If we are at the final entry (the one we are trying to complete)
    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      # Fill results with the subcommands
      RESULT_WORDS="${!SUBCOMMANDS[@]}"

      # And any flags if the completee starts with "-"
      # (or if we have no subcommands)
      if [[ $CWORD == "-"* || -z $RESULT_WORDS ]]; then
        RESULT_WORDS+=" ${!FLAGS[@]}"
      fi

      COMPREPLY+=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
      return
    fi

    # Otherwise, we need to jump to the correct state.
    SUBCOMMAND="${SUBCOMMANDS[$CWORD]}"
    FLAG="${FLAGS[$CWORD]}"
    if [[ -n $FLAG || $CWORD == "-"* ]]; then
      # If $CWORD is a flag or partial flag, parse it as a flag
      _bpt_complete_flags
      # After parsing a flag, we should behave as if we are still parsing the command:
      _bpt_complete_command
    elif [[ -n $SUBCOMMAND ]]; then
      # Drop the subcommand word
      ((CWORDI=CWORDI+1))
      CWORD="${COMP_WORDS[$CWORDI]}"
      # Call the subcommand's completion function
      $SUBCOMMAND
    fi
  else
    # Do positional arguments
    local POS_ARG CONTINUE
    # Go through all the positional arguments.
    # We expect to either see that positional argument or a flag.
    for POS_ARG in "${POSITIONAL[@]}"; do
      CONTINUE=1
      # Parse as many flags as we can
      while [[ $CWORDI -lt $COMP_CWORD && $CONTINUE -eq 1 ]]; do
        FLAG="${FLAGS[$CWORD]}"
        if [[ -n $FLAG || $CWORD == "-"* ]]; then
          _bpt_complete_flags
          CONTINUE=1
        else
          _bpt_complete_positional_arg
          CONTINUE=0 # break; we need to advance POS_ARG
        fi
      done

      if [[ $CONTINUE -ne 0 ]]; then
        # This means that $CWORDI -ge $COMP_CWORD,
        # i.e. that we have reached the completing argument.

        # Fill out COMPREPLY with the completions for this POS_ARG
        _bpt_complete_positional_arg

        # Also add any relevant flags
        if [[ $CWORD == "-"* || -z "${COMPREPLY[@]}" ]]; then
          RESULT_WORDS=" ${!FLAGS[@]}"
          COMPREPLY+=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
        fi

        return
      fi
    done

    # If we have parsed all the positional arguments, still parse any remaining flags.
    local POSITIONAL
    POSITIONAL=()
    # This will hit the SUBCOMMANDS branch, but as SUBCOMMANDS is empty, it works fine.
    _bpt_complete_command
  fi
} &&

# Computes the bpt builtin toolchains
# Output: BUILTIN_TOOLCHAINS
_bpt_complete_get_builtin_toolchains()
{
  local gccs clangs cxxstds base ccached debugged cxx98 cxx03 cxx11 cxx14 cxx17 cxx20

  gccs=$(find $(echo $PATH | sed 's/:/ /g') -regextype posix-extended -regex '^.*/gcc(-[0-9]+)?$' -print | sed -E 's/^.*\/(gcc(-[0-9]+)?)/:\1/g')
  clangs=$(find $(echo $PATH | sed 's/:/ /g') -regextype posix-extended -regex '^.*/clang(-[0-9]+)?$' -print | sed -E 's/^.*\/(clang(-[0-9]+)?)/:\1/g')

  base="$gccs $clangs"

  base=($base)
  cxx98=(${base[@]/#/:c++98})
  cxx03=(${base[@]/#/:c++03})
  cxx11=(${base[@]/#/:c++11})
  cxx14=(${base[@]/#/:c++14})
  cxx17=(${base[@]/#/:c++17})
  cxx20=(${base[@]/#/:c++20})
  base=($cxx98 $cxx03 $cxx11 $cxx14 $cxx17 $cxx20)

  ccached=${base[@]/#/:ccache}

  BUILTIN_TOOLCHAINS="${base[@]} $ccached"
  BUILTIN_TOOLCHAINS=($BUILTIN_TOOLCHAINS)
  debugged=${BUILTIN_TOOLCHAINS[@]/#/:debug}
  BUILTIN_TOOLCHAINS="${BUILTIN_TOOLCHAINS[@]} $debugged"
} &&

# bpt build
_bpt_complete_build()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _bpt_complete_get_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--project]='directory'
    [--no-tests]=''
    [--no-apps]=''
    [--no-warnings]=''
    [--output]='directory'
    [--add-repo]=' '
    [--update-repos]=''
    [--libman-index]='file'
    [--jobs]=' '
    [--tweaks-dir]='directory'
  )
  POSITIONAL=()

  _bpt_complete_command
} &&

# bpt compile-file
_bpt_complete_compile_file()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _bpt_complete_get_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--project]='directory'
    [--no-warnings]=''
    [--output]='file'
    [--libman-index]='file'
    [--jobs]=' '
    [--tweaks-dir]='directory'
  )
  POSITIONAL=(
    'repeat:file' # <source-files> ...
  )
} &&

# bpt build-deps
_bpt_complete_build_deps()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _bpt_complete_get_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--jobs]=' '
    [--output]='file'
    [--libman-index]=' '
    [--deps-file]='file'
    [--cmake]=' '
    [--tweaks-dir]='directory'
  )
  POSITIONAL=(
    # <dependency> ... # No completion implemented
  )

  _bpt_complete_command
} &&

# bpt pkg create
_bpt_complete_pkg_create()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--project]='directory'
    [--out]=' '
    [--if-exists]='replace ignore fail'
  )
  POSITIONAL=()

  _bpt_complete_command
} &&

# bpt pkg prefetch
_bpt_complete_pkg_prefetch()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    # <pkg-id> ... # No completion implemented
  )

  _bpt_complete_command
} &&


# bpt pkg search
_bpt_complete_pkg_search()
{
  local RESULT_WORDS POSITIONAL PACKAGES
  if [[ -x "$(command -v bpt)" ]]; then
    PACKAGES=$(bpt pkg search | grep Name: | sed -E 's/Name://g')
  else
    PACKAGES=""
  fi

  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    "$PACKAGES"
  )

  _bpt_complete_command
}

# bpt pkg
_bpt_complete_pkg()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=(
    [init-db]=:
    [ls]=:
    [create]=_bpt_complete_pkg_create
    [prefetch]=_bpt_complete_pkg_prefetch
    [search]=_bpt_complete_pkg_search
  )
  FLAGS=()
  POSITIONAL=()

  _bpt_complete_command
} &&

# bpt repo init
_bpt_complete_repo_init()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--if-exists]='replace ignore fail'
    [--name]=' '
  )
  POSITIONAL=(
    'directory' # <repo-dir>
  )

  _bpt_complete_command
} &&

# bpt repo ls
_bpt_complete_repo_ls()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    'directory' # <repo-dir>
  )

  _bpt_complete_command
} &&


# bpt repo import
_bpt_complete_repo_import()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=()
  FLAGS=(
    [--if-exists]="replace ignore fail"
  )
  POSITIONAL=(
    'directory' # <repo-dir>
    'repeat:file' # <sdist-file-path> ...
  )

  _bpt_complete_command
} &&

# bpt repo remove
_bpt_complete_repo_remove()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    'directory' # <repo-dir>
    # <pkg-id> ... # No completion implemented
  )

  _bpt_complete_command
} &&

# bpt repo
_bpt_complete_repo()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  local RESULT_WORDS
  SUBCOMMANDS=(
    [init]=_bpt_complete_repo_init
    [ls]=_bpt_complete_repo_ls
    [import]=_bpt_complete_repo_import
    [remove]=_bpt_complete_repo_remove
  )
  FLAGS=()
  POSITIONAL=()

  _bpt_complete_command
} &&

# bpt install-yourself
_bpt_complete_install_yourself()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--where]='user system'
    [--dry-run]=''
    [--no-modify-path]=''
    [--symlink]=''
  )
  POSITIONAL=()

  _bpt_complete_command
} &&

# bpt
_bpt_complete_impl()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=(
    [build]=_bpt_complete_build
    [compile-file]=_bpt_complete_compile_file
    [build-deps]=_bpt_complete_build_deps
    [pkg]=_bpt_complete_pkg
    [repo]=_bpt_complete_repo
    [install-yourself]=_bpt_complete_install_yourself
  )
  FLAGS=(
    [--log-level]='trace debug info warn error critical silent'
    [--crs-cache-dir]='directory'
  )
  POSITIONAL=()

  _bpt_complete_command
} &&

_bpt_complete()
{
  local CWORDI CWORD
  COMPREPLY=()
  CWORDI=1
  CWORD="${COMP_WORDS[$CWORDI]}"
  _bpt_complete_impl
  return 0
} &&
complete -F _bpt_complete bpt
