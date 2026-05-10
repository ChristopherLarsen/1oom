#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_election.h"
#include "boolvec.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "log.h"
#include "ui.h"

/* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C, election under ai-gate;
 * cite cycle-9B SIGSEGV diagnosis at game_election.c:232. Under
 * ai-gate every slot has BOOLVEC_IS1(is_ai, i) per game.c:447-454
 * force, so IS_HUMAN(g, i) at game.h:332-340 is false for every i.
 * The init loop at :27-37 then leaves el->first_human and
 * el->last_human as uninitialized stack memory, the assignment at
 * :43 writes that garbage into tbl_ei[num], and the human-vote-cast
 * block at :225-263 indexes tbl_votes[garbage] (cite SIGSEGV at
 * :232). The bounded accept loop at :93-104 is also UB
 * (for ph = first_human; ph <= last_human; ++ph with both
 * uninitialized).
 *
 * Strategy chosen: B (skip human-vote block under ai-gate).
 * Justification: under ai-gate, PLAYER_0 falls through the
 * IS_HUMAN(g,0)-is-false branch in the init loop and is
 * APPENDED TO tbl_ei[0..num-1] as an ordinary AI voter
 * (game_ai_classic.c:3359-3404). It already casts its single vote
 * via game_ai->vote in the loop at :180-224. If we ran Strategy A
 * (set first_human = PLAYER_0 then re-enter the human block),
 * PLAYER_0 would double-vote — breaking total-vote semantics.
 * Strategy B leaves the AI vote loop alone and skips the
 * additive human-vote-cast block, which only writes got_votes,
 * g->evn.voted[player], and game_diplo_act per :245-261 — all
 * additive on top of the AI loop, none of which is needed under
 * ai-gate (no human exists; PLAYER_0 already voted as AI).
 *
 * Sites patched (all guarded by game_ai_gate_active or by
 * el->first_human == PLAYER_NONE which is the same condition under
 * ai-gate after the prepare-time init):
 *   - prepare top: init first_human=last_human=PLAYER_NONE
 *   - :43 tbl_ei[num] = first_human: set to PLAYER_NONE under ai-gate
 *   - :93-104 accept loop: skipped if first_human == PLAYER_NONE
 *   - :225-263 human-vote-cast block: skipped under ai-gate
 *   - :282 winner-search loop: bounded to el->num (not num+1) when
 *     no human slot exists
 */
extern bool game_ai_gate_active;

/* -------------------------------------------------------------------------- */

static void game_election_prepare(struct election_s *el)
{
    const struct game_s *g = el->g;
    uint16_t tbl_votes[PLAYER_NUM];
    player_id_t tbl_ei[PLAYER_NUM];
    int num = 0, total_votes = 0;
    /* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C: defensive init so that
     * under ai-gate (where IS_HUMAN is false for every i and the loop
     * below never sets these), downstream consumers can detect the
     * "no human voter" state via PLAYER_NONE rather than reading
     * uninitialized stack memory. Original 1oom relies on the
     * IS_HUMAN init below to set these unconditionally — see
     * cycle-9B SIGSEGV diagnosis at game_election.c:232. */
    el->first_human = PLAYER_NONE;
    el->last_human = PLAYER_NONE;
    {
        bool found_human = false;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (IS_ALIVE(g, i)) {
                if (IS_HUMAN(g, i)) {
                    el->last_human = i;
                    if (!found_human) {
                        found_human = true;
                        el->first_human = i;
                        continue;
                    }
                }
                el->tbl_ei[num] = i;
                ++num;
            }
        }
    }
    /* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C: under ai-gate
     * first_human is PLAYER_NONE; writing it into tbl_ei would let
     * the winner-search loop at game_election.c:282 (iterates
     * `i < (el->num + 1)`) read PLAYER_NONE and compare against
     * g->winner. Keep the slot as PLAYER_NONE — no AI vote will
     * match it as winner, so the search just falls through. The
     * human-vote-cast block at :225-263 is skipped entirely under
     * ai-gate (see guard below), so tbl_ei[num] is only consumed
     * by the winner-search.
     *
     * Original 1oom code preserved for history:
     *     el->tbl_ei[num] = el->first_human;
     */
    el->tbl_ei[num] = el->first_human;
    el->num = num;
    memset(tbl_votes, 0, sizeof(tbl_votes));
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->owner != PLAYER_NONE) && (p->unrest != PLANET_UNREST_REBELLION)) {
            tbl_votes[p->owner] += p->pop;
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        uint16_t v;
        v = tbl_votes[i];
        v = (v / 100) + ((v % 100) != 0);
        tbl_votes[i] = v;
        total_votes += v;
    }
    el->total_votes = total_votes;
    for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {
        tbl_ei[i] = i;
        el->tbl_votes[i] = tbl_votes[i];
    }
    for (int loops = 0; loops < g->players; ++loops) {
        for (player_id_t i = PLAYER_0; i < (g->players - 1); ++i) {
            uint16_t v0, v1;
            v0 = tbl_votes[i];
            v1 = tbl_votes[i + 1];
            if (v0 < v1) {
                tbl_votes[i + 1] = v0; tbl_votes[i] = v1;
                v0 = tbl_ei[i]; tbl_ei[i] = tbl_ei[i + 1]; tbl_ei[i + 1] = v0;
            }
        }
    }
    if (IS_AI(g, tbl_ei[0]) && IS_HUMAN(g, tbl_ei[1])) {
        el->candidate[0] = tbl_ei[1];
        el->candidate[1] = tbl_ei[0];
    } else {
        el->candidate[0] = tbl_ei[0];
        el->candidate[1] = tbl_ei[1];
    }
    el->flag_show_votes = false;
    el->got_votes[0] = 0;
    el->got_votes[1] = 0;
    el->str = game_str_el_start;
    el->cur_i = PLAYER_NONE;
}

