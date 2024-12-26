#define NOB_IMPLEMENTATION
#include "nob.h"

#define DEFAULT_CC "gcc"

#define STR_OR_DEFAULT(str, def) ((str) ? (str) : (def))

int main(int argc, char* argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    nob_mkdir_if_not_exists("out");

    char* CC = getenv("CC");
    nob_cmd_append(&cmd, STR_OR_DEFAULT(CC, DEFAULT_CC));
    nob_cmd_append(&cmd, "-O3");
    nob_cmd_append(&cmd, "-o", "out/main");
    nob_cmd_append(&cmd, "src/main.c", "src/crawler.c");
    nob_cmd_append(&cmd, "-lcurl", "-ljson-c", "-lpthread");

    nob_cmd_run_sync(cmd);

    return 0;
}
