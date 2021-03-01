# A bash completion script for dds

# Method:
# Keep a separate pointer $CWORDI (evaluated result stored in $CWORD)
#   which iterates over the $COMP_WORDS array.
# Use $CWORD to run the "state machine" that keeps track of where in
#   the command line we are. That is, we use the information to
#   call the appropriate _dds_complete_* function.
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
_dds_complete_single_arg()
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
# POS_ARG may have the same values as in _dds_complete_single_arg, but also:
#  - "repeat:..." where the "..." is any of the valid _dds_complete_single_arg specifiers.
#    This will consume all remaining commandline arguments to follow the specified schema.
_dds_complete_positional_arg()
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

    _dds_complete_single_arg
    return
  fi

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"
} &&

# Evaluates the completion for a flag argument
# If ARG is an empty string, assumes the flag takes no arguments.
# ARG can also be any of what is specified in _dds_complete_single_arg
_dds_complete_flags()
{
  local ARG
  ARG="${FLAGS[$CWORD]}"
  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ -n "$ARG" ]]; then
    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      _dds_complete_single_arg
    else
      ((CWORDI=CWORDI+1))
      CWORD="${COMP_WORDS[$CWORDI]}"
    fi
  fi
} &&

# General completion function.
# Inputs:
#  - SUBCOMMANDS - Associative Array: SubcommandString -> Function
#  - FLAGS - Associative Array: FlagString -> ARG specifier (see _dds_complete_flags)
#  - POSITIONAL - Array: List of POS_ARG specifiers (see _dds_complete_positional_arg)
# Only one of SUBCOMMANDS or POSITIONAL can contain elements at the same time
# Output: COMPREPLY
_dds_complete_command()
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
      _dds_complete_flags
      # After parsing a flag, we should behave as if we are still parsing the command:
      _dds_complete_command
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
          _dds_complete_flags
          CONTINUE=1
        else
          _dds_complete_positional_arg
          CONTINUE=0 # break; we need to advance POS_ARG
        fi
      done

      if [[ $CONTINUE -ne 0 ]]; then
        # This means that $CWORDI -ge $COMP_CWORD,
        # i.e. that we have reached the completing argument.

        # Fill out COMPREPLY with the completions for this POS_ARG
        _dds_complete_positional_arg

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
    _dds_complete_command
  fi
} &&

# Computes the dds builtin toolchains
# Output: BUILTIN_TOOLCHAINS
_dds_complete_get_builtin_toolchains()
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

# dds build
_dds_complete_build()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _dds_complete_get_builtin_toolchains

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

  _dds_complete_command
} &&

# dds compile-file
_dds_complete_compile_file()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _dds_complete_get_builtin_toolchains

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

# dds build-deps
_dds_complete_build_deps()
{
  local BUILTIN_TOOLCHAINS RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  _dds_complete_get_builtin_toolchains

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

  _dds_complete_command
} &&

# dds pkg create
_dds_complete_pkg_create()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--project]='directory'
    [--out]=' '
    [--if-exists]='replace skip fail'
  )
  POSITIONAL=()

  _dds_complete_command
} &&

# dds pkg get
_dds_complete_pkg_get()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--output]='directory'
  )
  POSITIONAL=(
    # <pkg-id> ... # No completion implemented
  )

  _dds_complete_command
} &&

# dds pkg import
_dds_complete_pkg_import()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--stdin]=''
    [--if-exists]='replace skip fail'
  )
  POSITIONAL=(
    'repeat:file' # <path-or-url> ...
  )

  _dds_complete_command
} &&

# dds pkg repo add
_dds_complete_pkg_repo_add()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--no-update]=''
  )
  POSITIONAL=(
    # <url> # No completion implemented
  )

  _dds_complete_command
} &&

# dds pkg repo remove
_dds_complete_pkg_repo_remove()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--if-missing]='fail ignore'
  )
  POSITIONAL=(
    # <repo-name> # No completion implemented
  )

  _dds_complete_command
} &&

# dds pkg repo
_dds_complete_pkg_repo()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=(
    [add]=_dds_complete_pkg_repo_add
    [remove]=_dds_complete_pkg_repo_remove
    [update]=:
    [ls]=:
  )
  FLAGS=()
  POSITIONAL=()

  _dds_complete_command
} &&

# dds pkg
_dds_complete_pkg()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=(
    [init-db]=:
    [ls]=:
    [create]=_dds_complete_pkg_create
    [get]=_dds_complete_pkg_get
    [import]=_dds_complete_pkg_import
    [repo]=_dds_complete_pkg_repo
    [search]=:
  )
  FLAGS=()
  POSITIONAL=()

  _dds_complete_command
} &&

# dds repoman init
_dds_complete_repoman_init()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--if-exists]='replace skip fail'
    [--name]=' '
  )
  POSITIONAL=(
    'directory' # <repo-dir>
  )

  _dds_complete_command
} &&

# dds repoman ls
_dds_complete_repoman_ls()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    'directory' # <repo-dir>
  )

  _dds_complete_command
} &&

# dds repoman add
_dds_complete_repoman_add()
{
  local RESULT_WORDS POSITIONAL
  declare -A SUBCOMMANDS FLAGS

  SUBCOMMANDS=()
  FLAGS=(
    [--description]=' '
  )
  POSITIONAL=(
    'directory' # <repo-dir>
    # <url> # No completion implemented
  )

  _dds_complete_command
} &&

# dds repoman import
_dds_complete_repoman_import()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    'directory' # <repo-dir>
    'repeat:file' # <sdist-file-path> ...
  )

  _dds_complete_command
} &&

# dds repoman remove
_dds_complete_repoman_remove()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=()
  FLAGS=()
  POSITIONAL=(
    'directory' # <repo-dir>
    # <pkg-id> ... # No completion implemented
  )

  _dds_complete_command
} &&

# dds repoman
_dds_complete_repoman()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  local RESULT_WORDS
  SUBCOMMANDS=(
    [init]=_dds_complete_repoman_init
    [ls]=_dds_complete_repoman_ls
    [add]=_dds_complete_repoman_add
    [import]=_dds_complete_repoman_import
    [remove]=_dds_complete_repoman_remove
  )
  FLAGS=()
  POSITIONAL=()

  _dds_complete_command
} &&

# dds install-yourself
_dds_complete_install_yourself()
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

  _dds_complete_command
} &&

# dds
_dds_complete_impl()
{
  local POSITIONAL
  declare -A SUBCOMMANDS FLAGS
  SUBCOMMANDS=(
    [build]=_dds_complete_build
    [compile-file]=_dds_complete_compile_file
    [build-deps]=_dds_complete_build_deps
    [pkg]=_dds_complete_pkg
    [repoman]=_dds_complete_repoman
    [install-yourself]=_dds_complete_install_yourself
  )
  FLAGS=(
    [--log-level]='trace debug info warn error critical silent'
    [--data-dir]='directory'
    [--pkg-cache-dir]='directory'
    [--pkg-db-path]='file'
  )
  POSITIONAL=()

  _dds_complete_command
} &&

_dds_complete()
{
  local CWORDI CWORD
  COMPREPLY=()
  CWORDI=1
  CWORD="${COMP_WORDS[$CWORDI]}"
  _dds_complete_impl
  return 0
} &&
complete -F _dds_complete dds
