/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo�ny <speedy@ziew.org>
 *			    Pawe� Maziarz <drg@infomex.pl>
			   Adam Osuchowski <adwol@polsl.gliwice.pl>
 *			    Wojtek Bojdo� <wojboj@htcon.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2.h"

#include <sys/ioctl.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_READLINE_READLINE_H
#	include <readline/history.h>
#	include <readline/readline.h>
#else
#	include <history.h>
#	include <readline.h>
#endif

#include "ui-readline.h"

int ui_screen_height;
int ui_screen_width;
int ui_need_refresh;

int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;

/* podstawmy ewentualnie brakuj�ce funkcje i definicje readline */

extern void rl_extend_line_buffer(int len);

#ifndef HAVE_RL_BIND_KEY_IN_MAP
int rl_bind_key_in_map(int key, void *function, void *keymap)
{
	return -1;
}
#endif

#ifndef HAVE_RL_GET_SCREEN_SIZE
int rl_get_screen_size(int *lines, int *columns)
{
#ifdef TIOCGWINSZ
	struct winsize ws;

	memset(&ws, 0, sizeof(ws));

	ioctl(0, TIOCGWINSZ, &ws);

	*columns = ws.ws_col;
	*lines = ws.ws_row;
#else
	*columns = 80;
	*lines = 24;
#endif
	return 0;
}
#endif

#ifndef HAVE_RL_FILENAME_COMPLETION_FUNCTION
char *rl_filename_completion_function(void)
{
	return NULL;
}
#endif

#ifndef HAVE_RL_SET_KEY
int rl_set_key(const char *key, void *function, void *keymap)
{
	return -1;
}
#endif

void set_prompt(const char *prompt) {
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(prompt);
#else
	rl_expand_prompt((char *)prompt);
#endif
}

/*
 * my_getc()
 *
 * pobiera znak z klawiatury obs�uguj�c w mi�dzyczasie sie�.
 */
int my_getc(FILE *f)
{
	if (ui_need_refresh) {
		ui_need_refresh = 0;
		rl_get_screen_size(&screen_lines, &screen_columns);
		if (screen_lines < 1)
			screen_lines = 24;
		if (screen_columns < 1)
			screen_columns = 80;
		ui_screen_width = screen_columns;
		ui_screen_height = screen_lines;
	}
	return rl_getc(f);
}

/*
 * ui_readline_print()
 *
 * wy�wietla dany tekst na ekranie, uwa�aj�c na trwaj�ce w danych chwili
 * readline().
 */
void ui_readline_print(window_t *w, int separate, const /*locale*/ char *xline)
{
	int old_end = rl_end, id = w->id;
	char *old_prompt = NULL, *line_buf = NULL;
	const char *line = NULL;

	if (config_timestamp) {
			/* XXX: recode timestamp? for fun or wcs? */
		string_t s = string_init(NULL);
		const char *p = xline;
		const char *buf = timestamp(format_string(config_timestamp));

		string_append(s, "\033[0m");
		string_append(s, buf);
		string_append_c(s, ' ');
		
		while (*p) {
			string_append_c(s, *p);
			if (*p == '\n' && *(p + 1)) {
				string_append(s, buf);
				string_append_c(s, ' ');
			}
			p++;
		}

		line = line_buf = string_free(s, 0);
	} else
		line = xline;

	/* je�li nie piszemy do aktualnego, to zapisz do bufora i wyjd� */
	if (id != window_current->id) {
		window_write(id, line);

		/* XXX trzeba jeszcze waln�� od�wie�enie prompta */
		goto done;
	}

	/* je�li mamy ukrywa� wszystko, wychodzimy */
	if (pager_lines == -2)
		goto done;

	window_write(window_current->id, line);

	/* ukryj prompt, je�li jeste�my w trakcie readline */
	if (in_readline) {
		old_prompt = g_strdup(rl_prompt);
		rl_end = 0;
/*		set_prompt(NULL);*/
		rl_redisplay();
			/* XXX: string width instead?
			 * or cleartoeol? */
		printf("\r%*c\r", (int) xstrlen(old_prompt), ' ');
	}

	printf("%s", line);

	if (pager_lines >= 0) {
		pager_lines++;

		if (pager_lines >= screen_lines - 2) {
			const gchar *prompt = format_find("readline_more");
			char *lprompt = ekg_recode_to_locale(prompt);
			char *tmp;
				/* XXX: lprompt pretty const, make it static? */

			in_readline++;

			set_prompt(lprompt);

			pager_lines = -1;
			tmp = readline(lprompt);
			g_free(lprompt);
			in_readline--;
			if (tmp) {
				free(tmp); /* allocd by readline */
				pager_lines = 0;
			} else {
				printf("\n");
				pager_lines = -2;
			}
			printf("\033[A\033[K");		/* XXX brzydko */
		}
	}

	/* je�li jeste�my w readline, poka� z powrotem prompt */
	if (in_readline) {
		rl_end = old_end;
		set_prompt(old_prompt);
		g_free(old_prompt);
		rl_forced_update_display();
	}
	
done:
	if (line_buf)
		g_free(line_buf);
}

