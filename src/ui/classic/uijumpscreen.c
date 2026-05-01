/* DRG agent-orion patch: -jumpscreen dispatch.

   Phase 1a: empire_status only. Smoke test before expanding the table.

   Architecture:
     - ui_jumpscreen_dispatch is called from game.c right after ui_game_start,
       before the main turn loop.
     - It parses spec "NAME[:k=v,k=v,...]" and looks NAME up in the table.
     - For each entry it calls the UI function with resolved args.
     - The UI function runs its own internal event loop. Our MOO_AUTOSHOT_*
       patch in hwsdl2.c fires from inside that loop on the Nth tick and
       exits the process cleanly.
     - If the UI function returns (e.g. user dismisses normally, or the
       MOO_AUTOSHOT_EXIT=0 case), the dispatcher returns and the caller
       does an immediate clean shutdown (no main loop).

   Failure modes (all log_error + non-zero return):
     - unknown screen name
     - required save state absent (e.g. opponent=mrrshan but no Mrrshan contact)
     - malformed spec
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uijumpscreen.h"
#include "boolvec.h"
#include "game.h"
#include "game_audience.h"
#include "game_battle.h"
#include "game_election.h"
#include "game_ground.h"
#include "game_news.h"
#include "game_new.h"
#include "game_planet.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "game_tech.h"
#include "game_types.h"
#include "lib.h"
#include "log.h"
#include "util.h"
#include "uibasescrap.h"
#include "uibattlepre.h"
#include "uicaught.h"
#include "uidialog.h"
#include "uiempirereport.h"
#include "uiempirestatus.h"
#include "uifleet.h"
#include "uigameopts.h"
#include "uiload.h"
#include "uiplanets.h"
#include "uiraces.h"
#include "uisave.h"
#include "uispecs.h"
#include "uistarmap.h"
#include "uitech.h"
#include "uihelp.h"
#include "uinewgame.h"
#include "uinews.h"
#include "ui.h"  /* for ui_newtech, ui_newships, ui_play_intro,
                    ui_play_ending_*, ui_landing, ui_bomb_show,
                    ui_election_*, ui_ground */

/* -------------------------------------------------------------------------- */

typedef int (*ui_jumpscreen_fn_t)(struct game_s *g, const char *args);

struct ui_jumpscreen_entry_s {
    const char *name;
    ui_jumpscreen_fn_t fn;
    const char *desc;
};

/* -------------------------------------------------------------------------- */
/* Tier 2 helpers: resolve string args against the loaded save state. */

static int jumpscreen_strcasecmp(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return (int)ca - (int)cb;
        ++a; ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* Extract value of "key=" from a comma-separated args string into out_buf.
   Returns true if key found. Stops at ',' or end of string. */
static bool jumpscreen_arg_get(const char *args, const char *key,
                               char *out_buf, size_t out_sz)
{
    size_t klen;
    const char *p;
    if (args == NULL || key == NULL || out_buf == NULL || out_sz == 0) return false;
    klen = strlen(key);
    p = args;
    while (*p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            size_t i = 0;
            while (v[i] && v[i] != ',' && i + 1 < out_sz) {
                out_buf[i] = v[i];
                ++i;
            }
            out_buf[i] = '\0';
            return true;
        }
        /* advance to char after next ',' */
        while (*p && *p != ',') ++p;
        if (*p == ',') ++p;
    }
    return false;
}

/* Resolve race-name string to player_id_t for the loaded save.
   Returns the player id of the contacted opponent, or PLAYER_NONE on failure
   (unknown name OR not contacted by g->active_player OR same as active player).
   Logs the failure reason. */
static player_id_t jumpscreen_resolve_opponent(struct game_s *g, const char *name)
{
    player_id_t active = g->active_player;
    race_t want_race = RACE_NUM;
    int i;

    if (name == NULL || name[0] == '\0') {
        log_error("Jumpscreen: opponent name is empty\n");
        return PLAYER_NONE;
    }

    /* Match name against game_str_tbl_race[0..RACE_NUM-1] (singular forms). */
    for (i = 0; i < RACE_NUM; ++i) {
        if (jumpscreen_strcasecmp(name, game_str_tbl_race[i]) == 0) {
            want_race = (race_t)i;
            break;
        }
    }
    if (want_race == RACE_NUM) {
        log_error("Jumpscreen: unknown race name '%s'. Known races:", name);
        for (i = 0; i < RACE_NUM; ++i) {
            log_error(" %s", game_str_tbl_race[i]);
        }
        log_error("\n");
        return PLAYER_NONE;
    }

    /* Walk every player slot; find one with matching race that is contacted
       by active_player, alive, and not active_player itself. */
    for (i = 0; i < g->players; ++i) {
        if (i == active) continue;
        if (g->eto[i].race != want_race) continue;
        if (!IS_ALIVE(g, i)) {
            log_error("Jumpscreen: opponent '%s' (player %d) is not alive\n", name, i);
            return PLAYER_NONE;
        }
        if (BOOLVEC_IS0(g->eto[active].contact, i)) {
            log_error("Jumpscreen: opponent '%s' (player %d) has not been contacted by active player %d\n",
                      name, i, active);
            return PLAYER_NONE;
        }
        log_message("Jumpscreen: resolved opponent '%s' -> player %d\n", name, i);
        return (player_id_t)i;
    }

    log_error("Jumpscreen: no player with race '%s' in this save\n", name);
    return PLAYER_NONE;
}

