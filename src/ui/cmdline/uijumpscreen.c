#include "config.h"

#include "ui.h"
#include "log.h"

/* DRG agent-orion patch: cmdline UI cannot render screens; -jumpscreen is
   a no-op that fails loudly. Provided so libuicmdline links against game.c. */
int ui_jumpscreen_dispatch(struct game_s *g, const char *spec)
{
    (void)g;
    log_error("Jumpscreen: cmdline UI cannot dispatch screens (spec='%s')\n", spec ? spec : "");
    return 1;
}