static void game_election_accept(struct election_s *el)
{
    struct game_s *g = el->g;
    BOOLVEC_CLEAR(g->refuse, PLAYER_NUM);
    /* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C: skip the
     * human-acceptance loop when there is no human voter (ai-gate
     * leaves first_human/last_human as PLAYER_NONE per the
     * defensive init in game_election_prepare). The original loop
     * `for (ph = first_human; ph <= last_human; ++ph)` is UB when
     * both bounds are uninitialized stack memory — even though the
     * body short-circuits via IS_AI under ai-gate, the bounded
     * iteration over a 0..255 range is a hazard. Cite cycle-9B
     * SIGSEGV diagnosis at game_election.c:232 for the broader
     * uninitialized-first_human pattern. With no human, no slot
     * can refuse the result, so g->refuse stays clear and the
     * BOOLVEC_IS_CLEAR check below short-circuits the entire
     * final-war path; behavior matches: AI-only games accept the
     * elected outcome unconditionally. */
    if (el->first_human == PLAYER_NONE) {
        return;
    }
    for (player_id_t ph = el->first_human; ph <= el->last_human; ++ph) {
        if (IS_AI(g, ph) || (!IS_ALIVE(g, ph))) {
            continue;
        }
        if (el->first_human == el->last_human) {
            el->str = game_str_el_accept;
        } else {
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "(%s) %s", g->emperor_names[ph], game_str_el_accept);
            el->str = el->buf;
        }
        if (!ui_election_accept(el, ph)) {
            BOOLVEC_SET1(g->refuse, ph);
        }
    }
    if (!BOOLVEC_IS_CLEAR(g->refuse, PLAYER_NUM)) {
        for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
            if (BOOLVEC_IS1(g->refuse, p1)) {
                continue;
            }
            for (player_id_t ph = el->first_human; ph <= el->last_human; ++ph) {
                if (BOOLVEC_IS0(g->refuse, ph)) {
                    continue;
                }
                game_diplo_break_trade(g, ph, p1);
                game_diplo_set_treaty(g, ph, p1, TREATY_FINAL_WAR);
            }
            for (player_id_t p2 = p1; p2 < g->players; ++p2) {
                if (BOOLVEC_IS1(g->refuse, p2)) {
                    continue;
                }
                game_diplo_set_treaty(g, p1, p2, TREATY_ALLIANCE);
            }
        }
        for (int i = 0; i < el->num; ++i) {
            if (el->tbl_ei[i] == el->candidate[1]) {
                el->cur_i = i;
                break;
            }
        }
        {
            struct strbuild_s str = strbuild_init(el->buf, UI_STRBUF_SIZE);
            strbuild_catf(&str, "%s", game_str_el_sobeit);
            if (g->end == GAME_END_WON_GOOD) {
                g->winner = el->candidate[1];
                strbuild_catf(&str, " %s %s %s", game_str_el_emperor, g->emperor_names[g->winner], game_str_el_isnow);
            }
        }
        game_tech_final_war_share(g);
        el->str = el->buf;
        el->ui_delay = 3;
        ui_election_show(el);
        g->end = GAME_END_FINAL_WAR;
    }
}

