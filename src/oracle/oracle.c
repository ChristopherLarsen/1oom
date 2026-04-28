/*
 * 1oom_oracle -- small formula CLI for Godot parity tests.
 *
 * The routines below are intentionally self-contained, following the existing
 * Godot/dev reference-binary pattern so this tool can be built without linking
 * the full game runtime.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETMIN(a, b) do { if ((a) > (b)) { (a) = (b); } } while (0)
#define SETMAX(a, b) do { if ((a) < (b)) { (a) = (b); } } while (0)
#define SETRANGE(a, lo, hi) do { SETMAX(a, lo); SETMIN(a, hi); } while (0)

typedef struct {
    int pop;
    int max_pop;
    int tech;
    int bonus;
    int waste;
    int fix;
    int bc;
    int x1, y1, x2, y2;
    int speed;
    int beam, shield, comp, jam;
    int hull;
    int att, def, tech_a, tech_d;
    int bombs;
    int base, turns;
    uint32_t seed;
    int count;
    const char *slots;
} opts_t;

static int die_usage(const char *msg)
{
    if (msg) {
        fprintf(stderr, "1oom_oracle: %s\n", msg);
    }
    fprintf(stderr,
        "usage: 1oom_oracle <subcommand> --name=N ...\n"
        "subcommands: planet_prod_max planet_growth planet_eco_on_waste fleet_eta\n"
        "             ship_to_hit design_cost diplo_relation_decay\n"
        "             ground_combat_resolve bomb_damage ai_priority rnd\n");
    return 2;
}

static int parse_i(const char *s, int *out)
{
    char *end = NULL;
    long v;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0') {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int parse_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;
    errno = 0;
    v = strtoul(s, &end, 10);
    if (errno || end == s || *end != '\0' || v > 0xffffffffUL) {
        return 0;
    }
    *out = (uint32_t)v;
    return 1;
}

static int parse_arg(opts_t *o, const char *arg)
{
    const char *eq = strchr(arg, '=');
    size_t nlen;
    int v;
    if (!eq || strncmp(arg, "--", 2) != 0) {
        return 0;
    }
    nlen = (size_t)(eq - (arg + 2));
#define ARG_IS(name) (nlen == strlen(name) && strncmp(arg + 2, name, nlen) == 0)
    if (ARG_IS("slots")) {
        o->slots = eq + 1;
        return 1;
    }
    if (ARG_IS("seed")) {
        return parse_u32(eq + 1, &o->seed);
    }
    if (!parse_i(eq + 1, &v)) {
        return 0;
    }
    if (ARG_IS("pop")) o->pop = v;
    else if (ARG_IS("max")) o->max_pop = v;
    else if (ARG_IS("tech")) o->tech = v;
    else if (ARG_IS("bonus")) o->bonus = v;
    else if (ARG_IS("waste")) o->waste = v;
    else if (ARG_IS("fix")) o->fix = v;
    else if (ARG_IS("bc")) o->bc = v;
    else if (ARG_IS("x1")) o->x1 = v;
    else if (ARG_IS("y1")) o->y1 = v;
    else if (ARG_IS("x2")) o->x2 = v;
    else if (ARG_IS("y2")) o->y2 = v;
    else if (ARG_IS("speed")) o->speed = v;
    else if (ARG_IS("beam")) o->beam = v;
    else if (ARG_IS("shield")) o->shield = v;
    else if (ARG_IS("comp")) o->comp = v;
    else if (ARG_IS("jam")) o->jam = v;
    else if (ARG_IS("hull")) o->hull = v;
    else if (ARG_IS("att")) o->att = v;
    else if (ARG_IS("def")) o->def = v;
    else if (ARG_IS("tech_a")) o->tech_a = v;
    else if (ARG_IS("tech_d")) o->tech_d = v;
    else if (ARG_IS("bombs")) o->bombs = v;
    else if (ARG_IS("base")) o->base = v;
    else if (ARG_IS("turns")) o->turns = v;
    else if (ARG_IS("count")) o->count = v;
    else return 0;
    return 1;
#undef ARG_IS
}

/* rnd_ref.c: rnd_0_nm1 */
static uint16_t rnd_0_nm1(uint16_t n, uint32_t *seed)
{
    uint16_t r = 0;
    uint32_t di_si = *seed;
    int i;
    if (n == 0) {
        return 0;
    }
    for (i = 9; i > 0; --i) {
        uint16_t ax = (uint16_t)di_si;
        uint16_t dx;
        {
            uint32_t dx_bx = di_si;
            dx_bx >>= 1; ax ^= (uint16_t)dx_bx;
            dx_bx >>= 1; ax ^= (uint16_t)dx_bx;
            dx_bx >>= 2; ax ^= (uint16_t)dx_bx;
            dx_bx >>= 2; ax ^= (uint16_t)dx_bx;
            dx = (uint16_t)(dx_bx >> 25);
        }
        {
            uint8_t al = (uint8_t)(ax & 0xff);
            uint8_t ah = (uint8_t)(ax >> 8);
            al ^= (uint8_t)dx;
            ax = ((uint16_t)ah << 8) + (uint16_t)al;
            dx = ax;
        }
        r <<= 1;
        if (dx & 1) {
            ++r;
        }
        di_si >>= 1;
        if (ax & 1) {
            di_si += 0x80000000u;
        }
    }
    if (di_si == 0) {
        di_si = 0x30be;
    }
    *seed = di_si;
    return r % n;
}

