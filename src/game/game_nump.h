#ifndef INC_1OOM_GAME_NUMP_H
#define INC_1OOM_GAME_NUMP_H

#include "types.h"

extern bool game_opt_fix_starting_ships;

extern void game_num_apply_canonical_bugfixes(void);
extern void game_num_apply_1_3a_rules(void);
extern void game_num_fix_starting_ships(void);
extern void game_num_dump(void);

#endif
