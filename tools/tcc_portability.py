"""Small, verified TinyCC macOS portability adaptation (see the distributed patch)."""
OLD = '\tchar command[1024];\n\tint retval;\n\n\tsnprintf(command, sizeof(command), "codesign -f -s - %s", filename);\n\tretval = system (command);\n\tif (retval == -1 || !(WIFEXITED(retval) && WEXITSTATUS(retval) == 0))\n\t    tcc_error ("command failed \'%s\'", command);'
NEW = '        int status = 0;\n        pid_t child = fork(), waited;\n        if (child == 0) {\n            execl("/usr/bin/codesign", "codesign", "-f", "-s", "-", filename, (char *)NULL);\n            _exit(127);\n        }\n        if (child < 0) tcc_error("cannot start codesign");\n        do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);\n        if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)\n            tcc_error("codesign failed for \'%s\'", filename);'


def patch_codesign(text):
    if text.count(OLD) != 1:
        raise ValueError('TinyCC codesign patch context changed')
    return text.replace('#include "tcc.h"', '#include "tcc.h"\n#ifdef CONFIG_CODESIGN\n#include <sys/wait.h>\n#endif', 1).replace(OLD, NEW, 1)