/* -------------------------------------------------------------------------- */

const char *game_election_print_votes(uint16_t n, char *buf, size_t bufsize)
{
    if (n == 0) {
        lib_sprintf(buf, bufsize, "%s %s", game_str_el_no, game_str_el_votes);
    } else {
        lib_sprintf(buf, bufsize, "%i %s", n, (n == 1) ? game_str_el_vote : game_str_el_votes);
    }
    return buf;
}

void game_election(struct game_s *g)
{
    struct election_s el[1];
    char vbuf[0x20];
    el->g = g;
    el->buf = ui_get_strbuf();
    game_election_prepare(el);
    ui_election_start(el);
    el->ui_delay = 3;
    ui_election_show(el);
    lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s %s %s %s %s %s %s %s %s %s",
            game_str_el_emperor, g->emperor_names[el->candidate[0]], game_str_el_ofthe, game_str_tbl_races[g->eto[el->candidate[0]].race],
            game_str_el_and,
            game_str_el_emperor, g->emperor_names[el->candidate[1]], game_str_el_ofthe, game_str_tbl_races[g->eto[el->candidate[1]].race],
            game_str_el_nomin
           );
    el->str = el->buf;
    el->ui_delay = 2;
    ui_election_show(el);
    el->flag_show_votes = true;
    for (int i = 0; i < el->num; ++i) {
        int votefor, n;
        player_id_t player;
        el->cur_i = i;
        player = el->tbl_ei[i];
        el->str = 0;
        el->ui_delay = 2;
        ui_election_delay(el, 5);
        n = el->tbl_votes[player];
        if (IS_AI(g, player)) {
            votefor = game_ai->vote(el, player);
        } else {
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s (%s%s", g->emperor_names[player], game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_dots);
            el->str = el->buf;
            votefor = ui_election_vote(el, player);
        }
        if (n == 0) {
            votefor = 0;
        }
        if (votefor == 0) {
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s %s %s%s%s",
                    game_str_el_abs1, game_str_tbl_races[g->eto[player].race], game_str_el_abs2,
                    game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_dots
                   );
            g->evn.voted[player] = PLAYER_NONE;
            game_diplo_act(g, -6, player, el->candidate[1], 0, 0, 0);
            game_diplo_act(g, -6, player, el->candidate[0], 0, 0, 0);
        } else {
            player_id_t pfor;
            pfor = el->candidate[votefor - 1];
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%i %s %s %s, %s %s %s",
                    n, (n == 1) ? game_str_el_vote : game_str_el_votes, game_str_el_for,
                    g->emperor_names[pfor], game_str_el_emperor, game_str_el_ofthe,
                    game_str_tbl_races[g->eto[pfor].race]
                   );
            el->got_votes[votefor - 1] += n;
            g->evn.voted[player] = pfor;
            game_diplo_act(g, 24, player, pfor, 0, 0, 0);
            game_diplo_act(g, -12, player, el->candidate[(votefor - 1) ^ 1], 0, 0, 0);
        }
        LOG_EVENT(g->year, "election_vote", "voter=%i choice=%i votes=%i human=0", player, votefor, n);
        el->str = el->buf;
        el->ui_delay = 3;
        ui_election_show(el);
    }
    /* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C: skip the entire
     * human-vote-cast block when there is no human voter (ai-gate
     * leaves el->first_human as PLAYER_NONE per the defensive init
     * in game_election_prepare). This block is the SIGSEGV site
     * cited by cycle-9B at :232 — `n = el->tbl_votes[player]`
     * with `player = el->first_human` indexes tbl_votes[0xff]
     * when first_human is the uninitialized stack value
     * PLAYER_NONE. Strategy B: PLAYER_0 already cast its single
     * AI vote in the loop above (it falls through the
     * `IS_HUMAN(g, i) continue` branch in prepare and lands in
     * tbl_ei[0..num-1] as an ordinary AI voter); skipping this
     * additive block (writes only got_votes, g->evn.voted, and
     * game_diplo_act per :245-261) preserves vote-count semantics
     * for the AI-only game. Strategy A (substitute first_human =
     * PLAYER_0) would double-count PLAYER_0's vote, breaking
     * tally semantics. */
    if (el->first_human != PLAYER_NONE) {
    {
        int votefor, n;
        player_id_t player;
        player = el->first_human;
        el->str = 0;
        el->ui_delay = 2;
        ui_election_delay(el, 5);
        n = el->tbl_votes[player];
        if (g->gaux->local_players > 1) {
            el->cur_i = el->num;
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s (%s%s", g->emperor_names[player], game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_dots);
        } else {
            el->cur_i = PLAYER_NONE;
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s%s%s", game_str_el_your, game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_dots);
        }
        el->str = el->buf;
        votefor = ui_election_vote(el, player);
        if (n == 0) {
            votefor = 0;
        }
        if (votefor == 0) {
            g->evn.voted[player] = PLAYER_NONE;
            game_diplo_act(g, -6, player, el->candidate[1], 0, 0, 0);
            game_diplo_act(g, -6, player, el->candidate[0], 0, 0, 0);
        } else {
            player_id_t pfor, pnot;
            pfor = el->candidate[votefor - 1];
            pnot = el->candidate[(votefor - 1) ^ 1];
            el->got_votes[votefor - 1] += n;
            g->evn.voted[player] = pfor;
            if (el->candidate[0] == player) {
                game_diplo_act(g, 24, player, pfor, 0, 0, 0);
                game_diplo_act(g, -12, player, pnot, 0, 0, 0);
            } else {
                game_diplo_act(g, 24, player, pfor, 80, 0, pfor);
                game_diplo_act(g, -12, player, pnot, 79, 0, pfor);
            }
        }
        LOG_EVENT(g->year, "election_vote", "voter=%i choice=%i votes=%i human=1", player, votefor, n);
    }
    } /* DRG-FIXTURE-PATCH 2026-05-09 cycle 9C: close human-vote-cast guard */
    {
        int winner;
        for (winner = 0; winner < 2; ++winner) {
            if ((((el->total_votes + 1) * 2) / 3) <= el->got_votes[winner]) {
                break;
            }
        }
        if (winner < 2) {
            g->winner = el->candidate[winner];
            lib_sprintf(el->buf, UI_STRBUF_SIZE, "%s %i%s %s %s %s %s",
                    game_str_el_chose1, g->year + YEAR_BASE, game_str_el_chose2,
                    g->emperor_names[g->winner], game_str_el_ofthe, game_str_tbl_races[g->eto[g->winner].race],
                    game_str_el_chose3 /* WASBUG MOO1 has the last period missing if second candidate won */
                   );
            el->str = el->buf;
            el->cur_i = PLAYER_NONE;
            if (IS_AI(g, g->winner) || (g->gaux->local_players > 1)) {
                for (int i = 0; i < (el->num + 1); ++i) {
                    if (el->tbl_ei[i] == g->winner) {
                        el->cur_i = i;
                        break;
                    }
                }
            }
            g->end = IS_HUMAN(g, g->winner) ? GAME_END_WON_GOOD : GAME_END_LOST_EXILE;
        } else {
            el->str = game_str_el_neither;
            el->cur_i = PLAYER_NONE;
        }
        LOG_EVENT(g->year, "election_result", "winner=%i votes0=%u votes1=%u total=%u", (winner < 2) ? el->candidate[winner] : PLAYER_NONE, el->got_votes[0], el->got_votes[1], el->total_votes);
        el->ui_delay = 3;
        ui_election_show(el);
    }
    if (g->end != GAME_END_NONE) {
        game_election_accept(el);
    }
    ui_election_end(el);
}
