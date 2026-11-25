# mkn.yaml

## Basic project files

### Binary

```yaml
name: exe_project
main: cpp.cpp
```

### Library

```yaml
name: lib_project
src: ./src
```

## Schema

Values which allow property resolution are marked with `${aaa}`

```yaml
name: name
version: version                    # considered git branch for pulling
scm: string                         # Appended to remote URLs for lookup if non concrete URL, used if concrete
super: $directory                   # Inherit properties of project at directory, super (..) or sub directory advisable
parent: $directory                  # See profile inheritance below
property:
  - aaa: AAA
    bbb: ${aaa}.BBB
inc: <directory> <directory> ${aaa}
src: <directory> <file> ${aaa}
path: <directory> ${aaa}            # extra library paths for linking, generally inadvisable unless project is in sub directory
lib: <library> ${aaa}               # prefixes/file types automatically deduced e.g. "lib: math" = libmath.a/so math.lib/dll
dep:                                # also supports "with" syntax
  - name: name                      # Used in URL lookup if ${scm} is missing
    version: version                # git branch and directory name of dependency under $name if no local tag
    scm: ${aaa}                     # Appended to remote URLs for lookup if non concrete URL, used if concrete
    local: ${aaa}                   # overrides local repository directory
    profile: ${aaa}                 # choose profile from config
with:
  xxx.yyy[profile]                  # The same as "-w" (See dependencies.md)
 if_dep:                            # optional deps for OS valid values [ bsd / nix / win ]
    win:
      - name: name                  # Used in URL lookup if ${scm} is missing
        version: version            # git branch and directory name of dependency under $name if no local tag
        scm: ${aaa}                 # Appended to remote URLs for lookup if non concrete URL, used if concrete
        local: ${aaa}               # overrides local repository directory
        profile: ${aaa}             # choose profile from config
main: <file>
arg: -DTHIS_PROFILE ${aaa}          # Optional additional compiler arguments
link: ${aaa}                        # Optional additional linker arguments, applied to end
pub_arg: -DTHIS_PROFILE             # Optional compiler arguments for this project and every project
                                    #  depending on it, passed on as public includes are. A map is
                                    #  keyed as if_arg, e.g. "win_static: -DYAML_CPP_STATIC_DEFINE"
if_arg:                             # optional arguments if case is true
    shared: ${aaa}                  # Add arguments to compilation if mode is shared
    static: ${aaa}                  # Add arguments to compilation if mode is static
    bin: ${aaa}                     # Add arguments to compilation if linking is binary
    lib: ${aaa}                     # Add arguments to compilation if linking is library
    bsd: ${aaa}
    bsd_bin: ${aaa}
    bsd_lib: ${aaa}
    bsd_shared: ${aaa}
    bsd_static: ${aaa}
    nix: ${aaa}
    nix_bin: ${aaa}
    nix_lib: ${aaa}
    nix_shared: ${aaa}
    nix_static: ${aaa}
    win: ${aaa}
    win_bin: ${aaa}
    win_lib: ${aaa}
    win_shared: ${aaa}
    win_static: ${aaa}
if_inc:                         # optional includes if case is true
    bsd: ${aaa}
    nix: ${aaa}
    win: ${aaa}
if_src:                         # optional sources if case is true
    bsd: <directory/file> ${aaa}
    nix: <directory/file> ${aaa}
    win: <directory/file> ${aaa}
if_lib:                         # optional libraries if case is true
    bsd: ${aaa}
    nix: ${aaa}
    win: ${aaa}
env:
  - name: name
    mode: prepend/append/replace    # optional default is prepend
    value: ${aaa}
profile:
  - name: a
    inc: |
        <directory>, false/no/0     # Don't expose if dependency / private include
        <directory>, true/yes/1     # Include if dependency / public include
        <directory> <directory>     # Include both if dependency / public includes
    src: |
        <directory>, false/no/0     # Don't find sources recursively
        <directory>, true/yes/1     # Find/add sources recursively
        <directory> <file>          # Find/add sources recursively, add file
        <directory>, 1, -DFlags     # Find/add sources recursively, add -DFlags to file compile commands
        <file>, -DFlagsForThisFile  # Add <file> and pass -DFlagsForThisFile to compile command
    dep:
      - name: name
        version: version
        local: .                    # Dependency on another profile in same project allowed, cycles detected
  - name: bbb
    parent: ${aaa}
  - name: bsd
    parent: a
  - name: nix
    parent: a
  - name: win
    parent: a

  - name: test
    self: ${OS} ${aaa}              # dependencies on current project profiles, cycles detected
    sub: project                    # subprojects, follows "with" rules (see dependencies.md) for resolution, requires URL
    main: test.cpp
    test: <file/regex> ${aaa}       # each match is built into its own binary under ./bin/<profile>/test
                                    # by "build" ("link"), run by the "test" command, space or line separated
    out: ${aaa}                     # override default binary/library name
    install: ${aaa}                 # install binary or library to directory specified
                                    # library naming convention is libname_profile.a/so if linux, name_profile.lib if windows
```