/**
 * current_prompt()
 *
 * Get the current prompt, locale-recoded.
 *
 * @return Static buffer pointer, non-NULL, locale-encoded.
 */
const /*locale*/ char *current_prompt(void)
{
	static gchar *buf = NULL;
	session_t *s;
	char *tmp, *act, *sid;
	char *format, *format_act;

	if (no_prompt)
		return "";

	s = session_current;
	sid = s ? (s->alias?s->alias:s->uid) : "";

	if (window_current->id > 1) {
		format		= "rl_prompt_query";
		format_act	= "rl_prompt_query_act";
	} else if (s && (s && s->status == EKG_STATUS_INVISIBLE)) {
		format		= "rl_prompt_invisible";
		format_act	= "rl_prompt_invisible_act";
	} else if (s && (s->status < EKG_STATUS_AVAIL)) {
		format		= "rl_prompt_away";
		format_act	= "rl_prompt_away_act";
	} else {
		format		= "rl_prompt";
		format_act	= "rl_prompt_act";
	}

	act = window_activity();
	if (act)
		tmp = format_string(format_find(format_act), sid, ekg_itoa(window_current->id), act, window_current->target);
	else
		tmp = format_string(format_find(format), sid, ekg_itoa(window_current->id), window_current->target);

	g_free(buf);
	buf = ekg_recode_to_locale(tmp);
	g_free(tmp);
	g_free(act);

	return buf;
}

int my_loop(void) {
	extern void ekg_loop();
	ekg_loop();
	return 0;
}

/**
 * my_readline()
 *
 * Wrapper for readline(). Prepares appropriate prompt, locks screen
 * output. Recodes the result to utf-8.
 *
 * @return A pointer to utf8 string or NULL.
 */
static gchar *my_readline(void)
{
		/* main loop could call current_prompt()
		 * and break the buffer */
	char *prompt = g_strdup(current_prompt());
	char *res, *tmp;

	in_readline++;
	res = readline(prompt);
	in_readline--;

	if (config_print_line) {
		tmp = g_strdup_printf("%s%s\n", prompt, (res) ? res : "");
		window_write(window_current->id, tmp);
		g_free(tmp);
	} else {
		window_refresh();
	}

	g_free(prompt);

	return res;
}

/*
 * ui_readline_loop()
 *
 * g��wna p�tla programu. wczytuje dane z klawiatury w mi�dzyczasie
 * obs�uguj�c sie� i takie tam.
 */
int ui_readline_loop(void)
{
	char *line = my_readline();
	char *rline = line; /* for freeing */
	gchar *out, *p;
	gint len;

	if (!line) {
		/* Ctrl-D handler */
		if (window_current->id == 0) {			/* debug window */
			window_switch(1);
		} else if (window_current->id == 1) {		/* status window */
			if (config_ctrld_quits)	{
				return 0;
			} else {
				printf("\n");
			}
		} else if (window_current->id > 1) {		/* query window */
			window_kill(window_current);
		}
		return 1;
	}

	len = strlen(line);
	if (G_LIKELY(len > 0)) {
		if (G_UNLIKELY(line[len - 1] == '\\')) {
			/* multi line handler */
			GString *s = g_string_new_len(line, len-1);
			
			free(line);

			no_prompt = 1;
			rl_bind_key(9, rl_insert);

			while ((line = my_readline())) {
				if (!strcmp(line, "."))
					break;
				g_string_append(s, line);
				g_string_append_len(s, "\r\n", 2); /* XXX */
				free(line);
			}

			rl_bind_key(9, rl_complete);
			no_prompt = 0;

			if (line) {
				g_string_free(s, TRUE);
				free(line);
				return 1;
			}

			line = g_string_free(s, FALSE);
		}
		
		/* if no empty line and we save duplicate lines, add it to history */
		if (config_history_savedups || !history_length || strcmp(line, history_get(history_length)->line))
			add_history(line);
	}

	pager_lines = 0;

	/* now we can definitely recode */
	out = ekg_recode_from_locale(line);
	if (G_LIKELY(line == rline))
		free(rline); /* allocd by readline */
	else
		g_free(line); /* allocd by us */

	/* omit leading whitespace */
	for (p = out; g_unichar_isspace(g_utf8_get_char(p)); p = g_utf8_next_char(p));
	if (*p || config_send_white_lines)
		command_exec(window_current->target, window_current->session, out, 0);

	pager_lines = -1;

	g_free(out);
	return 1;
}

/*
 * wy�wietla ponownie zawarto�� okna.
 *
 * XXX podpi�� pod Ctrl-L.
 */
int window_refresh() {
	char **p;

	printf("\033[H\033[J"); /* XXX */

	for (p = readline_current->line; *p; p++)
		printf("%s", *p);

	return 0;
}

/*
 * window_write()
 *
 * dopisuje lini� do bufora danego okna.
 */