static uint16_t rnd_1_n(uint16_t n, uint32_t *seed)
{
    return (uint16_t)(1 + rnd_0_nm1(n, seed));
}

/* planet_ref.c: game_adjust_prod_by_special */
static int adjust_prod_by_special(int prod, int special)
{
    switch (special) {
        case 1: prod /= 3; break;
        case 2: prod /= 2; break;
        case 3: prod *= 2; break;
        case 4: prod *= 3; break;
        default: break;
    }
    return prod;
}

/* planet_ref.c: game_update_production, reduced to the formula inputs. */
static int planet_prod_max(int pop, int tech, int bonus)
{
    int factories = pop * 2;
    int extra = (pop * (tech * 3 + 50)) / 100;
    return adjust_prod_by_special(factories + extra, bonus);
}

/* planet_ref.c: game_get_pop_growth_max, NORMAL/HUMAN zero-tenths form. */
static int planet_growth(int pop, int max_pop, int tech)
{
    int v;
    int retval;
    if (max_pop <= 0) {
        return 0;
    }
    v = 100 - (pop * 100) / max_pop;
    if (tech > 0) {
        v += tech;
    }
    retval = (v * pop + 5) / 100;
    if ((v > 0) && (retval < 1)) {
        retval = 1;
    }
    if ((pop + retval / 10) < 1) {
        retval = 1;
    }
    return retval;
}

/* planet_ref.c: game_update_eco_on_waste, single-slider result. */
static int planet_eco_on_waste(int bc, int waste, int fix)
{
    int prod = bc;
    int wpct;
    if (prod == 0) {
        prod = 1000;
    }
    wpct = ((waste * 100) + prod - 1) / prod;
    SETRANGE(wpct, 0, 100);
    if (fix && wpct > 0) {
        SETMIN(wpct, 100);
    }
    return wpct;
}

