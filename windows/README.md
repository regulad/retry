# retry for Windows

This fork adds a Windows build of [minfrin/retry](https://github.com/minfrin/retry)
and nothing else: upstream's tree and workflow are untouched, so the fork
follows upstream with plain merges. The build is published as a release here
and installed through the `retry` manifest in
[regulad/scoop-regulad](https://github.com/regulad/scoop-regulad):

```
scoop bucket add regulad https://github.com/regulad/scoop-regulad.git
scoop install retry
```

## How it is built

`retry.c` uses `fork`, `execvp`, `pipe` and `poll`, so it cannot be built
natively with MinGW or MSVC. It is built against the MSYS2 runtime instead,
from upstream's release tarball, and ships with the `msys-2.0.dll` it was
linked against. `.github/workflows/windows.yml` does the build, a smoke test
from native shells, and the release.

## What `win32-compat.c` changes

Linked in beside `retry.c` with `-Wl,--wrap=execvp`. Two differences between
an MSYS2 program and a native one would otherwise break the common case of
retrying a native Windows command:

- **Argument conversion.** The MSYS2 runtime rewrites POSIX-looking arguments
  when it starts a native program, so `retry -- cmd /c exit 1` would hand
  cmd.exe a Windows path in place of `/c`. `MSYS2_ARG_CONV_EXCL=*` is set at
  startup (unless already set), so children get their arguments verbatim.
  They inherit the variable like any other.
- **Command lookup.** `execvp` only tries `NAME` and `NAME.exe`, so
  `retry -- code --version` fails: `code` is `code.cmd`, as are npm's globals
  and scoop's shims for PowerShell apps. A bare name is looked up the way
  cmd.exe does it, trying `.com`, `.exe`, `.bat` and `.cmd` in each `PATH`
  directory before the next.

Everything else -- options, delays, `--until`/`--while`, stdin replay, exit
status -- is upstream's behaviour.

## Licenses

retry is Apache-2.0 (`COPYING`). `msys-2.0.dll` is the MSYS2 runtime, a
Cygwin derivative under the GPL with Cygwin's linking exception;
`SOURCES.txt` in each release zip names the exact runtime package and where
its source is.