/* -------------------------------------------------------------------------- */
/* Per-screen handlers. Each returns 0 on success, non-zero on arg failure.
   Side note: most callers will be killed by the autoshot patch from inside
   the UI function before the handler returns — that exit(0) IS the success
   path. The return value here is only for the "no autoshot, screen dismissed
   normally" case and for arg-resolution failures. */

static int jump_empire_status(struct game_s *g, const char *args)
{
    (void)args;
    log_message("Jumpscreen: empire_status active_player=%d\n", g->active_player);
    ui_empirestatus(g, g->active_player);
    return 0;
}

static int jump_newgame(struct game_s *g, const char *args)
{
    (void)g;
    (void)args;
    struct game_new_options_s opts = GAME_NEW_OPTS_DEFAULT;
    ui_new_game(&opts);
    return 0;
}

static int jump_starmap(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_do(g, g->active_player);
    return 0;
}

static int jump_empire_planets(struct game_s *g, const char *args)
{
    (void)args;
    ui_planets(g, g->active_player);
    return 0;
}

static int jump_fleet(struct game_s *g, const char *args)
{
    (void)args;
    ui_fleet(g, g->active_player);
    return 0;
}

static int jump_tech(struct game_s *g, const char *args)
{
    (void)args;
    ui_tech(g, g->active_player);
    return 0;
}

static int jump_specs(struct game_s *g, const char *args)
{
    /* args may be "cheat=1" to enable cheat-mode info; default false. */
    bool cheat = (args != NULL && strstr(args, "cheat=1") != NULL);
    ui_specs(g, g->active_player, cheat);
    return 0;
}

static int jump_races(struct game_s *g, const char *args)
{
    (void)args;
    ui_races(g, g->active_player);
    return 0;
}

static int jump_gameopts(struct game_s *g, const char *args)
{
    (void)args;
    int load_i = 0;
    ui_gameopts(g, &load_i);
    return 0;
}

static int jump_newtech_alert(struct game_s *g, const char *args)
{
    (void)args;
    ui_newtech(g, g->active_player);
    return 0;
}

static int jump_newships_alert(struct game_s *g, const char *args)
{
    (void)args;
    ui_newships(g, g->active_player);
    return 0;
}

static int jump_spy_caught(struct game_s *g, const char *args)
{
    (void)args;
    ui_caught(g, g->active_player);
    return 0;
}

static int jump_help(struct game_s *g, const char *args)
{
    /* args may be "index=N" to pick a specific help topic; default 0. */
    (void)g;
    int idx = 0;
    if (args != NULL) {
        const char *p = strstr(args, "index=");
        if (p != NULL) idx = (int)strtol(p + 6, NULL, 10);
    }
    ui_help(idx);
    return 0;
}

static int jump_intro(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    ui_play_intro();
    return 0;
}

/* Tier 2: requires opponent=NAME arg resolved against g->eto[].race */

static int jump_empire_report(struct game_s *g, const char *args)
{
    char namebuf[32];
    player_id_t opponi;
    if (!jumpscreen_arg_get(args, "opponent", namebuf, sizeof(namebuf))) {
        log_error("Jumpscreen: empire_report requires 'opponent=NAME' arg\n");
        return 2;
    }
    opponi = jumpscreen_resolve_opponent(g, namebuf);
    if (opponi == PLAYER_NONE) {
        return 2;
    }
    log_message("Jumpscreen: empire_report active=%d opponi=%d\n", g->active_player, opponi);
    ui_empirereport(g, g->active_player, opponi);
    return 0;
}

