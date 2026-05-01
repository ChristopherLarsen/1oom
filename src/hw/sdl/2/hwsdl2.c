#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SDLMAIN
#include "SDL_main.h"
#endif

#include "SDL.h"
#include "SDL_keycode.h"

#include "hw.h"
#include "hwsdl_audio.h"
#include "hwsdl_mouse.h"
#include "hwsdl_opt.h"
#include "hwsdl_video.h"
#include "hwsdl2_video.h"
#include "kbd.h"
#include "log.h"
#include "main.h"
#include "mouse.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

const char *idstr_hw = "sdl2";

/* -------------------------------------------------------------------------- */

static mookey_t key_xlat_key[0x80];
static mookey_t key_xlat_scan[SDL_NUM_SCANCODES];
static char key_xlat_char[SDL_NUM_SCANCODES];

#define SDLK_TBLI_FROM_SCAN(i) ((i) & (~SDLK_SCANCODE_MASK))

static void build_key_xlat(void)
{
    memset(key_xlat_key, 0, sizeof(key_xlat_key));
    memset(key_xlat_scan, 0, sizeof(key_xlat_scan));
    memset(key_xlat_char, 0, sizeof(key_xlat_char));
    key_xlat_key[SDLK_BACKSPACE] = MOO_KEY_BACKSPACE;
    key_xlat_key[SDLK_TAB] = MOO_KEY_TAB;
    key_xlat_key[SDLK_RETURN] = MOO_KEY_RETURN;
    key_xlat_key[SDLK_ESCAPE] = MOO_KEY_ESCAPE;
    key_xlat_key[SDLK_SPACE] = MOO_KEY_SPACE;
    key_xlat_key[SDLK_EXCLAIM] = MOO_KEY_EXCLAIM;
    key_xlat_key[SDLK_QUOTEDBL] = MOO_KEY_QUOTEDBL;
    key_xlat_key[SDLK_HASH] = MOO_KEY_HASH;
    key_xlat_key[SDLK_DOLLAR] = MOO_KEY_DOLLAR;
    key_xlat_key[SDLK_AMPERSAND] = MOO_KEY_AMPERSAND;
    key_xlat_key[SDLK_QUOTE] = MOO_KEY_QUOTE;
    key_xlat_key[SDLK_LEFTPAREN] = MOO_KEY_LEFTPAREN;
    key_xlat_key[SDLK_RIGHTPAREN] = MOO_KEY_RIGHTPAREN;
    key_xlat_key[SDLK_ASTERISK] = MOO_KEY_ASTERISK;
    key_xlat_key[SDLK_PLUS] = MOO_KEY_PLUS;
    key_xlat_key[SDLK_COMMA] = MOO_KEY_COMMA;
    key_xlat_key[SDLK_MINUS] = MOO_KEY_MINUS;
    key_xlat_key[SDLK_PERIOD] = MOO_KEY_PERIOD;
    key_xlat_key[SDLK_SLASH] = MOO_KEY_SLASH;
    key_xlat_key[SDLK_0] = MOO_KEY_0;
    key_xlat_key[SDLK_1] = MOO_KEY_1;
    key_xlat_key[SDLK_2] = MOO_KEY_2;
    key_xlat_key[SDLK_3] = MOO_KEY_3;
    key_xlat_key[SDLK_4] = MOO_KEY_4;
    key_xlat_key[SDLK_5] = MOO_KEY_5;
    key_xlat_key[SDLK_6] = MOO_KEY_6;
    key_xlat_key[SDLK_7] = MOO_KEY_7;
    key_xlat_key[SDLK_8] = MOO_KEY_8;
    key_xlat_key[SDLK_9] = MOO_KEY_9;
    key_xlat_key[SDLK_COLON] = MOO_KEY_COLON;
    key_xlat_key[SDLK_SEMICOLON] = MOO_KEY_SEMICOLON;
    key_xlat_key[SDLK_LESS] = MOO_KEY_LESS;
    key_xlat_key[SDLK_EQUALS] = MOO_KEY_EQUALS;
    key_xlat_key[SDLK_GREATER] = MOO_KEY_GREATER;
    key_xlat_key[SDLK_QUESTION] = MOO_KEY_QUESTION;
    key_xlat_key[SDLK_AT] = MOO_KEY_AT;
    key_xlat_key[SDLK_LEFTBRACKET] = MOO_KEY_LEFTBRACKET;
    key_xlat_key[SDLK_BACKSLASH] = MOO_KEY_BACKSLASH;
    key_xlat_key[SDLK_RIGHTBRACKET] = MOO_KEY_RIGHTBRACKET;
    key_xlat_key[SDLK_CARET] = MOO_KEY_CARET;
    key_xlat_key[SDLK_UNDERSCORE] = MOO_KEY_UNDERSCORE;
    key_xlat_key[SDLK_BACKQUOTE] = MOO_KEY_BACKQUOTE;
    key_xlat_key[SDLK_a] = MOO_KEY_a;
    key_xlat_key[SDLK_b] = MOO_KEY_b;
    key_xlat_key[SDLK_c] = MOO_KEY_c;
    key_xlat_key[SDLK_d] = MOO_KEY_d;
    key_xlat_key[SDLK_e] = MOO_KEY_e;
    key_xlat_key[SDLK_f] = MOO_KEY_f;
    key_xlat_key[SDLK_g] = MOO_KEY_g;
    key_xlat_key[SDLK_h] = MOO_KEY_h;
    key_xlat_key[SDLK_i] = MOO_KEY_i;
    key_xlat_key[SDLK_j] = MOO_KEY_j;
    key_xlat_key[SDLK_k] = MOO_KEY_k;
    key_xlat_key[SDLK_l] = MOO_KEY_l;
    key_xlat_key[SDLK_m] = MOO_KEY_m;
    key_xlat_key[SDLK_n] = MOO_KEY_n;
    key_xlat_key[SDLK_o] = MOO_KEY_o;
    key_xlat_key[SDLK_p] = MOO_KEY_p;
    key_xlat_key[SDLK_q] = MOO_KEY_q;
    key_xlat_key[SDLK_r] = MOO_KEY_r;
    key_xlat_key[SDLK_s] = MOO_KEY_s;
    key_xlat_key[SDLK_t] = MOO_KEY_t;
    key_xlat_key[SDLK_u] = MOO_KEY_u;
    key_xlat_key[SDLK_v] = MOO_KEY_v;
    key_xlat_key[SDLK_w] = MOO_KEY_w;
    key_xlat_key[SDLK_x] = MOO_KEY_x;
    key_xlat_key[SDLK_y] = MOO_KEY_y;
    key_xlat_key[SDLK_z] = MOO_KEY_z;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_CAPSLOCK)] = MOO_KEY_CAPSLOCK;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F1)] = MOO_KEY_F1;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F2)] = MOO_KEY_F2;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F3)] = MOO_KEY_F3;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F4)] = MOO_KEY_F4;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F5)] = MOO_KEY_F5;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F6)] = MOO_KEY_F6;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F7)] = MOO_KEY_F7;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F8)] = MOO_KEY_F8;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F9)] = MOO_KEY_F9;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F10)] = MOO_KEY_F10;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F11)] = MOO_KEY_F11;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F12)] = MOO_KEY_F12;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_SCROLLLOCK)] = MOO_KEY_SCROLLOCK;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAUSE)] = MOO_KEY_PAUSE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_INSERT)] = MOO_KEY_INSERT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_HOME)] = MOO_KEY_HOME;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAGEUP)] = MOO_KEY_PAGEUP;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_DELETE)] = MOO_KEY_DELETE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_END)] = MOO_KEY_END;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PAGEDOWN)] = MOO_KEY_PAGEDOWN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RIGHT)] = MOO_KEY_RIGHT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LEFT)] = MOO_KEY_LEFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_DOWN)] = MOO_KEY_DOWN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_UP)] = MOO_KEY_UP;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_CLEAR)] = MOO_KEY_CLEAR;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F13)] = MOO_KEY_F13;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F14)] = MOO_KEY_F14;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_F15)] = MOO_KEY_F15;

    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_0)] = MOO_KEY_KP0;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_1)] = MOO_KEY_KP1;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_2)] = MOO_KEY_KP2;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_3)] = MOO_KEY_KP3;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_4)] = MOO_KEY_KP4;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_5)] = MOO_KEY_KP5;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_6)] = MOO_KEY_KP6;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_7)] = MOO_KEY_KP7;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_8)] = MOO_KEY_KP8;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_9)] = MOO_KEY_KP9;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_PERIOD)] = MOO_KEY_KP_PERIOD;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_DIVIDE)] = MOO_KEY_KP_DIVIDE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_MULTIPLY)] = MOO_KEY_KP_MULTIPLY;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_MINUS)] = MOO_KEY_KP_MINUS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_PLUS)] = MOO_KEY_KP_PLUS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_ENTER)] = MOO_KEY_RETURN;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_KP_EQUALS)] = MOO_KEY_KP_EQUALS;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_NUMLOCKCLEAR)] = MOO_KEY_NUMLOCK;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RSHIFT)] = MOO_KEY_RSHIFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LSHIFT)] = MOO_KEY_LSHIFT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RCTRL)] = MOO_KEY_RCTRL;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LCTRL)] = MOO_KEY_LCTRL;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RALT)] = MOO_KEY_RALT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LALT)] = MOO_KEY_LALT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_LGUI)] = MOO_KEY_LSUPER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_RGUI)] = MOO_KEY_RSUPER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_MODE)] = MOO_KEY_MODE;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_HELP)] = MOO_KEY_HELP;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_PRINTSCREEN)] = MOO_KEY_PRINT;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_SYSREQ)] = MOO_KEY_SYSREQ;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_MENU)] = MOO_KEY_MENU;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_POWER)] = MOO_KEY_POWER;
    key_xlat_scan[SDLK_TBLI_FROM_SCAN(SDLK_UNDO)] = MOO_KEY_UNDO;

    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_PERIOD)] = SDLK_PERIOD;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_DIVIDE)] = SDLK_SLASH;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_MULTIPLY)] = SDLK_ASTERISK;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_MINUS)] = SDLK_MINUS;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_PLUS)] = SDLK_PLUS;
    key_xlat_char[SDLK_TBLI_FROM_SCAN(SDLK_KP_EQUALS)] = SDLK_EQUALS;
}

