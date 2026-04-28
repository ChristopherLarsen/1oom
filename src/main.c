#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "gameapi.h"
#include "hw.h"
#include "lbx.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "os.h"
#include "pbx.h"
#include "ui.h"
#include "version.h"

/* -------------------------------------------------------------------------- */

static bool main_startup_ok = false;
const char *main_replay_file = NULL;

/* -------------------------------------------------------------------------- */

static int main_early_init(void)
{
    if (0
      || os_early_init()
      || hw_early_init()
      || ui_early_init()
      || lbxfile_init()
    ) {
        return 1;
    }
    return 0;
}

static int main_init(void)
{
    if (0
      || os_init()
      || hw_init()
      || ui_init()
    ) {
        return 1;
    }
    return 0;
}

static void main_shutdown(void)
{
    options_shutdown(main_startup_ok);
    main_do_shutdown();
    lbxfile_shutdown();
    ui_shutdown();
    hw_shutdown();
    os_shutdown();
    log_trace_close();
    log_file_close();
    log_message("Thanks for playing 1oom.\n");
}

/* -------------------------------------------------------------------------- */

static char *main_replay_extract_load(const char *replay_filename)
{
    FILE *fd;
    char line[1024];
    char *load_filename = NULL;
    bool header_seen = false;
    int line_no = 0;

    fd = fopen(replay_filename, "r");
    if (fd == NULL) {
        log_error("Replay: could not open '%s'\n", replay_filename);
        return NULL;
    }

    while (fgets(line, sizeof(line), fd) != NULL) {
        char *p;
        char *op;
        char *arg;
        ++line_no;
        if (strchr(line, '\n') == NULL && !feof(fd)) {
            log_error("Replay: %s:%i line is too long\n", replay_filename, line_no);
            lib_free(load_filename);
            fclose(fd);
            return NULL;
        }
        line[strcspn(line, "\r\n")] = '\0';
        p = line;
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }
        if (*p == '\0') {
            continue;
        }
        if (*p == '#') {
            if (strcmp(p, "# 1oom-godot-replay v1") == 0) {
                header_seen = true;
            }
            continue;
        }
        {
            char *comment = strchr(p, '#');
            if (comment != NULL) {
                *comment = '\0';
            }
        }
        op = strtok(p, " \t");
        if (op == NULL) {
            continue;
        }
        arg = strtok(NULL, " \t");
        if (strcmp(op, "LOAD") == 0) {
            if (load_filename != NULL) {
                log_error("Replay: %s:%i duplicate LOAD\n", replay_filename, line_no);
                lib_free(load_filename);
                fclose(fd);
                return NULL;
            }
            if (arg == NULL) {
                log_error("Replay: %s:%i LOAD is missing the fixture path\n", replay_filename, line_no);
                fclose(fd);
                return NULL;
            }
            if ((arg[0] == '"') && (arg[strlen(arg) - 1] == '"')) {
                arg[strlen(arg) - 1] = '\0';
                ++arg;
            }
            load_filename = lib_stralloc(arg);
        }
    }

    fclose(fd);
    if (!header_seen) {
        log_error("Replay: %s: missing supported header comment: # 1oom-godot-replay v1\n", replay_filename);
        lib_free(load_filename);
        return NULL;
    }
    if (load_filename == NULL) {
        log_error("Replay: %s: missing LOAD operation\n", replay_filename);
    }
    return load_filename;
}

static int main_filter_trace_args(int argc, char **argv, int *argc_out, char ***argv_out, const char **trace_filename_out)
{
    char **argv_filtered;
    int argc_filtered = 0;
    char *replay_load_filename = NULL;

    argv_filtered = malloc(sizeof(*argv_filtered) * (argc + 3));
    if (argv_filtered == NULL) {
        log_error("Trace: could not allocate argv filter\n");
        return -1;
    }

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-trace") == 0) {
            if ((i + 1) >= argc) {
                log_error("option '-trace' is missing the parameter\n");
                free(argv_filtered);
                return -1;
            }
            *trace_filename_out = argv[i + 1];
            ++i;
            continue;
        }
        if (strcmp(argv[i], "-replay") == 0) {
            if ((i + 1) >= argc) {
                log_error("option '-replay' is missing the parameter\n");
                free(argv_filtered);
                return -1;
            }
            if (main_replay_file != NULL) {
                log_error("option '-replay' may appear only once\n");
                lib_free(replay_load_filename);
                free(argv_filtered);
                return -1;
            }
            main_replay_file = argv[i + 1];
            replay_load_filename = main_replay_extract_load(main_replay_file);
            if (replay_load_filename == NULL) {
                free(argv_filtered);
                return -1;
            }
            ++i;
            continue;
        }
        argv_filtered[argc_filtered++] = argv[i];
    }
    if (replay_load_filename != NULL) {
        argv_filtered[argc_filtered++] = "-load";
        argv_filtered[argc_filtered++] = replay_load_filename;
    }
    argv_filtered[argc_filtered] = NULL;
    *argc_out = argc_filtered;
    *argv_out = argv_filtered;
    return 0;
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    int argc_filtered = argc;
    char **argv_filtered = NULL;
    const char *trace_filename = NULL;
    int ret;

    if (main_early_init()) {
        return 1;
    }
    if (main_filter_trace_args(argc, argv, &argc_filtered, &argv_filtered, &trace_filename)) {
        return 1;
    }
    if (trace_filename && log_trace_open(trace_filename)) {
        free(argv_filtered);
        return 1;
    }
    if (options_parse_early(argc_filtered, argv_filtered)) {
        log_trace_close();
        free(argv_filtered);
        return 1;
    }
    log_message(PACKAGE_NAME " " VERSION_STR ": main:%s ui:%s hw:%s os:%s\n", idstr_main, idstr_ui, idstr_hw, idstr_os);
    atexit(main_shutdown);
    if (main_init()) {
        free(argv_filtered);
        return 2;
    }
    if (options_parse(argc_filtered, argv_filtered)) {
        free(argv_filtered);
        return 3;
    }
    if (lbxfile_find_dir()) {
        ui_early_show_message_box("Error: could not find\nthe LBX files!\n"
                            "1oom requires a copy\nof the Master of Orion\n(v1.3) LBX files.\n"
                            "Simply copy the EXE and\nDLLs to your MOO1\ndirectory.\n"
                            "Alternatively copy your\nMOO1 LBX files to your\n1oom directory.");
        free(argv_filtered);
        return 4;
    }
    game_apply_rules();
    if (pbx_apply_queued_files()) {
        free(argv_filtered);
        return 5;
    }

    log_message("USER: '%s'\n", os_get_path_user());
    main_startup_ok = true;
    options_finish();
    ret = main_do();
    free(argv_filtered);
    return ret;
}
