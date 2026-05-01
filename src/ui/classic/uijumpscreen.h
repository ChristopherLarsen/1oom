#ifndef INC_1OOM_UI_CLASSIC_JUMPSCREEN_H
#define INC_1OOM_UI_CLASSIC_JUMPSCREEN_H

/* DRG agent-orion patch: -jumpscreen dispatch.

   ui_jumpscreen_dispatch parses spec ("NAME" or "NAME:k=v,k=v"), calls the
   named UI function with resolved args, then returns a non-zero exit code
   on failure (unknown screen / missing required arg state / save lacks
   required entity). Returns 0 if the screen function returned cleanly OR if
   the autoshot patch already exited the process from inside it.
*/

struct game_s;
extern int ui_jumpscreen_dispatch(struct game_s *g, const char *spec);

#endif