static inline uint32_t mod_xlat(SDL_Keymod smod)
{
    uint32_t mod = 0;
    if (smod & KMOD_SHIFT) { mod |= MOO_MOD_SHIFT; }
    if (smod & KMOD_ALT) { mod |= MOO_MOD_ALT; }
    if (smod & KMOD_CTRL) { mod |= MOO_MOD_CTRL; }
    return mod;
}

static bool hw_textinput_active = false;
static bool hw_key_press_repeat_active = true;
extern const char *os_get_path_user(void);
static SDL_Keycode hw_screenshot_key = SDLK_F12;
static int hw_screenshot_count = 0;

/* ------------------------------------------------------------------------- */
/* agent-orion patch: env-driven auto-screenshot for autonomous capture.
   Lets Claude Code (or any non-interactive caller) grab a screenshot of the
   current 1oom screen without needing macOS Screen Recording or Accessibility
   permissions to send F12 to the SDL2 window.

   Env vars:
     MOO_AUTOSHOT_FRAMES=N   take a shot after N hw_event_handle() ticks (~60/s)
                             default: disabled (-1)
     MOO_AUTOSHOT_PATH=PATH  absolute path of the PNG to write (overrides the
                             default ~/.config/1oom/shots/shotNNNN.png)
                             default: unset → use the standard manual-shot path
     MOO_AUTOSHOT_EXIT=0     stay running after the shot (default: exit cleanly)

   Typical use:
     MOO_AUTOSHOT_FRAMES=180 \
     MOO_AUTOSHOT_PATH=/abs/path/to/Reference\ Screens/Orion\ Intro.png \
       ./1oom_classic_sdl2 -data ./MOO -nolog -cro
*/
static int hw_auto_shot_remaining = -1;
static const char *hw_auto_shot_path = NULL;
static int hw_auto_shot_exit_after = 1;
/* DRG agent-orion: hw_auto_shot_armed / hw_auto_shot_fired live in main.c
   so they exist for any HW backend (cmdline, sdl1, sdl2). hwsdl2 sets them. */