## Caveats

### Type deduction

- If no main tag is found, library linking is assumed
- If no lang tag is found, language is deduced from first main tag found in file,
  else linker inferred from max(file type)
- If the project is a dependency, even if a main tag is present, library linking is assumed.

### Mode

The mode tag may have three values: static/shared/none

- If the mode tag is found, it takes precedence for linking
- If no mode tag is found, the `-K` and `-S` args will be used if found.
- Linking cannot be guaranteed if no mode is used, but shared is generally default.

Unless a project requires a mode, it's advised to avoid using one. So it can be overridden with `-K` or `-S`.

If an application includes both static and shared dependencies, using the mode "none" is advised.

### Entry points

All entry points must be referenced by at least one main tag, otherwise linking cannot be guaranteed.

```yaml
profile:
  - name: a
    main: a.cpp
  - name: b
    main: b.cpp
```

### Compiler deduction

Compiler binaries are expected to have their default names e.g. gcc/gcc.exe/cl.exe. This only applies to compilers and not archivers or linkers. Compiler masking is provided to use alternate compiler binaries with the same functionality as the underlying compiler.

Masking [settings.yaml](settings.md) example

```yaml
compiler:
  mask:
    g++:
      mpicxx
```

Each value under "g++:" is considered to be a filesystem binary which supports all flags which are supported by "g++".

## Properties

System properties: maiken's own `mkn.sys.*` and `mkn.user.*`, which nothing can set (a
`property:`, `-P` or `-X` naming one is an error), and those a project or settings file can
override, defaulting to them:

| Property    | Default | Value |
|-------------|---------|-------|
| `OS`        | `mkn.sys.os` | system OS, can be either bsd/nix/win |
| `ARCH`      | `mkn.sys.arch` | system architecture, `MKN_ARCH` from the environment, else x86_64 |
| `HOME`      | `mkn.user.home` | `HOME` from the environment, else on Windows `%HOMEDRIVE%%HOMEPATH%` |
| `MKN_HOME`  | `mkn.user.maiken` | maiken's home directory: `HOME/.maiken`, on Windows `HOME/maiken` |
| `MKN_REPO`  | `mkn.user.repo` | the dependency repository: the first `local: repo:` of the settings files, else `MKN_HOME/repo` |
| `MKN_MOD_REPO` | `mkn.user.mod_repo` | the module repository: the first `local: mod-repo:`, else `MKN_HOME/mod-repo` |
| `DATETIME`  | | DateTime when app launched format `%Y-%m-%d-%H:%M:%S` |
| `TIMESTAMP` | | Unix timestamp when app launched in seconds |
| `MKN_ROOT`  | | Directory of current mkn.yaml file |
| `mkn.bin.key`, `mkn.with.bin` | | prebuilt binaries, settings files' only, see [settings](settings.md) |

`mkn.user.repo` and `mkn.user.mod_repo` are the defaults, not where `local:` points: a property
named `MKN_REPO` changes what `${MKN_REPO}` gives, `local: repo:` where dependencies are.

A project sees `-P name=value`, then its own `property:` (with its `super:` project's and the
settings files'), then the system property. `-P` is how one build overrides a project's value;
a settings file's is `-X`, see [settings.yaml](settings.md#a-property-for-one-build). A
project's `mkn.bin.key` or `mkn.with.bin` is reported once and not used (`WARNING: ... is a
settings file property, its value here is not used`).

A project's property using its own name, `a: pre.${a}`, extends the value `a` has in its
`super:` project, the settings files or the system properties, and is an error if none has one.

System properties never end in `.version`: a project names its dependencies' versions
`<dependency>.version` (e.g. `parse.yaml.version: 0.8.0`, used as `parse.yaml#${parse.yaml.version}`)
without meeting one, and `-P parse.yaml.version=0.8.1` tries another for one build.

## Profile dependencies

Adding a profile as a dependency can do a number of things:

- Import additional public includes
- Add libraries while linking.
  - This can happen when a dependency profile has sources so is linked into a library, or
  - This can happen when a dependency has the "lib" tag
- Import additional library search directories with the "path" tag

## Profile inheritance

Using the "parent" tag, one profile may derive a number of values from the selected profile.
Generally all values are inherited, however even if this is the case, some values such as
the "main" tag or others with only a single value are overridden in the case they exist in the active profile.

## SCM

Git supported, SVN planned.