static int jump_audience(struct game_s *g, const char *args)
{
    char namebuf[32];
    player_id_t opponi;
    if (!jumpscreen_arg_get(args, "opponent", namebuf, sizeof(namebuf))) {
        log_error("Jumpscreen: audience requires 'opponent=NAME' arg\n");
        return 2;
    }
    opponi = jumpscreen_resolve_opponent(g, namebuf);
    if (opponi == PLAYER_NONE) {
        return 2;
    }
    log_message("Jumpscreen: audience active=%d opponi=%d\n", g->active_player, opponi);
    game_audience(g, g->active_player, opponi);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Tier 3: transient-state synthesis. These screens are normally only entered
   mid-action (battle, bombing run, election vote, ending cinematic). For
   capture we synthesize a minimal valid state from the loaded save.

   ANIMATIONS: All of these screens animate. We capture a SINGLE
   representative frame — the autoshot patch in hwsdl2.c calls exit(0) from
   inside the screen's own draw loop on the Nth tick. The port replaces all
   animations — never capture sequences. Tune MOO_AUTOSHOT_FRAMES per screen
   empirically. */

/* --- Endings: direct call, no state synth needed -------------------------- */

static int jump_ending_victory(struct game_s *g, const char *args)
{
    (void)args;
    log_message("Jumpscreen: ending_victory race=%d\n", g->eto[g->active_player].race);
    ui_play_ending_good(g->eto[g->active_player].race, "Mentar");
    return 0;
}

static int jump_ending_tyrant(struct game_s *g, const char *args)
{
    (void)args;
    log_message("Jumpscreen: ending_tyrant race=%d\n", g->eto[g->active_player].race);
    ui_play_ending_tyrant(g->eto[g->active_player].race, "Mentar");
    return 0;
}

static int jump_ending_funeral(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    log_message("Jumpscreen: ending_funeral\n");
    ui_play_ending_funeral(0, 0);
    return 0;
}

static int jump_ending_exile(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    log_message("Jumpscreen: ending_exile\n");
    ui_play_ending_exile("Mentar");
    return 0;
}

/* --- Helpers for state-synth screens -------------------------------------- */

/* Resolve own homeworld (or any owned planet) for active player. Returns
   PLANET_NONE if active player has no planet. */
static uint8_t jumpscreen_first_own_planet(struct game_s *g, player_id_t pi)
{
    for (int k = 0; k < g->galaxy_stars; ++k) {
        if (g->planet[k].owner == pi) {
            return (uint8_t)k;
        }
    }
    return PLANET_NONE;
}

/* Resolve any planet owned by an opponent of pi. Sets *out_owner to the
   owner. Returns PLANET_NONE if no such planet exists. */
static uint8_t jumpscreen_first_enemy_planet(struct game_s *g, player_id_t pi,
                                             player_id_t *out_owner)
{
    for (int k = 0; k < g->galaxy_stars; ++k) {
        player_id_t o = g->planet[k].owner;
        if (o != PLAYER_NONE && o != pi) {
            if (out_owner) *out_owner = o;
            return (uint8_t)k;
        }
    }
    return PLANET_NONE;
}

/* --- 5. Landing ----------------------------------------------------------- */

static int jump_landing(struct game_s *g, const char *args)
{
    (void)args;
    player_id_t pi = g->active_player;
    uint8_t planet_i = jumpscreen_first_own_planet(g, pi);
    if (planet_i == PLANET_NONE) {
        log_error("Jumpscreen: landing requires an owned planet, none found for player %d\n", pi);
        return 2;
    }
    log_message("Jumpscreen: landing pi=%d planet_i=%d\n", pi, planet_i);
    ui_landing(g, pi, planet_i);
    return 0;
}

/* --- 6. Bomb -------------------------------------------------------------- */

static int jump_bomb(struct game_s *g, const char *args)
{
    (void)args;
    player_id_t pi = g->active_player;
    player_id_t owner_i = PLAYER_NONE;
    uint8_t planet_i = jumpscreen_first_enemy_planet(g, pi, &owner_i);
    if (planet_i == PLANET_NONE) {
        log_error("Jumpscreen: bomb requires an enemy-owned planet, none found\n");
        return 2;
    }
    log_message("Jumpscreen: bomb pi=%d attacker=%d owner=%d planet_i=%d\n",
                pi, pi, owner_i, planet_i);
    ui_bomb_show(g, pi, /*attacker_i=*/pi, /*owner_i=*/owner_i, planet_i,
                 /*popdmg=*/5, /*factdmg=*/5,
                 /*play_music=*/false, /*hide_other=*/false);
    return 0;
}

/* --- 7. Election ---------------------------------------------------------- */

/* Replicates the reachable parts of game_election_prepare (which is static).
   We populate enough of the election_s for ui_election_show to render a
   frame of the chamber with both candidates and per-race vote counts. */
static int jump_election(struct game_s *g, const char *args)
{
    (void)args;
    struct election_s el[1];
    uint16_t tbl_votes[PLAYER_NUM];
    int num = 0, total_votes = 0;
    int alive = 0;
    bool found_human = false;

    memset(el, 0, sizeof(*el));
    el->g = g;
    el->buf = ui_get_strbuf();
    el->first_human = PLAYER_NONE;
    el->last_human = PLAYER_NONE;

    /* Populate tbl_ei from alive players. Mirror game_election_prepare:
       human goes into first_human/last_human, others into tbl_ei[]. */
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if (!IS_ALIVE(g, i)) continue;
        ++alive;
        if (IS_HUMAN(g, i)) {
            el->last_human = i;
            if (!found_human) {
                found_human = true;
                el->first_human = i;
                continue;
            }
        }
        el->tbl_ei[num++] = i;
    }
    if (alive < 2) {
        log_error("Jumpscreen: election needs >=2 alive players, found %d\n", alive);
        return 2;
    }
    if (!found_human) {
        /* Fabricate: just use first alive as "human" slot. */
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (IS_ALIVE(g, i)) { el->first_human = el->last_human = i; break; }
        }
    }
    el->tbl_ei[num] = el->first_human;
    el->num = num;

    /* Vote tally from planet pop, exactly like game_election_prepare. */
    memset(tbl_votes, 0, sizeof(tbl_votes));
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->owner != PLAYER_NONE) && (p->unrest != PLANET_UNREST_REBELLION)) {
            tbl_votes[p->owner] += p->pop;
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        uint16_t v = tbl_votes[i];
        v = (v / 100) + ((v % 100) != 0);
        tbl_votes[i] = v;
        total_votes += v;
        el->tbl_votes[i] = v;
    }
    el->total_votes = total_votes;

    /* Pick top-2 vote getters as candidates. Linear scan twice. */
    {
        player_id_t c0 = PLAYER_NONE, c1 = PLAYER_NONE;
        uint16_t v0 = 0, v1 = 0;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (!IS_ALIVE(g, i)) continue;
            if (el->tbl_votes[i] > v0) {
                v1 = v0; c1 = c0;
                v0 = el->tbl_votes[i]; c0 = i;
            } else if (el->tbl_votes[i] > v1) {
                v1 = el->tbl_votes[i]; c1 = i;
            }
        }
        if (c0 == PLAYER_NONE || c1 == PLAYER_NONE) {
            /* No-vote save: just take first two alive players. */
            int chosen = 0;
            for (player_id_t i = PLAYER_0; i < g->players && chosen < 2; ++i) {
                if (!IS_ALIVE(g, i)) continue;
                el->candidate[chosen++] = i;
            }
        } else {
            el->candidate[0] = c0;
            el->candidate[1] = c1;
        }
    }

    el->got_votes[0] = 0;
    el->got_votes[1] = 0;
    el->flag_show_votes = true;
    el->str = game_str_el_start;
    el->cur_i = PLAYER_NONE;
    el->ui_delay = 3;

    log_message("Jumpscreen: election candidates=%d,%d alive=%d total_votes=%d\n",
                el->candidate[0], el->candidate[1], alive, total_votes);

    /* ui_election_show requires el->uictx, set by ui_election_start. */
    ui_election_start(el);
    ui_election_show(el);
    ui_election_end(el);
    return 0;
}

