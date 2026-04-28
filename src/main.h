#ifndef INC_1OOM_MAIN_H
#define INC_1OOM_MAIN_H

#include "options.h"
#include "types.h"

extern bool main_use_lbx;
extern bool main_use_cfg;
extern const char *idstr_main;
extern const struct cmdline_options_s main_cmdline_options[];
extern const struct cmdline_options_s main_cmdline_options_early[];
extern const char *main_replay_file;
extern const char *main_autoshot_dir;
extern const char *main_shotkey_name;
extern int main_replay_current_year;
extern int main_handle_option(const char *argv);
extern void (*main_usage)(void);

extern int main_1oom(int argc, char **argv);
extern int main_do(void);
extern void main_do_shutdown(void);

#endif
