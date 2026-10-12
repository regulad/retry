/*
 * Windows behaviour for retry built against the MSYS2 runtime.
 *
 * retry.c is upstream's, unmodified. This file is linked in alongside it with
 * -Wl,--wrap=execvp, and fixes the two ways an MSYS2 program differs from a
 * native one when it starts a native Windows program -- which, on Windows, is
 * nearly everything retry is ever asked to run.
 *
 * Licensed under the Apache License, Version 2.0, like retry itself.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * 1. Argument conversion. When an MSYS2 program starts a native one, the
 *    runtime rewrites every argument that looks like a POSIX path: `cmd /c
 *    exit 1` reaches cmd.exe with /c turned into a Windows path, and cmd runs
 *    something else entirely. MSYS2_ARG_CONV_EXCL=* turns that off. Set before
 *    main, and only when the caller has not chosen a value of their own; the
 *    children inherit it, as they would any other environment variable.
 */
__attribute__((constructor))
static void retry_no_arg_conversion(void)
{
    setenv("MSYS2_ARG_CONV_EXCL", "*", 0);
}

/*
 * 2. Command lookup. execvp tries NAME and NAME.exe in each PATH directory and
 *    nothing else, so `retry -- code --version` fails with ENOENT: code is
 *    code.cmd, as are npm's globals and scoop's shims for PowerShell apps.
 *    cmd.exe instead tries the PATHEXT extensions in each directory before
 *    moving on to the next, so do the same for a bare name, with the
 *    extensions the runtime can start (.cmd and .bat run through cmd.exe).
 *    A name with a directory or an extension in it goes to execvp unchanged.
 */
int __real_execvp(const char *file, char *const argv[]);

static const char *const pathext[] = { ".com", ".exe", ".bat", ".cmd", NULL };

int __wrap_execvp(const char *file, char *const argv[])
{
    const char *path = getenv("PATH");
    const char *dir, *end;
    char candidate[PATH_MAX];
    struct stat st;
    size_t len;
    int i;

    if (!*file || strpbrk(file, "/\\.") || !path) {
        return __real_execvp(file, argv);
    }

    for (dir = path; ; dir = end + 1) {
        end = strchr(dir, ':');
        len = end ? (size_t)(end - dir) : strlen(dir);
        for (i = 0; pathext[i]; i++) {
            if (snprintf(candidate, sizeof(candidate), "%.*s%s%s%s", (int)len,
                    dir, len ? "/" : "", file, pathext[i])
                    >= (int)sizeof(candidate)) {
                continue;
            }
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) {
                execv(candidate, argv);
                /* found but would not start: report that, not ENOENT */
                return -1;
            }
        }
        if (!end) {
            break;
        }
    }

    return __real_execvp(file, argv);
}