/* --- 8. Battle pre -------------------------------------------------------- */

/* Find a planet that can host a battle: pi has ships in orbit AND some
   opponent has ships at the same planet, OR pi has ships at a planet whose
   owner is an alive opponent. Returns PLANET_NONE if no contested orbit
   exists. Sets *out_opponent to the opposing party. */
static uint8_t jumpscreen_first_contested_planet(struct game_s *g, player_id_t pi,
                                                 player_id_t *out_opponent)
{
    /* Pass 1: shared orbit (both sides have ships present). */
    for (int k = 0; k < g->galaxy_stars; ++k) {
        const empiretechorbit_t *ep = &(g->eto[pi]);
        bool pi_present = false;
        for (int j = 0; j < ep->shipdesigns_num; ++j) {
            if (ep->orbit[k].ships[j] > 0) { pi_present = true; break; }
        }
        if (!pi_present) continue;
        for (player_id_t o = PLAYER_0; o < g->players; ++o) {
            if (o == pi) continue;
            if (!IS_ALIVE(g, o)) continue;
            const empiretechorbit_t *eo = &(g->eto[o]);
            for (int j = 0; j < eo->shipdesigns_num; ++j) {
                if (eo->orbit[k].ships[j] > 0) {
                    if (out_opponent) *out_opponent = o;
                    return (uint8_t)k;
                }
            }
        }
    }
    /* Pass 2: pi has ships at a planet owned by an opponent (planet bases
       count as defender force). */
    for (int k = 0; k < g->galaxy_stars; ++k) {
        const empiretechorbit_t *ep = &(g->eto[pi]);
        bool pi_present = false;
        for (int j = 0; j < ep->shipdesigns_num; ++j) {
            if (ep->orbit[k].ships[j] > 0) { pi_present = true; break; }
        }
        if (!pi_present) continue;
        player_id_t o = g->planet[k].owner;
        if (o != PLAYER_NONE && o != pi && IS_ALIVE(g, o)) {
            if (out_opponent) *out_opponent = o;
            return (uint8_t)k;
        }
    }
    /* Pass 3 (synth fallback): pick any planet where pi has ships and any
       alive opponent — battle_pre is a visual capture, the screen renders
       fine with one side having only ships and the other being the
       nominal aggressor. */
    {
        player_id_t any_opp = PLAYER_NONE;
        for (player_id_t o = PLAYER_0; o < g->players; ++o) {
            if (o == pi) continue;
            if (IS_ALIVE(g, o)) { any_opp = o; break; }
        }
        if (any_opp != PLAYER_NONE) {
            for (int k = 0; k < g->galaxy_stars; ++k) {
                const empiretechorbit_t *ep = &(g->eto[pi]);
                for (int j = 0; j < ep->shipdesigns_num; ++j) {
                    if (ep->orbit[k].ships[j] > 0) {
                        if (out_opponent) *out_opponent = any_opp;
                        return (uint8_t)k;
                    }
                }
            }
        }
    }
    return PLANET_NONE;
}