int window_write(int id, const /*locale*/ char *line)
{
	window_t *w = window_exist(id);
	readline_window_t *r = readline_window(w);
	int i = 1;

	if (!line || !w)
		return -1;

	/* je�li ca�y bufor zaj�ty, zwolnij pierwsz� lini� i przesu� do g�ry */
	if (r->line[MAX_LINES_PER_SCREEN - 1]) {
		xfree(r->line[0]);
		memmove(&(r->line[0]), &(r->line[1]), sizeof(char *) * (MAX_LINES_PER_SCREEN - 1));
		r->line[MAX_LINES_PER_SCREEN - 1] = xstrdup(line);
	} else {
		/* znajd� pierwsz� woln� lini� i si� wpisz. */
		for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
			if (!r->line[i]) {
				r->line[i] = xstrdup(line);
				break;
			}
	}

	if (w != window_current) {
		set_prompt(current_prompt());
		rl_redisplay();
	}
	
	return 0;
}

#if 0 /* TODO */
	/* nowe okno w == window_current ? */ {
		set_prompt(current_prompt());
#endif

/*
 * window_activity()
 *
 * zwraca string z actywnymi oknami 
 */
gchar *window_activity(void)
{
	GString *s = g_string_sized_new(8);
	gboolean first = TRUE;
	window_t *w;

	for (w = windows; w; w = w->next) {
/* we cannot make it colorful with default formats because grey on black doesn't look so good... */
		if (!w->act || !w->id) 
			continue;

		if (!first)
			g_string_append_c(s, ',');
		g_string_append(s, ekg_itoa(w->id));
		first = FALSE;
	}

	return g_string_free(s, first);
}
		
/*
 * bind_find_command()
 *
 * szuka komendy, kt�r� nale�y wykona� dla danego klawisza.
 */
char *bind_find_command(const char *seq)
{
	struct binding *s;

	if (!seq)
		return NULL;
	
	for (s = bindings; s; s = s->next) {
		if (s->key && !xstrcasecmp(s->key, seq))
			return s->action;
	}

	return NULL;
}

/*
 * bind_handler_ctrl()
 *
 * obs�uguje klawisze Ctrl-A do Ctrl-Z, wywo�uj�c przypisane im akcje.
 */
int bind_handler_ctrl(int a, int key)
{
	char *tmp = saprintf("Ctrl-%c", 'A' + key - 1);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(window_current->target, window_current->session, bind_find_command(tmp), 0);
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_alt()
 *
 * obs�uguje klawisze z Altem, wywo�uj�c przypisane im akcje.
 */
int bind_handler_alt(int a, int key)
{
	char *tmp = saprintf("Alt-%c", key);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(window_current->target, window_current->session, bind_find_command(tmp), 0);
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_window()
 *
 * obs�uguje klawisze Alt-1 do Alt-0, zmieniaj�c okna na odpowiednie.
 */
int bind_handler_window(int a, int key)
{
	if (key > '0' && key <= '9')
		window_switch(key - '0');
	else
		window_switch(10);

	return 0;
}

int bind_sequence(const char *seq, const char *command, int quiet)
{
	char *nice_seq = NULL;
	
	if (!seq)
		return -1;

	if (command && bind_find_command(seq)) {
		printq("bind_seq_exist", seq);
		return -1;
	}
	
	if (!xstrncasecmp(seq, "Ctrl-", 5) && xstrlen(seq) == 6 && xisalpha(seq[5])) {
		int key = CTRL(xtoupper(seq[5]));

		if (command) {
			rl_bind_key(key, bind_handler_ctrl);
			nice_seq = xstrdup(seq);
			nice_seq[0] = xtoupper(nice_seq[0]);
			nice_seq[1] = xtolower(nice_seq[1]);
			nice_seq[2] = xtolower(nice_seq[2]);
			nice_seq[3] = xtolower(nice_seq[3]);
			nice_seq[5] = xtoupper(nice_seq[5]);
		} else
			rl_unbind_key(key);

	} else if (!xstrncasecmp(seq, "Alt-", 4) && xstrlen(seq) == 5) {

		if (command) {
			rl_bind_key_in_map(xtolower(seq[4]), bind_handler_alt, emacs_meta_keymap);
			nice_seq = xstrdup(seq);
			nice_seq[0] = xtoupper(nice_seq[0]);
			nice_seq[1] = xtolower(nice_seq[1]);
			nice_seq[2] = xtolower(nice_seq[2]);
			nice_seq[4] = xtoupper(nice_seq[4]);
		} else
			rl_unbind_key_in_map(xtolower(seq[4]), emacs_meta_keymap);
		
	} else {
		printq("bind_seq_incorrect", seq);

		return -1;
	}

	if (command) {
		struct binding *s;

		s = xmalloc(sizeof(struct binding));
		
		s->key = nice_seq;
		s->action = xstrdup(command);
		s->internal = 0;

		LIST_ADD2(&bindings, s);

		if (!quiet) {
			print("bind_seq_add", s->key);
			config_changed = 1;
		}
	} else {
		struct binding *s;

		for (s = bindings; s; s = s->next) {
			if (s->key && !xstrcasecmp(s->key, seq)) {
				LIST_REMOVE2(&bindings, s, NULL);
				if (!quiet) {
					print("bind_seq_remove", seq);
					config_changed = 1;
				}
				return 0;
			}
		}
	}

	return 1;
}
