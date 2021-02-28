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

_dds_complete_command()
{
  local RESULT_WORDS SUBCOMMAND FLAG
  if [[ ${#POSITIONAL[@]} -eq 0 ]]; then
    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      RESULT_WORDS="${!SUBCOMMANDS[@]}"

      if [[ $CWORD == "-"* || -z $RESULT_WORDS ]]; then
        RESULT_WORDS+=" ${!FLAGS[@]}"
      fi

      COMPREPLY+=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
      return
    fi

    SUBCOMMAND="${SUBCOMMANDS[$CWORD]}"
    FLAG="${FLAGS[$CWORD]}"
    if [[ -n $FLAG || $CWORD == "-"* ]]; then
      _dds_complete_flags
      _dds_complete_command
    elif [[ -n $SUBCOMMAND ]]; then
      ((CWORDI=CWORDI+1))
      CWORD="${COMP_WORDS[$CWORDI]}"
      $SUBCOMMAND
    fi
  else
    local POS_ARG
    for POS_ARG in "${POSITIONAL[@]}"; do
      while [[ $CWORDI -lt $COMP_CWORD ]]; do
        FLAG="${FLAGS[$CWORD]}"
        if [[ -n $FLAG || $CWORD == "-"* ]]; then
          _dds_complete_flags
        else
          _dds_complete_positional_arg
        fi
      done

      _dds_complete_positional_arg

      if [[ $CWORD == "-"* || -z "${COMPREPLY[@]}" ]]; then
        RESULT_WORDS=" ${!FLAGS[@]}"
        COMPREPLY+=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
      fi

      return
    done

    local POSITIONAL
    POSITIONAL=()
    _dds_complete_command
  fi
} &&

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