static int jump_battle_pre(struct game_s *g, const char *args)
{
    (void)args;
    player_id_t pi = g->active_player;
    player_id_t opponent = PLAYER_NONE;
    uint8_t planet_i = jumpscreen_first_contested_planet(g, pi, &opponent);
    if (planet_i == PLANET_NONE || opponent == PLAYER_NONE) {
        /* Final fallback: own homeworld + first alive opponent. battle_pre
           renders the dialogue + planet info even with empty fleets — fine
           for capture. */
        for (player_id_t o = PLAYER_0; o < g->players; ++o) {
            if (o == pi) continue;
            if (IS_ALIVE(g, o)) { opponent = o; break; }
        }
        planet_i = jumpscreen_first_own_planet(g, pi);
        if (planet_i == PLANET_NONE) {
            planet_i = 0; /* deepest fallback */
        }
        if (opponent == PLAYER_NONE) {
            log_error("Jumpscreen: battle_pre needs >=1 alive opponent; none found\n");
            return 2;
        }
        log_message("Jumpscreen: battle_pre using fallback (own homeworld + first opponent), planet_i=%d opp=%d\n",
                    planet_i, opponent);
    }
    struct battle_s bt[1];
    memset(bt, 0, sizeof(*bt));
    bt->g = g;
    bt->flag_human_att = true;
    /* SIDE_R = right (attacker as drawn), SIDE_L = left. game_battle_handle_all
       passes (party_r, party_l) — convention: human/active on right. */
    game_battle_prepare(bt, /*party_r=*/pi, /*party_l=*/opponent, planet_i);
    log_message("Jumpscreen: battle_pre pi=%d opp=%d planet_i=%d items=%d\n",
                pi, opponent, planet_i, bt->items_num);
    ui_battle_pre(g, bt, /*hide_other=*/false, /*winner=*/-1);
    return 0;
}

/* --- 9. Ground ------------------------------------------------------------ */

/* game_ground_resolve_init and game_ground_show_init are static — replicate
   the parts that populate the ground_s fields ui_ground reads. We set up
   tech tier scans for armor / personal armor / shield / weapon, then format
   strings the same way show_init does. */