extern int hw_auto_shot_armed;
extern int hw_auto_shot_fired;
extern int hw_video_screenshot_png(const char *filename);

/* -------------------------------------------------------------------------- */

#define SDL1or2Key  SDL_Keycode
#define SDL1or2Mod  SDL_Keymod

#include "hwsdl.c"

/* -------------------------------------------------------------------------- */

static void hw_event_handle_window(SDL_WindowEvent *e)
{
    switch (e->event) {
        case SDL_WINDOWEVENT_RESIZED:
            hw_video_resize(0, 0);
            break;
        case SDL_WINDOWEVENT_EXPOSED:
            hw_video_update();
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            hw_mouse_ungrab();
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            if (!hw_opt_relmouse && hw_opt_nograbmouse) {
                hw_mouse_grab();
            }
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            hw_video_set_visible(false);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            hw_video_set_visible(true);
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */

enum replay_op_e {
    REPLAY_OP_LOAD,
    REPLAY_OP_WAIT,
    REPLAY_OP_KEY,
    REPLAY_OP_KEYDOWN,
    REPLAY_OP_KEYUP,
    REPLAY_OP_CLICK,
    REPLAY_OP_RCLICK,
    REPLAY_OP_MOVE,
    REPLAY_OP_SCREENSHOT,
    REPLAY_OP_ASSERT_TURN,
    REPLAY_OP_END
};

struct replay_key_s {
    mookey_t key;
    char c;
};

struct replay_op_s {
    enum replay_op_e op;
    int line;
    int a;
    int b;
    char *text;
    struct replay_key_s key;
};

static struct replay_state_s {
    bool active;
    struct replay_op_s *ops;
    int num_ops;
    int cap_ops;
    int pos;
    int wait_frames;
    char **keymap_names;
    int num_keymap_names;
} replay;

static void replay_free(void)
{
    for (int i = 0; i < replay.num_ops; ++i) {
        free(replay.ops[i].text);
    }
    free(replay.ops);
    replay.ops = NULL;
    replay.num_ops = 0;
    replay.cap_ops = 0;
    for (int i = 0; i < replay.num_keymap_names; ++i) {
        free(replay.keymap_names[i]);
    }
    free(replay.keymap_names);
    replay.keymap_names = NULL;
    replay.num_keymap_names = 0;
}

static char *replay_strdup(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (out != NULL) {
        memcpy(out, s, len + 1);
    }
    return out;
}

static char *replay_concat3(const char *a, const char *b, const char *c)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    size_t clen = strlen(c);
    char *out = malloc(alen + blen + clen + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, a, alen);
    memcpy(out + alen, b, blen);
    memcpy(out + alen + blen, c, clen + 1);
    return out;
}

static char *replay_dirname(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if ((slash == NULL) || ((backslash != NULL) && (backslash > slash))) {
        slash = backslash;
    }
#endif
    if (slash == NULL) {
        return replay_strdup(".");
    }
    {
        size_t len = (size_t)(slash - path);
        char *out = malloc(len + 1);
        if (out == NULL) {
            return NULL;
        }
        memcpy(out, path, len);
        out[len] = '\0';
        return out;
    }
}

static FILE *replay_open_existing(const char *path)
{
    return fopen(path, "r");
}

static char *replay_find_keymap(const char *replay_filename)
{
    FILE *fd;
    char *dir;
    char *candidate;

    fd = replay_open_existing("tools/replay/keymap.txt");
    if (fd != NULL) {
        fclose(fd);
        return replay_strdup("tools/replay/keymap.txt");
    }
    dir = replay_dirname(replay_filename);
    if (dir == NULL) {
        return NULL;
    }
    candidate = replay_concat3(dir, "/", "keymap.txt");
    if (candidate != NULL) {
        fd = replay_open_existing(candidate);
        if (fd != NULL) {
            fclose(fd);
            free(dir);
            return candidate;
        }
        free(candidate);
    }
    candidate = replay_concat3(dir, "/../", "keymap.txt");
    if (candidate != NULL) {
        fd = replay_open_existing(candidate);
        if (fd != NULL) {
            fclose(fd);
            free(dir);
            return candidate;
        }
        free(candidate);
    }
    free(dir);
    return NULL;
}

static int replay_load_keymap(const char *replay_filename)
{
    char line[256];
    char *keymap_path = replay_find_keymap(replay_filename);
    FILE *fd;
    if (keymap_path == NULL) {
        log_error("Replay: could not find tools/replay/keymap.txt\n");
        return -1;
    }
    fd = fopen(keymap_path, "r");
    if (fd == NULL) {
        log_error("Replay: could not open keymap '%s'\n", keymap_path);
        free(keymap_path);
        return -1;
    }
    while (fgets(line, sizeof(line), fd) != NULL) {
        char *sdl_name;
        char *godot_name;
        char **names_new;
        line[strcspn(line, "\r\n")] = '\0';
        sdl_name = strtok(line, " \t");
        if ((sdl_name == NULL) || (sdl_name[0] == '#')) {
            continue;
        }
        godot_name = strtok(NULL, " \t");
        if (godot_name == NULL) {
            log_error("Replay: malformed keymap '%s'\n", keymap_path);
            fclose(fd);
            free(keymap_path);
            return -1;
        }
        names_new = realloc(replay.keymap_names, sizeof(*names_new) * (replay.num_keymap_names + 1));
        if (names_new == NULL) {
            fclose(fd);
            free(keymap_path);
            return -1;
        }
        replay.keymap_names = names_new;
        replay.keymap_names[replay.num_keymap_names] = replay_strdup(sdl_name);
        if (replay.keymap_names[replay.num_keymap_names] == NULL) {
            fclose(fd);
            free(keymap_path);
            return -1;
        }
        ++replay.num_keymap_names;
    }
    fclose(fd);
    free(keymap_path);
    return 0;
}

static bool replay_keymap_has(const char *name)
{
    for (int i = 0; i < replay.num_keymap_names; ++i) {
        if (strcmp(name, replay.keymap_names[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool replay_key_lookup(const char *name, struct replay_key_s *out)
{
    out->c = 0;
    if ((name[0] >= 'A') && (name[0] <= 'Z') && (name[1] == '\0')) {
        out->key = (mookey_t)(MOO_KEY_a + (name[0] - 'A'));
        out->c = (char)tolower((unsigned char)name[0]);
        return true;
    }
    if ((name[0] >= '0') && (name[0] <= '9') && (name[1] == '\0')) {
        out->key = (mookey_t)(MOO_KEY_0 + (name[0] - '0'));
        out->c = name[0];
        return true;
    }
    if ((name[0] == 'F') && isdigit((unsigned char)name[1])) {
        char *endptr = NULL;
        long fnum = strtol(name + 1, &endptr, 10);
        if ((endptr != NULL) && (*endptr == '\0') && (fnum >= 1) && (fnum <= 12)) {
            out->key = (mookey_t)(MOO_KEY_F1 + fnum - 1);
            return true;
        }
    }
    if (strcmp(name, "ESCAPE") == 0) {
        out->key = MOO_KEY_ESCAPE;
        out->c = (char)MOO_KEY_ESCAPE;
        return true;
    }
    if (strcmp(name, "RETURN") == 0) {
        out->key = MOO_KEY_RETURN;
        out->c = (char)MOO_KEY_RETURN;
        return true;
    }
    if (strcmp(name, "UP") == 0) {
        out->key = MOO_KEY_UP;
        return true;
    }
    if (strcmp(name, "DOWN") == 0) {
        out->key = MOO_KEY_DOWN;
        return true;
    }
    if (strcmp(name, "LEFT") == 0) {
        out->key = MOO_KEY_LEFT;
        return true;
    }
    if (strcmp(name, "RIGHT") == 0) {
        out->key = MOO_KEY_RIGHT;
        return true;
    }
    if (strcmp(name, "SLASH") == 0) {
        out->key = MOO_KEY_SLASH;
        out->c = '/';
        return true;
    }
    return false;
}

static SDL_Keycode replay_key_to_sdl(const char *name)
{
    if ((name[0] >= 'A') && (name[0] <= 'Z') && (name[1] == '\0')) {
        return SDLK_a + (name[0] - 'A');
    }
    if ((name[0] >= '0') && (name[0] <= '9') && (name[1] == '\0')) {
        return SDLK_0 + (name[0] - '0');
    }
    if ((name[0] == 'F') && isdigit((unsigned char)name[1])) {
        char *endptr = NULL;
        long fnum = strtol(name + 1, &endptr, 10);
        if ((endptr != NULL) && (*endptr == '\0') && (fnum >= 1) && (fnum <= 12)) {
            return SDLK_F1 + fnum - 1;
        }
    }
    if (strcmp(name, "ESCAPE") == 0) {
        return SDLK_ESCAPE;
    }
    if (strcmp(name, "RETURN") == 0) {
        return SDLK_RETURN;
    }
    if (strcmp(name, "UP") == 0) {
        return SDLK_UP;
    }
    if (strcmp(name, "DOWN") == 0) {
        return SDLK_DOWN;
    }
    if (strcmp(name, "LEFT") == 0) {
        return SDLK_LEFT;
    }
    if (strcmp(name, "RIGHT") == 0) {
        return SDLK_RIGHT;
    }
    if (strcmp(name, "SLASH") == 0) {
        return SDLK_SLASH;
    }
    return SDLK_UNKNOWN;
}

static char *replay_screenshot_path(const char *path)
{
    const char *slash;

    if ((main_autoshot_dir != NULL) && (main_autoshot_dir[0] != '\0')) {
        slash = strrchr(path, '/');
#ifdef _WIN32
        {
            const char *backslash = strrchr(path, '\\');
            if ((slash == NULL) || ((backslash != NULL) && (backslash > slash))) {
                slash = backslash;
            }
        }
#endif
        return replay_concat3(main_autoshot_dir, "/", (slash != NULL) ? slash + 1 : path);
    }
    return replay_strdup(path);
}

static char *hw_manual_screenshot_path(void)
{
    char name[64];
    const char *dir;

    ++hw_screenshot_count;
    snprintf(name, sizeof(name), "shot%04i.png", hw_screenshot_count);
    if ((main_autoshot_dir != NULL) && (main_autoshot_dir[0] != '\0')) {
        return replay_concat3(main_autoshot_dir, "/", name);
    }
    dir = os_get_path_user();
    return replay_concat3(dir, "/shots/", name);
}

static void hw_manual_screenshot(void)
{
    char *path = hw_manual_screenshot_path();
    if (path == NULL) {
        log_error("Replay: could not allocate screenshot path\n");
        return;
    }
    if (hw_video_screenshot_png(path) == 0) {
        log_message("Screenshot: wrote '%s'\n", path);
    }
    free(path);
}

static int replay_add_op(struct replay_op_s *op)
{
    if (replay.num_ops >= replay.cap_ops) {
        int cap_new = (replay.cap_ops == 0) ? 32 : replay.cap_ops * 2;
        struct replay_op_s *ops_new = realloc(replay.ops, sizeof(*ops_new) * cap_new);
        if (ops_new == NULL) {
            return -1;
        }
        replay.ops = ops_new;
        replay.cap_ops = cap_new;
    }
    replay.ops[replay.num_ops++] = *op;
    return 0;
}

static int replay_parse_int(const char *text, int *out)
{
    char *endptr = NULL;
    long value = strtol(text, &endptr, 10);
    if ((endptr == NULL) || (*endptr != '\0') || (value < 0) || (value > 1000000000L)) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static int replay_split_line(char *line, char **tokens, int max_tokens)
{
    int count = 0;
    char *p = line;
    while (*p != '\0') {
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }
        if ((*p == '\0') || (*p == '#')) {
            break;
        }
        if (count >= max_tokens) {
            return -1;
        }
        if (*p == '"') {
            tokens[count++] = ++p;
            while ((*p != '\0') && (*p != '"')) {
                ++p;
            }
            if (*p != '"') {
                return -1;
            }
            *p++ = '\0';
        } else {
            tokens[count++] = p;
            while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != '#')) {
                ++p;
            }
            if (*p == '#') {
                *p = '\0';
                break;
            }
            if (*p != '\0') {
                *p++ = '\0';
            }
        }
    }
    return count;
}

static int replay_parse_file(const char *filename)
{
    FILE *fd;
    char line[1024];
    bool header_seen = false;
    bool load_seen = false;
    bool end_seen = false;
    bool held_keys[256] = { false };
    int line_no = 0;

    if (replay_load_keymap(filename) != 0) {
        return -1;
    }
    fd = fopen(filename, "r");
    if (fd == NULL) {
        log_error("Replay: could not open '%s'\n", filename);
        return -1;
    }
    while (fgets(line, sizeof(line), fd) != NULL) {
        char *tokens[4];
        int token_count;
        struct replay_op_s op;
        ++line_no;
        if (strchr(line, '\n') == NULL && !feof(fd)) {
            log_error("Replay: %s:%i line is too long\n", filename, line_no);
            fclose(fd);
            return -1;
        }
        line[strcspn(line, "\r\n")] = '\0';
        {
            char *p = line;
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
        }
        token_count = replay_split_line(line, tokens, 4);
        if (token_count < 0) {
            log_error("Replay: %s:%i cannot parse line\n", filename, line_no);
            fclose(fd);
            return -1;
        }
        if (token_count == 0) {
            continue;
        }
        if (end_seen) {
            log_error("Replay: %s:%i operation appears after END\n", filename, line_no);
            fclose(fd);
            return -1;
        }
        memset(&op, 0, sizeof(op));
        op.line = line_no;
        if (strcmp(tokens[0], "LOAD") == 0) {
            if (token_count != 2 || load_seen) {
                log_error("Replay: %s:%i malformed LOAD\n", filename, line_no);
                fclose(fd);
                return -1;
            }
            {
                FILE *load_fd = fopen(tokens[1], "rb");
                if (load_fd == NULL) {
                    log_error("Replay: %s:%i LOAD fixture does not exist: %s\n", filename, line_no, tokens[1]);
                    fclose(fd);
                    return -1;
                }
                fclose(load_fd);
            }
            op.op = REPLAY_OP_LOAD;
            op.text = replay_strdup(tokens[1]);
            load_seen = true;
        } else if (strcmp(tokens[0], "WAIT") == 0 || strcmp(tokens[0], "ASSERT_TURN") == 0) {
            if (token_count != 2 || replay_parse_int(tokens[1], &op.a) != 0) {
                log_error("Replay: %s:%i malformed %s\n", filename, line_no, tokens[0]);
                fclose(fd);
                return -1;
            }
            op.op = (strcmp(tokens[0], "WAIT") == 0) ? REPLAY_OP_WAIT : REPLAY_OP_ASSERT_TURN;
        } else if (strcmp(tokens[0], "KEY") == 0 || strcmp(tokens[0], "KEYDOWN") == 0 || strcmp(tokens[0], "KEYUP") == 0) {
            if (token_count != 2 || !replay_keymap_has(tokens[1]) || !replay_key_lookup(tokens[1], &op.key)) {
                log_error("Replay: %s:%i unknown or unsupported key '%s'\n", filename, line_no, token_count >= 2 ? tokens[1] : "");
                fclose(fd);
                return -1;
            }
            if (strcmp(tokens[0], "KEY") == 0) {
                op.op = REPLAY_OP_KEY;
                if (held_keys[op.key.key]) {
                    log_error("Replay: %s:%i KEY used while key is held\n", filename, line_no);
                    fclose(fd);
                    return -1;
                }
            } else if (strcmp(tokens[0], "KEYDOWN") == 0) {
                op.op = REPLAY_OP_KEYDOWN;
                if (held_keys[op.key.key]) {
                    log_error("Replay: %s:%i KEYDOWN repeats a held key\n", filename, line_no);
                    fclose(fd);
                    return -1;
                }
                held_keys[op.key.key] = true;
            } else {
                op.op = REPLAY_OP_KEYUP;
                if (!held_keys[op.key.key]) {
                    log_error("Replay: %s:%i KEYUP has no matching KEYDOWN\n", filename, line_no);
                    fclose(fd);
                    return -1;
                }
                held_keys[op.key.key] = false;
            }
        } else if (strcmp(tokens[0], "CLICK") == 0 || strcmp(tokens[0], "RCLICK") == 0 || strcmp(tokens[0], "MOVE") == 0) {
            if (token_count != 3 || replay_parse_int(tokens[1], &op.a) != 0 || replay_parse_int(tokens[2], &op.b) != 0
              || op.a < 0 || op.a >= 320 || op.b < 0 || op.b >= 200) {
                log_error("Replay: %s:%i malformed coordinates\n", filename, line_no);
                fclose(fd);
                return -1;
            }
            if (strcmp(tokens[0], "CLICK") == 0) {
                op.op = REPLAY_OP_CLICK;
            } else if (strcmp(tokens[0], "RCLICK") == 0) {
                op.op = REPLAY_OP_RCLICK;
            } else {
                op.op = REPLAY_OP_MOVE;
            }
        } else if (strcmp(tokens[0], "SCREENSHOT") == 0) {
            size_t len;
            if (token_count != 2) {
                log_error("Replay: %s:%i malformed SCREENSHOT\n", filename, line_no);
                fclose(fd);
                return -1;
            }
            len = strlen(tokens[1]);
            if (len < 5 || strcmp(tokens[1] + len - 4, ".png") != 0) {
                log_error("Replay: %s:%i SCREENSHOT path must end in .png\n", filename, line_no);
                fclose(fd);
                return -1;
            }
            op.op = REPLAY_OP_SCREENSHOT;
            op.text = replay_strdup(tokens[1]);
        } else if (strcmp(tokens[0], "END") == 0) {
            if (token_count != 1) {
                log_error("Replay: %s:%i malformed END\n", filename, line_no);
                fclose(fd);
                return -1;
            }
            op.op = REPLAY_OP_END;
            end_seen = true;
        } else {
            log_error("Replay: %s:%i unknown op '%s'\n", filename, line_no, tokens[0]);
            fclose(fd);
            return -1;
        }
        if (!load_seen && op.op != REPLAY_OP_LOAD) {
            log_error("Replay: %s:%i %s appears before LOAD\n", filename, line_no, tokens[0]);
            fclose(fd);
            return -1;
        }
        if ((op.text == NULL) && (op.op == REPLAY_OP_LOAD || op.op == REPLAY_OP_SCREENSHOT)) {
            fclose(fd);
            return -1;
        }
        if (replay_add_op(&op) != 0) {
            log_error("Replay: could not allocate op list\n");
            free(op.text);
            fclose(fd);
            return -1;
        }
    }
    fclose(fd);
    if (!header_seen) {
        log_error("Replay: %s: missing supported header comment: # 1oom-godot-replay v1\n", filename);
        return -1;
    }
    if (!load_seen) {
        log_error("Replay: %s: missing LOAD operation\n", filename);
        return -1;
    }
    if (!end_seen) {
        log_error("Replay: %s: missing END operation\n", filename);
        return -1;
    }
    for (int i = 0; i < 256; ++i) {
        if (held_keys[i]) {
            log_error("Replay: %s: keys still pressed at END\n", filename);
            return -1;
        }
    }
    replay.active = true;
    replay.pos = 0;
    replay.wait_frames = -1;
    return 0;
}

static void replay_key_event(struct replay_key_s key, bool down)
{
    if (down) {
        kbd_add_keypress(key.key, 0, key.c);
    }
    kbd_set_pressed(key.key, 0, down);
}

static void replay_mouse_move(int x, int y)
{
    mouse_set_xy_from_hw(x, y);
}

static void replay_mouse_click(int x, int y, int button)
{
    replay_mouse_move(x, y);
    hw_mouse_grab();
    hw_mouse_button(button, true);
    hw_mouse_button(button, false);
}

static void replay_step(void)
{
    if (!replay.active) {
        return;
    }
    while (replay.pos < replay.num_ops) {
        struct replay_op_s *op = &replay.ops[replay.pos];
        switch (op->op) {
            case REPLAY_OP_LOAD:
                ++replay.pos;
                break;
            case REPLAY_OP_WAIT:
                if (replay.wait_frames < 0) {
                    replay.wait_frames = op->a;
                }
                if (replay.wait_frames > 0) {
                    --replay.wait_frames;
                    return;
                }
                replay.wait_frames = -1;
                ++replay.pos;
                break;
            case REPLAY_OP_KEY:
                replay_key_event(op->key, true);
                replay_key_event(op->key, false);
                ++replay.pos;
                return;
            case REPLAY_OP_KEYDOWN:
                replay_key_event(op->key, true);
                ++replay.pos;
                return;
            case REPLAY_OP_KEYUP:
                replay_key_event(op->key, false);
                ++replay.pos;
                return;
            case REPLAY_OP_CLICK:
                replay_mouse_click(op->a, op->b, SDL_BUTTON_LEFT);
                ++replay.pos;
                return;
            case REPLAY_OP_RCLICK:
                replay_mouse_click(op->a, op->b, SDL_BUTTON_RIGHT);
                ++replay.pos;
                return;
            case REPLAY_OP_MOVE:
                replay_mouse_move(op->a, op->b);
                ++replay.pos;
                return;
            case REPLAY_OP_SCREENSHOT:
                {
                    char *path = replay_screenshot_path(op->text);
                    if (path == NULL || hw_video_screenshot_png(path) != 0) {
                        free(path);
                        exit(EXIT_FAILURE);
                    }
                    free(path);
                }
                ++replay.pos;
                break;
            case REPLAY_OP_ASSERT_TURN:
                if (main_replay_current_year != op->a) {
                    log_error("Replay: %s:%i ASSERT_TURN expected %i, got %i\n", main_replay_file, op->line, op->a, main_replay_current_year);
                    exit(EXIT_FAILURE);
                }
                ++replay.pos;
                break;
            case REPLAY_OP_END:
                log_message("Replay: END\n");
                exit(EXIT_SUCCESS);
                break;
        }
    }
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    return main_1oom(argc, argv);
}

int hw_early_init(void)
{
    return 0;
}

int hw_init(void)
{
    int flags = SDL_INIT_VIDEO | (opt_audio_enabled ? SDL_INIT_AUDIO : 0);
    log_message("SDL_Init\n");
    if (SDL_Init(flags) < 0) {
        log_error("SDL_Init(0x%x) failed: %s\n", flags, SDL_GetError());
        return 11;
    }
    if (hw_audio_init()) {
        return 12;
    }
    build_key_xlat();
    hw_screenshot_key = replay_key_to_sdl(main_shotkey_name);
    if (hw_screenshot_key == SDLK_UNKNOWN) {
        log_error("option '-shotkey' has unsupported key '%s'\n", main_shotkey_name);
        return 13;
    }
    if (main_replay_file != NULL) {
        if (replay_parse_file(main_replay_file) != 0) {
            replay_free();
            return 13;
        }
        log_message("Replay: loaded '%s'\n", main_replay_file);
    }
    if (hw_opt_relmouse) {
        SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    }
    /* agent-orion patch: configure env-driven auto-screenshot. */
    {
        const char *s = getenv("MOO_AUTOSHOT_FRAMES");
        if (s && s[0]) {
            int n = atoi(s);
            if (n > 0) {
                hw_auto_shot_remaining = n;
                hw_auto_shot_armed = 1;
                hw_auto_shot_path = getenv("MOO_AUTOSHOT_PATH");
                if (hw_auto_shot_path && hw_auto_shot_path[0] == '\0') {
                    hw_auto_shot_path = NULL;
                }
                {
                    const char *exitstr = getenv("MOO_AUTOSHOT_EXIT");
                    if (exitstr && exitstr[0] == '0') {
                        hw_auto_shot_exit_after = 0;
                    }
                }
                log_message("AutoShot: armed for %d frames%s%s%s\n", n,
                    hw_auto_shot_path ? ", path='" : "",
                    hw_auto_shot_path ? hw_auto_shot_path : "",
                    hw_auto_shot_path ? "'" : "");
            }
        }
    }
    return 0;
}

void hw_shutdown(void)
{
    replay_free();
    hw_audio_shutdown();
    hw_video_shutdown();
    log_message("SDL_Quit\n");
    SDL_Quit();
}

void hw_textinput_start(void)
{
    SDL_StartTextInput();
    hw_textinput_active = true;
}

void hw_textinput_stop(void)
{
    SDL_StopTextInput();
    hw_textinput_active = false;
}

int hw_event_handle(void)
{
    SDL_Event e;

    /* agent-orion patch: env-driven auto-screenshot countdown. */
    if (hw_auto_shot_remaining > 0) {
        --hw_auto_shot_remaining;
        if (hw_auto_shot_remaining == 0) {
            int rc;
            if (hw_auto_shot_path != NULL) {
                rc = hw_video_screenshot_png(hw_auto_shot_path);
                if (rc == 0) {
                    log_message("AutoShot: wrote '%s'\n", hw_auto_shot_path);
                    hw_auto_shot_fired = 1;
                } else {
                    log_error("AutoShot: failed to write '%s' (rc=%d)\n", hw_auto_shot_path, rc);
                }
            } else {
                hw_manual_screenshot();
                hw_auto_shot_fired = 1;
            }
            if (hw_auto_shot_exit_after) {
                log_message("AutoShot: exiting cleanly\n");
                exit(0);
            }
            hw_auto_shot_remaining = -1;
        }
    }

    replay_step();
    SDL_PumpEvents();

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            uint32_t mod;
            case SDL_KEYDOWN:
                if (e.key.repeat && !hw_key_press_repeat_active) {
                    break;
                }
                if ((e.key.keysym.sym == hw_screenshot_key) && (mod_xlat(e.key.keysym.mod) == 0)) {
                    hw_manual_screenshot();
                    break;
                }
                {
                    SDL_Keycode sym;
                    SDL_Keymod smod;
                    char c;
                    sym = e.key.keysym.sym;
                    smod = e.key.keysym.mod;
                    c = 0;
                    if (!(hw_kbd_check_hotkey(sym, smod, c))) {
                        mookey_t key;
                        if (sym & SDLK_SCANCODE_MASK) {
                            key = key_xlat_scan[SDLK_TBLI_FROM_SCAN(sym)];
                            c = 0;
                            if (!hw_textinput_active) {
                                c = key_xlat_char[SDLK_TBLI_FROM_SCAN(sym)];
                            }
                        } else {
                            key = key_xlat_key[sym];
                            c = (char)sym; /* TODO SDL 2 */
                            /* ignore ASCII range when expecting SDL_TEXTINPUT */
                            if (hw_textinput_active && ((key >= MOO_KEY_SPACE) && (key <= MOO_KEY_z))) {
                                key = MOO_KEY_LAST;
                            }
                        }
                        mod = mod_xlat(smod);
                        if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) mod |= MOO_MOD_CTRL;
                        if ((key != MOO_KEY_UNKNOWN) && (key < MOO_KEY_LAST)) {
                            kbd_add_keypress(key, mod, c);
                        }
                        kbd_set_pressed(key, mod, true);
                    }
                }
                break;
            case SDL_KEYUP:
                {
                    SDL_Keycode sym;
                    SDL_Keymod smod;
                    mookey_t key;
                    sym = e.key.keysym.sym;
                    smod = e.key.keysym.mod;
                    mod = mod_xlat(smod);
                    if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) mod &= ~MOO_MOD_CTRL;
                    if (sym & SDLK_SCANCODE_MASK) {
                        key = key_xlat_scan[SDLK_TBLI_FROM_SCAN(sym)];
                    } else {
                        key = key_xlat_key[sym];
                    }
                    kbd_set_pressed(key, mod, false);
                }
                break;
            case SDL_TEXTINPUT:
                if (hw_textinput_active && (e.text.text[0] != 0)) {
                    char c = e.text.text[0];
                    SDL_StopTextInput();
                    SDL_StartTextInput();
                    kbd_add_keypress(MOO_KEY_UNKNOWN, 0, c);
                }
                break;
            case SDL_MOUSEMOTION:
                if (!hw_opt_relmouse && hw_mouse_enabled) {
                    int x, y;
                    x = e.motion.x;
                    y = e.motion.y;
                    if (hw_opt_aspect_ratio_correct) {
                        y = (y * 5 + 5) / 6;
                    }
                    mouse_set_xy_from_hw(x, y);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                hw_mouse_button((int)(e.button.button), (e.button.state == SDL_PRESSED));
                break;
            case SDL_MOUSEWHEEL:
                if (e.wheel.y != 0) {
                    hw_mouse_scroll((e.wheel.y > 0) ? -1 : 1);
                }
                break;
            case SDL_QUIT:
                hw_audio_shutdown_pre();
                exit(EXIT_SUCCESS);
                break;
            case SDL_WINDOWEVENT:
                if (e.window.windowID == hw_video_get_window_id()) {
                    hw_event_handle_window(&e.window);
                }
            default:
                break;
        }
    }
    if (hw_opt_relmouse) {
        int x, y;
        SDL_GetRelativeMouseState(&x, &y);
        if (hw_mouse_enabled) {
            if ((x != 0) || (y != 0)) {
                hw_mouse_move(x, y);
            }
        }
    }

    if (hw_audio_check_process()) {
        exit(EXIT_FAILURE);
    }

    SDL_Delay(10);
    return 0;
}

bool hw_kbd_set_repeat(bool enabled)
{
    hw_key_press_repeat_active = enabled;
    return true;
}
