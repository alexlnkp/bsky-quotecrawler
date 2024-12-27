#define NOB_IMPLEMENTATION
#include "nob.h"

#define JSONC_REPO "https://github.com/json-c/json-c.git"

#define VENDOR_DIR "vendortest"
#define DEFAULT_CC "gcc"

#define STR_OR_DEFAULT(str, def) ((str) ? (str) : (def))

char jsonc_build[128];
char vendir_jsonc[128];

void grab_prereq(void) {
    nob_mkdir_if_not_exists(VENDOR_DIR);

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "git", "clone", JSONC_REPO, vendir_jsonc);
    nob_cmd_run_sync_and_reset(&cmd);

    nob_cmd_append(&cmd, "git", "-C", vendir_jsonc, "checkout", "ee83daa4093dfbc4bfd63468dcf2bd7c636d7a0e");
    nob_cmd_run_sync_and_reset(&cmd);

    nob_cmd_append(&cmd, "emcmake", "cmake", vendir_jsonc,
                         "-B", jsonc_build, "-DENABLE_THREADING=OFF",
                         "-DBUILD_APPS=OFF", "-DCMAKE_BUILD_TYPE=Release");
    nob_cmd_run_sync_and_reset(&cmd);

    nob_cmd_append(&cmd, "cmake", "--build", jsonc_build, "--config", "Release");
    nob_cmd_run_sync_and_reset(&cmd);
}

void build_default(void) {
    Nob_Cmd cmd = {0};

    char* CC = getenv("CC");
    nob_cmd_append(&cmd, STR_OR_DEFAULT(CC, DEFAULT_CC));
    nob_cmd_append(&cmd, "-O3");
    nob_cmd_append(&cmd, "-o", "out/main");
    nob_cmd_append(&cmd, "src/main.c", "src/crawler.c");
    nob_cmd_append(&cmd, "-lcurl", "-ljson-c", "-lpthread");

    nob_cmd_run_sync(cmd);
}

void build_ems(void) {
    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "emcc");
    nob_cmd_append(&cmd, "ems/main_ems.c", "ems/ems_crawler.c");
    nob_cmd_append(&cmd, "-I/usr/lib/emsdk/upstream/emscripten/cache/sysroot/include/", "-I", jsonc_build, "-I", vendir_jsonc);
    nob_cmd_append(&cmd, "-sFETCH");
    nob_cmd_append(&cmd, "-o", "out/out.html");
    nob_cmd_append(&cmd, "-L", jsonc_build, "-l:libjson-c.a");

    nob_cmd_run_sync(cmd);
}

int main(int argc, char* argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    sprintf(vendir_jsonc, "%s/json-c", VENDOR_DIR);
    sprintf(jsonc_build, "%s/build", vendir_jsonc);

    if (!nob_file_exists(VENDOR_DIR)) grab_prereq();

    nob_mkdir_if_not_exists("out");

    char* buildtype = STR_OR_DEFAULT(getenv("BUILDTYPE"), "default");

    if (strcmp(buildtype, "ems") == 0) {
        build_ems();
    } else {
        build_default();
    }

    return 0;
}