static void jumpscreen_ground_resolve_one(struct game_s *g, struct ground_side_s *s)
{
    const empiretechorbit_t *e = &(g->eto[s->player]);
    const shipresearch_t *srd = &(g->srd[s->player]);
    const uint8_t *rc;
    uint8_t bestarmor = 0, bestsuit = 0, bestshield = 0, bestweap = 0, besti = 0;

    s->human = IS_HUMAN(g, s->player);
    s->force = 0;

    rc = &(srd->researchcompleted[TECH_FIELD_CONSTRUCTION][0]);
    for (int j = 0; j < e->tech.completed[TECH_FIELD_CONSTRUCTION]; ++j) {
        tech_group_t group = game_tech_get_group(g->gaux, TECH_FIELD_CONSTRUCTION, rc[j]);
        uint8_t tier = game_tech_get_tier(g->gaux, TECH_FIELD_CONSTRUCTION, rc[j]);
        if (group == TECH_GROUP_ARMOR) {
            bestarmor = tier;
        } else if (group == TECH_GROUP_PERSONAL_ARMOR) {
            bestsuit = tier;
            besti = rc[j];
        }
    }
    s->force += bestarmor * 5 + bestsuit * 10;
    s->armori = bestarmor;
    s->suiti = besti;

    besti = 0;
    rc = &(srd->researchcompleted[TECH_FIELD_FORCE_FIELD][0]);
    for (int j = 0; j < e->tech.completed[TECH_FIELD_FORCE_FIELD]; ++j) {
        tech_group_t group = game_tech_get_group(g->gaux, TECH_FIELD_FORCE_FIELD, rc[j]);
        uint8_t tier = game_tech_get_tier(g->gaux, TECH_FIELD_FORCE_FIELD, rc[j]);
        if (group == TECH_GROUP_PERSONAL_SHIELD) {
            bestshield = tier;
            besti = rc[j];
        }
    }
    s->shieldi = besti;
    if (bestshield != 0) s->force += bestshield * 10;

    besti = 0;
    rc = &(srd->researchcompleted[TECH_FIELD_WEAPON][0]);
    for (int j = 0; j < e->tech.completed[TECH_FIELD_WEAPON]; ++j) {
        tech_group_t group = game_tech_get_group(g->gaux, TECH_FIELD_WEAPON, rc[j]);
        uint8_t tier = game_tech_get_tier(g->gaux, TECH_FIELD_WEAPON, rc[j]);
        if (group == TECH_GROUP_PERSONAL_WEAPONS) {
            bestweap = tier;
            besti = rc[j];
        }
    }
    s->weapi = besti;
    if (bestweap != 0) {
        if (bestweap > 2) ++bestweap;
        s->force += bestweap * 5;
    }
    if (e->race == RACE_BULRATHI) s->force += 25;
}

/* String setup mirroring game_ground_show_init. */
static void jumpscreen_ground_show_one(struct game_s *g, struct ground_side_s *s)
{
    char strbuf[GAME_GROUND_STRBUF_SIZE];
    uint8_t besti;
    s->human = IS_HUMAN(g, s->player);
    s->pop2 = s->pop1;
    s->strnum = 1;
    lib_strcpy(strbuf, *tbl_shiptech_armor[s->armori * 2].nameptr, sizeof(strbuf));
    util_str_tolower(&strbuf[1], sizeof(strbuf) - 1);
    lib_sprintf(s->str[0], GAME_GROUND_STRBUF_SIZE, "%s ", strbuf);
    besti = s->suiti;
    if (besti == 0) {
        lib_strcat(s->str[0], game_str_gr_carmor, GAME_GROUND_STRBUF_SIZE);
    } else {
        game_tech_get_name(g->gaux, TECH_FIELD_CONSTRUCTION, besti, strbuf, GAME_GROUND_STRBUF_SIZE);
        lib_strcat(s->str[0], strbuf, GAME_GROUND_STRBUF_SIZE);
    }
    besti = s->shieldi;
    if (besti != 0) {
        game_tech_get_name(g->gaux, TECH_FIELD_FORCE_FIELD, besti, s->str[1], GAME_GROUND_STRBUF_SIZE);
        s->strnum = 2;
    }
    besti = s->weapi;
    if (besti != 0) {
        game_tech_get_name(g->gaux, TECH_FIELD_WEAPON, besti, s->str[s->strnum++], GAME_GROUND_STRBUF_SIZE);
    }
}