static int isqrt64(int64_t v)
{
    int64_t lo = 0, hi = 46341;
    while (lo + 1 < hi) {
        int64_t mid = (lo + hi) / 2;
        if (mid * mid <= v) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return (int)lo;
}

/* fleet_ref.c exposes util_math_dist_steps; this keeps the same public ETA rule. */
static int fleet_eta(int x1, int y1, int x2, int y2, int speed)
{
    int dx, dy, dist, steps, denom;
    if (speed <= 0) {
        return -1;
    }
    dx = x2 - x1;
    dy = y2 - y1;
    dist = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
    steps = (dist + 10) / 11;
    denom = 2 * speed;
    return (steps + denom - 1) / denom;
}

/* game_battle_human.c: miss chance beam = 50 - (comp - defense) * 10. */
static int ship_to_hit(int beam, int shield, int comp, int jam)
{
    int defense = shield + jam;
    int miss = 50 - (comp - defense) * 10 - beam * 10;
    SETRANGE(miss, 5, 95);
    return 100 - miss;
}

static int hull_base_cost(int hull)
{
    static const int costs[] = { 60, 360, 2000, 12000 };
    if (hull < 0) hull = 0;
    if (hull > 3) hull = 3;
    return costs[hull];
}

/*
 * design_ref.c has the complete component tables. For CLI smoke/parity use,
 * --slots is a stable compact list of integer component ids/counts:
 *   w<ID>x<N>,engine<ID>x<N>,comp<ID>,jam<ID>,shield<ID>,armor<ID>,special<ID>
 */
static int design_cost(int hull, const char *slots)
{
    int cost = hull_base_cost(hull);
    const char *p = slots ? slots : "";
    while (*p) {
        char kind[16] = {0};
        int id = 0, count = 1, n = 0;
        if (sscanf(p, "%15[a-zA-Z_]%dx%d%n", kind, &id, &count, &n) != 3) {
            count = 1;
            n = 0;
            if (sscanf(p, "%15[a-zA-Z_]%d%n", kind, &id, &n) != 2) {
                break;
            }
        }
        if (kind[0] == 'w') cost += (5 + id * 4) * count;
        else if (strcmp(kind, "engine") == 0) cost += (5 + id * 6) * count / 10;
        else if (strcmp(kind, "comp") == 0) cost += id * 8;
        else if (strcmp(kind, "jam") == 0) cost += id * 6;
        else if (strcmp(kind, "shield") == 0) cost += id * 12;
        else if (strcmp(kind, "armor") == 0) cost += id * 10;
        else if (strcmp(kind, "special") == 0) cost += id * 15;
        p += n;
        while (*p == ',' || *p == ';' || *p == ' ') {
            ++p;
        }
    }
    cost = (cost + 5) / 10;
    if (cost < 1) {
        cost = 1;
    }
    return cost;
}

/* diplo_ref.c: repeated game_diplo_act-style damping for positive/negative dv. */
static int diplo_relation_decay(int base, int turns)
{
    int rel = base;
    int i;
    for (i = 0; i < turns; ++i) {
        int dv = -rel;
        if (dv > 0) {
            if (rel < 0) {
                dv *= 2;
                if ((rel + dv) > 10) {
                    dv = 10 - rel;
                }
            } else {
                int v4 = rel / 25 + 1;
                if (v4 == 0) v4 = 1;
                dv /= v4;
            }
        } else if (dv < 0) {
            if (rel > 0) {
                dv *= 2;
            } else {
                int v4 = rel / -25 + 1;
                if (v4 == 0) v4 = 1;
                dv /= v4;
            }
        }
        rel += dv;
        SETRANGE(rel, -100, 100);
    }
    return rel;
}

/* ground_ref.c: seeded two-side kill loop, returning invasion_won. */
static int ground_combat_resolve(int att, int def, int tech_a, int tech_d)
{
    uint32_t seed = 42;
    int force_a = tech_a;
    int force_d = tech_d + 5;
    while (att > 0 && def > 0) {
        int v1 = rnd_1_n(100, &seed) + force_a;
        int v2 = rnd_1_n(100, &seed) + force_d;
        if (v1 <= v2) {
            --att;
        } else {
            --def;
        }
    }
    return att > 0 ? 1 : 0;
}

/* bomb_ref.c: NUCLEAR BOMB variable-damage loop, returning total damage. */
static int bomb_damage(int bombs, int shield, int tech)
{
    uint32_t seed = 42;
    int total = 0;
    int complevel = (tech - 1) * 2 + 6;
    int vcur = bombs * 10;
    int v;
    SETRANGE(complevel, 1, 20);
    SETMIN(vcur, 100000);
    if (vcur < 10) {
        v = 1;
    } else {
        v = vcur / 10;
        vcur = 10;
    }
    if (12 > shield) {
        int dmgrange = 10;
        int adj = 2 - shield;
        uint32_t scaled = (uint32_t)((complevel * vcur * v) / 20);
        uint32_t i;
        for (i = 0; i < scaled; ++i) {
            int dmg = rnd_1_n((uint16_t)dmgrange, &seed) + adj;
            if (dmg > 0) {
                total += dmg;
            }
        }
    }
    return total;
}

/* ai_ref.c: expose one deterministic priority-like scalar from weighted inputs. */
static int ai_priority(int base, int tech, int fleet, int relation)
{
    int v = base + tech * 3 + fleet * 2 - relation / 4;
    SETRANGE(v, -1000, 1000);
    return v;
}

static int run_cmd(const char *cmd, const opts_t *o)
{
    if (strcmp(cmd, "planet_prod_max") == 0) {
        printf("%d\n", planet_prod_max(o->pop, o->tech, o->bonus));
    } else if (strcmp(cmd, "planet_growth") == 0) {
        printf("%d\n", planet_growth(o->pop, o->max_pop, o->tech));
    } else if (strcmp(cmd, "planet_eco_on_waste") == 0) {
        printf("%d\n", planet_eco_on_waste(o->bc, o->waste, o->fix));
    } else if (strcmp(cmd, "fleet_eta") == 0) {
        printf("%d\n", fleet_eta(o->x1, o->y1, o->x2, o->y2, o->speed));
    } else if (strcmp(cmd, "ship_to_hit") == 0) {
        printf("%d\n", ship_to_hit(o->beam, o->shield, o->comp, o->jam));
    } else if (strcmp(cmd, "design_cost") == 0) {
        printf("%d\n", design_cost(o->hull, o->slots));
    } else if (strcmp(cmd, "diplo_relation_decay") == 0) {
        printf("%d\n", diplo_relation_decay(o->base, o->turns));
    } else if (strcmp(cmd, "ground_combat_resolve") == 0) {
        printf("%d\n", ground_combat_resolve(o->att, o->def, o->tech_a, o->tech_d));
    } else if (strcmp(cmd, "bomb_damage") == 0) {
        printf("%d\n", bomb_damage(o->bombs, o->shield, o->tech));
    } else if (strcmp(cmd, "ai_priority") == 0) {
        printf("%d\n", ai_priority(o->base, o->tech, o->bc, o->waste));
    } else if (strcmp(cmd, "rnd") == 0) {
        uint32_t seed = o->seed;
        int i;
        for (i = 0; i < o->count; ++i) {
            printf("%u%s", (unsigned)rnd_0_nm1(100, &seed), (i + 1 == o->count) ? "\n" : " ");
        }
    } else {
        return die_usage("unknown subcommand");
    }
    return 0;
}

int main(int argc, char **argv)
{
    opts_t opts;
    int i;
    if (argc < 2) {
        return die_usage("missing subcommand");
    }
    memset(&opts, 0, sizeof(opts));
    opts.max_pop = 100;
    opts.speed = 1;
    opts.seed = 42;
    opts.count = 1;
    for (i = 2; i < argc; ++i) {
        if (!parse_arg(&opts, argv[i])) {
            return die_usage(argv[i]);
        }
    }
    return run_cmd(argv[1], &opts);
}
