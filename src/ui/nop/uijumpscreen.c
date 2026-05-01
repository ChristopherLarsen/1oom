#include "config.h"

#include "ui.h"
#include "log.h"

/* DRG agent-orion patch: nop UI stub for ui_jumpscreen_dispatch. */
int ui_jumpscreen_dispatch(struct game_s *g, const char *spec)
{
    (void)g;
    log_error("Jumpscreen: nop UI cannot dispatch screens (spec='%s')\n", spec ? spec : "");
    return 1;
}