static int jump_ground(struct game_s *g, const char *args)
{
    (void)args;
    player_id_t pi = g->active_player;
    player_id_t owner = PLAYER_NONE;
    uint8_t planet_i = jumpscreen_first_enemy_planet(g, pi, &owner);
    if (planet_i == PLANET_NONE) {
        log_error("Jumpscreen: ground requires an enemy-owned planet, none found\n");
        return 2;
    }
    struct ground_s gr[1];
    memset(gr, 0, sizeof(*gr));
    gr->seed = g->seed;
    gr->planet_i = planet_i;
    gr->fact = g->planet[planet_i].factories;
    gr->flag_swap = false;
    gr->flag_rebel = false;
    gr->inbound = 50;       /* synthetic attacker pop count */
    gr->total_inbound = 50;
    /* Convention from game_turn: s[0] = attacker, s[1] = defender.
       The flag_swap flip inside game_ground swaps these for human display;
       since pi is human, mirror that swap so pi sees themselves on the
       attacking side as in real play. */
    gr->s[0].player = pi;
    gr->s[1].player = owner;
    gr->s[0].pop1 = gr->s[0].pop2 = 50;
    gr->s[1].pop1 = gr->s[1].pop2 = g->planet[planet_i].pop ? g->planet[planet_i].pop : 30;
    jumpscreen_ground_resolve_one(g, &gr->s[0]);
    jumpscreen_ground_resolve_one(g, &gr->s[1]);
    /* defender bonus, mirrors resolve_init final line */
    gr->s[gr->flag_swap ? 0 : 1].force += 5;
    jumpscreen_ground_show_one(g, &gr->s[0]);
    jumpscreen_ground_show_one(g, &gr->s[1]);
    log_message("Jumpscreen: ground attacker=%d defender=%d planet_i=%d\n",
                pi, owner, planet_i);
    ui_ground(g, gr);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Section A additions: save / load / news */

static int jump_save(struct game_s *g, const char *args)
{
    (void)args;
    ui_save_game(g);
    return 0;
}

static int jump_load(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    ui_load_game();
    return 0;
}

static int jump_news_won_good(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    ui_news_won(true);
    return 0;
}

static int jump_news_won_bad(struct game_s *g, const char *args)
{
    (void)g; (void)args;
    ui_news_won(false);
    return 0;
}

/* News rollup: ui_news() takes a struct news_s* with type/subtype that index
   into the EVENTMSG table via EVENTMSG_PTR(ga, t, s) = (t-1)*7+s offset.
   type=0 segfaults (negative offset). Use GAME_NEWS_STARS (19), subtype=0
   which is a benign stat-style report. */
static int jump_news(struct game_s *g, const char *args)
{
    struct news_s ns;
    (void)args;
    memset(&ns, 0, sizeof(ns));
    ns.type = GAME_NEWS_STARS;
    ns.subtype = 0;
    ui_news_start();
    ui_news(g, &ns);
    ui_news_end();
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Section B additions: starmap sub-panels (Tier 1 — just (g, pi)) */

static int jump_starmap_orbit_own(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_orbit_own(g, g->active_player);
    return 0;
}

static int jump_starmap_orbit_en(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_orbit_en(g, g->active_player);
    return 0;
}

static int jump_starmap_enroute(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_enroute(g, g->active_player);
    return 0;
}

static int jump_starmap_reloc(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_reloc(g, g->active_player);
    return 0;
}

static int jump_starmap_ships(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_ships(g, g->active_player);
    return 0;
}

static int jump_starmap_trans(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_trans(g, g->active_player);
    return 0;
}

static int jump_starmap_transport(struct game_s *g, const char *args)
{
    (void)args;
    ui_starmap_transport(g, g->active_player);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Section C additions: popups / dialogs */

static int jump_basescrap(struct game_s *g, const char *args)
{
    (void)args;
    ui_basescrap(g, g->active_player);
    return 0;
}

/* Sample yes/no prompt for cosmetic capture. */
static int jump_dialog_yesno(struct game_s *g, const char *args)
{
    (void)args;
    ui_dialog_yesno(g, g->active_player,
                    "Are you sure you want to scrap this design?",
                    100, 80, 16);
    return 0;
}

/* Sample multi-choice prompt for cosmetic capture. */
static int jump_dialog_choose(struct game_s *g, const char *args)
{
    (void)args;
    ui_dialog_choose(g, g->active_player,
                     "Choose action: ATTACK\nDEFEND\nRETREAT",
                     100, 80, 16);
    return 0;
}

/* -------------------------------------------------------------------------- */

static const struct ui_jumpscreen_entry_s ui_jumpscreen_table[] = {
    /* Tier 1: (g, pi) only — no extra args */
    { "empire_status",   jump_empire_status,   "Empire status / racial stats" },
    { "newgame",         jump_newgame,         "New game setup" },
    { "starmap",         jump_starmap,         "Galactic starmap" },
    { "empire_planets",  jump_empire_planets,  "Empire planets list" },
    { "fleet",           jump_fleet,           "Fleet view" },
    { "tech",            jump_tech,            "Research / tech tree" },
    { "specs",           jump_specs,           "Ship specs (args: cheat=1)" },
    { "races",           jump_races,           "Races overview" },
    { "gameopts",        jump_gameopts,        "Game options" },
    /* Tier 1 popups (advisor: verify they draw their own background) */
    { "newtech_alert",   jump_newtech_alert,   "New tech complete popup" },
    { "newships_alert",  jump_newships_alert,  "New ships available popup" },
    { "spy_caught",      jump_spy_caught,      "Spy caught popup" },
    { "help",            jump_help,            "Help popup (args: index=N)" },
    /* No-save / pre-game */
    { "intro",           jump_intro,           "Title intro animation" },
    /* Tier 2: requires opponent=NAME (race name, case-insensitive) */
    { "empire_report",   jump_empire_report,   "Per-empire intel report (args: opponent=NAME)" },
    { "audience",        jump_audience,        "Diplomacy chamber (args: opponent=NAME)" },
    /* Tier 3: transient state synthesized in dispatch.
       Animations: capture a single representative frame via MOO_AUTOSHOT_FRAMES. */
    { "ending_victory",  jump_ending_victory,  "Ending: good victory cinematic" },
    { "ending_tyrant",   jump_ending_tyrant,   "Ending: tyrant victory cinematic" },
    { "ending_funeral",  jump_ending_funeral,  "Ending: funeral cinematic" },
    { "ending_exile",    jump_ending_exile,    "Ending: exile cinematic" },
    { "landing",         jump_landing,         "Landing animation (own homeworld)" },
    { "bomb",            jump_bomb,            "Bombing run on first known enemy planet" },
    { "election",        jump_election,        "Galactic Council election chamber" },
    { "battle_pre",      jump_battle_pre,      "Pre-battle dialogue (synthesized fleets)" },
    { "ground",          jump_ground,          "Ground combat (synthesized state)" },
    /* Section A additions: save / load / news */
    { "save",            jump_save,            "Save game dialog" },
    { "load",            jump_load,            "Load game dialog" },
    { "news",            jump_news,            "News rollup screen (synthesized event)" },
    { "news_won_good",   jump_news_won_good,   "Won-good banner" },
    { "news_won_bad",    jump_news_won_bad,    "Won-bad banner" },
    /* Section B: starmap sub-panels (in-screen panels of the starmap) */
    { "starmap_orbit_own",  jump_starmap_orbit_own,  "Starmap: own colony orbit panel" },
    { "starmap_orbit_en",   jump_starmap_orbit_en,   "Starmap: enemy colony orbit panel" },
    { "starmap_enroute",    jump_starmap_enroute,    "Starmap: en-route fleet panel" },
    { "starmap_reloc",      jump_starmap_reloc,      "Starmap: fleet relocation panel" },
    { "starmap_ships",      jump_starmap_ships,      "Starmap: ship list panel" },
    { "starmap_trans",      jump_starmap_trans,      "Starmap: transport assign panel" },
    { "starmap_transport",  jump_starmap_transport,  "Starmap: transport in-flight panel" },
    /* Section C: popups / dialogs */
    { "basescrap",       jump_basescrap,       "Base scrap confirm dialog" },
    { "dialog_yesno",    jump_dialog_yesno,    "Yes/No confirm dialog (sample prompt)" },
    { "dialog_choose",   jump_dialog_choose,   "Multi-choice dialog (sample prompt)" },
    { NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

int ui_jumpscreen_dispatch(struct game_s *g, const char *spec)
{
    char name_buf[64];
    const char *args = "";
    const char *colon;
    size_t name_len;
    const struct ui_jumpscreen_entry_s *e;

    if (spec == NULL || spec[0] == '\0') {
        log_error("Jumpscreen: empty spec\n");
        return 2;
    }

    /* Split NAME from :ARGS. */
    colon = strchr(spec, ':');
    name_len = (colon != NULL) ? (size_t)(colon - spec) : strlen(spec);
    if (name_len == 0 || name_len >= sizeof(name_buf)) {
        log_error("Jumpscreen: invalid spec '%s'\n", spec);
        return 2;
    }
    memcpy(name_buf, spec, name_len);
    name_buf[name_len] = '\0';
    if (colon != NULL) {
        args = colon + 1;
    }

    for (e = ui_jumpscreen_table; e->name != NULL; ++e) {
        if (strcmp(e->name, name_buf) == 0) {
            log_message("Jumpscreen: dispatching '%s' (%s)\n", e->name, e->desc);
            return e->fn(g, args);
        }
    }

    log_error("Jumpscreen: unknown screen '%s'. Known screens:\n", name_buf);
    for (e = ui_jumpscreen_table; e->name != NULL; ++e) {
        log_error("  %-24s %s\n", e->name, e->desc);
    }
    return 2;
}
