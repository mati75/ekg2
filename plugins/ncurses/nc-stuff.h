#ifndef __EKG_NCURSES_NC_STUFF_H
#define __EKG_NCURSES_NC_STUFF_H

#include "ecurses.h"

void ncurses_init(void);
void ncurses_deinit(void);

extern plugin_t ncurses_plugin;

// int ncurses_resize_term;

extern int ncurses_plugin_destroyed;

#define LINE_MAXLEN 1000		/* max line length */
#define MULTILINE_INPUT_SIZE 5

#define ncurses_current ((ncurses_window_t *) window_current->priv_data)

void update_statusbar(int commit);

struct screen_line { /* everything locale-encoded */
	int len;		/* line length */
	
	unsigned char *str;		/* content */
	fstr_attr_t *attr;		/* attributes */
	
	unsigned char *prompt_str;	/* prompt string */
	fstr_attr_t *prompt_attr;	/* prompt attributes */
	int prompt_len;		/* prompt length */
	
	char *ts;		/* timestamp */
	fstr_attr_t *ts_attr;	/* attributes of the timestamp */

	int backlog;		/* backlog line it comes from */
	int margin_left;	/* where the margin should be setted */	
};

enum window_frame_t {
	WF_LEFT = 1,
	WF_TOP = 2,
	WF_RIGHT = 4,
	WF_BOTTOM = 8,
	WF_ALL = 15
};

typedef struct {
	WINDOW *window;		/* WINDOW of a window */

		/* -- the input prompt -- */
	gchar *prompt;		/* prompt target or NULL */
	int prompt_len;		/* prompt length or 0 */

	int margin_left, margin_right, margin_top, margin_bottom;
				/* margins */

	fstring_t **backlog;	/* buffer with lines */
	int backlog_size;	/* backlog size */

	int redraw;		/* does it have to be redrawn before display */

	int start;		/* from which line displaying starts */
	int lines_count;	/* number of screen lines in backlog */
	struct screen_line *lines;
				/* screen lines */

	int overflow;		/* number of superfluous lines in a window */

	int (*handle_redraw)(window_t *w);
				/* window contents redraw handler */

	void (*handle_mouse)(int x, int y, int state);

	time_t last_red_line;	/* timestamp for red line marker */
} ncurses_window_t;

extern WINDOW *ncurses_contacts;
extern WINDOW *ncurses_input;

QUERY(ncurses_session_disconnect_handler);

gboolean ncurses_simple_print(WINDOW *w, const char *s, fstr_attr_t attr, gssize maxx);
const char *ncurses_fstring_print(WINDOW *w, const char *s, const fstr_attr_t *attr, gssize maxx);

void ncurses_prompt_set(window_t *w, const gchar *str);
void ncurses_update_real_prompt(ncurses_window_t *n);
void ncurses_resize(void);
void ncurses_redraw(window_t *w);
void ncurses_redraw_input(unsigned int ch);
void ncurses_clear(window_t *w, int full);
void ncurses_refresh(void);
void ncurses_commit(void);
int ncurses_window_kill(window_t *w);
int ncurses_window_new(window_t *w);

#define contacts ncurses_contacts
#define history ncurses_history
#define history_index ncurses_history_index
	
#define HISTORY_MAX 1000
extern CHAR_T *ncurses_history[HISTORY_MAX];
extern int ncurses_history_index;
extern int ncurses_debug;

void header_statusbar_resize(const char *dummy);
void changed_backlog_size(const char *var);

extern int config_backlog_size;
extern int config_backlog_scroll_half_page;

extern int config_display_transparent;
extern int config_enter_scrolls;
extern int config_margin_size;
extern int config_kill_irc_window;

extern int config_text_bottomalign;

int ncurses_lastlog_update(window_t *w);
void ncurses_lastlog_new(window_t *w);
extern int config_lastlog_size;
extern int config_lastlog_lock;
extern int config_mark_on_window_change;

WATCHER(ncurses_watch_stdin);
WATCHER(ncurses_watch_winch);
int ncurses_command_window(void *data, va_list ap);
COMMAND(cmd_mark);

extern int have_winch_pipe;
extern int winch_pipe[2];

extern int ncurses_screen_height;
extern int ncurses_screen_width;

int color_pair(int fg, int bg);
int ncurses_backlog_add_real(window_t *w, /*locale*/ fstring_t *str);

CHAR_T ncurses_fixchar(CHAR_T ch, int *attr);

#ifndef COLOR_DEFAULT
#  define COLOR_DEFAULT (-1)
#endif

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
