/* $Id$ */

/*
 *  (C) Copyright 2002-2004 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Wojtek Bojdo� <wojboj@htcon.pl>
 *			    Pawe� Maziarz <drg@infomex.pl>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
 *		  2008-2010 Wies�aw Ochmi�ski <wiechu@wiechu.com>
 *		       2010 S�awomir Nizio <poczta-sn@gazeta.pl>
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

#include <stdlib.h>
#include <string.h>

#ifdef USE_UNICODE
#  include <limits.h>
#endif

#include <ekg/completion.h>

#include "bindings.h"
#include "contacts.h"
#include "input.h"
#include "notify.h"
#include "nc-stuff.h"

struct binding *ncurses_binding_map[KEY_MAX + 1];	/* mapa klawiszy */
struct binding *ncurses_binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

void *ncurses_binding_complete = NULL;
void *ncurses_binding_accept_line = NULL;

int bindings_added_max = 0;

#define line ncurses_line
#define lines ncurses_lines

static const void *BINDING_HISTORY_NOEXEC = (void*) -1;


static void add_to_history() {
	if (history[0] != line)
		xfree(history[0]);

	history[0] = lines ? wcs_array_join(lines, TEXT("\015")) : xwcsdup(line);

	xfree(history[HISTORY_MAX - 1]);
	memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));

	history[0] = line;
	history_index = 0;
}

static int input_backward_word(void) {
	int i = line_index;
		/* XXX: isspace()/iswspace()? */
	while (i > 0 && line[i - 1] == ' ')
		i--;
	while (i > 0 && line[i - 1] != ' ')
		i--;
	return i;
}

static int input_forward_word(void) {
	size_t linelen = xwcslen(line);
	int i = line_index;
	while (i < linelen && line[i] == ' ')
		i++;
	while (i < linelen && line[i] != ' ')
		i++;
	return i;
}

static BINDING_FUNCTION(binding_backward_word)
{
	line_index = input_backward_word();
}

static BINDING_FUNCTION(binding_forward_word) {
	line_index = input_forward_word();
}

static BINDING_FUNCTION(binding_kill_word)
{
	int eaten = input_forward_word() - line_index;

	if (eaten == 0)
		return;

	xfree(yanked);
	yanked = xcalloc(eaten + 1, sizeof(CHAR_T));
	xwcslcpy(yanked, line + line_index, eaten + 1);

	memmove(line + line_index, line + line_index + eaten, sizeof(CHAR_T) * (xwcslen(line) - line_index - eaten + 1));
}

static BINDING_FUNCTION(binding_toggle_input)
{
	if (input_size == 1) {
		input_size = MULTILINE_INPUT_SIZE;
		ncurses_input_update(line_index);
	} else {
		string_t s = string_init((""));
		char *tmp;
		gchar *out, *p;
		int i;

		for (i = 0; lines[i]; i++) {
			char *tmp;

			string_append(s, (tmp = wcs_to_normal(lines[i])));	free_utf(tmp);
			if (lines[i + 1])
				string_append(s, ("\r\n"));
		}

			/* XXX: we could probably use string_t recoding func here */
		tmp = string_free(s, 0);
		out = ekg_recode_from_locale(tmp);
		g_free(tmp);

		add_to_history();

		input_size = 1;
		ncurses_input_update(0);

		/* omit leading whitespace */
		for (p = out; g_unichar_isspace(g_utf8_get_char(p)); p = g_utf8_next_char(p));
		if (*p || config_send_white_lines)
			command_exec(window_current->target, window_current->session, out, 0);

		if (!out[0] || out[0] == '/' || !window_current->target)
			ncurses_typing_mod		= 1;
		else {
			ncurses_typing_win		= NULL;
		}

		curs_set(1);
		g_free(out);
	}
}

static BINDING_FUNCTION(binding_cancel_input)
{
	if (input_size != 1) {
		input_size = 1;
		ncurses_input_update(0);
		ncurses_typing_mod = 1;
	}
}

static BINDING_FUNCTION(binding_backward_delete_char)
{
	if (lines && line_index == 0 && lines_index > 0 && xwcslen(lines[lines_index]) + xwcslen(lines[lines_index - 1]) < LINE_MAXLEN) {
		int i;

		line_index = xwcslen(lines[lines_index - 1]);
		xwcscat(lines[lines_index - 1], lines[lines_index]);

		xfree(lines[lines_index]);

		for (i = lines_index; i < g_strv_length((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (g_strv_length((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_index--;
		lines_adjust();
		ncurses_typing_mod = 1;
	} else if (xwcslen(line) > 0 && line_index > 0) {
		memmove(line + line_index - 1, line + line_index, (LINE_MAXLEN - line_index) * sizeof(CHAR_T));
		line[LINE_MAXLEN - 1] = 0;
		line_index--;
		ncurses_typing_mod = 1;
	}
}

static BINDING_FUNCTION(binding_window_kill)
{
	/* rfc2811: "Channels names are strings (beginning with a '&', '#', '+' or '!' character)..." */
	const char *pfx = "&#+!";
	char * ptr;
	ptr = xstrstr(window_current->target, "irc:");
	if (ptr && ptr == window_current->target && xstrchr(pfx, ptr[4]) && !config_kill_irc_window ) {
		print("cant_kill_irc_window");
		return;
	}
	command_exec(window_current->target, window_current->session, ("/window kill"), 0);
}

static BINDING_FUNCTION(binding_kill_line)
{
	line[line_index] = 0;
}

static BINDING_FUNCTION(binding_yank)
{
	if (yanked && xwcslen(yanked) + xwcslen(line) + 1 < LINE_MAXLEN) {
		memmove(line + line_index + xwcslen(yanked), line + line_index, (LINE_MAXLEN - line_index - xwcslen(yanked)) * sizeof(CHAR_T));
		memcpy(line + line_index, yanked, sizeof(CHAR_T) * xwcslen(yanked));
		line_index += xwcslen(yanked);
	}
}

static BINDING_FUNCTION(binding_delete_char)
{
	if (lines && line_index == xwcslen(line) && lines_index < g_strv_length((char **) lines) - 1 && xwcslen(line) + xwcslen(lines[lines_index + 1]) < LINE_MAXLEN) {
		int i;

		xwcscat(line, lines[lines_index + 1]);

		xfree(lines[lines_index + 1]);

		for (i = lines_index + 1; i < g_strv_length((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (g_strv_length((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_adjust();
		ncurses_typing_mod = 1;
	} else if (line_index < xwcslen(line)) {
		memmove(line + line_index, line + line_index + 1, (LINE_MAXLEN - line_index - 1) * sizeof(CHAR_T));
		line[LINE_MAXLEN - 1] =  0;
		ncurses_typing_mod = 1;
	}
}

static BINDING_FUNCTION(binding_accept_line)
{
	char *p, *txt;

#if 0
	if (ncurses_noecho) { /* we are running ui-password-input */
		ncurses_noecho = 0;
		ncurses_passbuf = xwcsdup(line);
		line[0] = 0;
		line_index = line_start = 0;
		return;
	}
#endif

	if (lines) {
		int i;

		lines = xrealloc(lines, (g_strv_length((char **) lines) + 2) * sizeof(CHAR_T *));

		for (i = g_strv_length((char **) lines); i > lines_index; i--)
			lines[i + 1] = lines[i];

		lines[lines_index + 1] = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));
		xwcscpy(lines[lines_index + 1], line + line_index);
		line[line_index] = 0;

		line_index = 0;
		line_start = 0;
		lines_index++;

		lines_adjust();

		return;
	}
	if (arg != BINDING_HISTORY_NOEXEC) {
		gchar *out;
		txt = wcs_to_normal(line);
		out = ekg_recode_from_locale(txt);
		for (p = out; g_unichar_isspace(g_utf8_get_char(p)); p = g_utf8_next_char(p));
		if (*p || config_send_white_lines)
			command_exec(window_current->target, window_current->session, out, 0);
		g_free(out);
		free_utf(txt);
	}

	if (ncurses_plugin_destroyed)
		return;
	if (!line[0] || line[0] == '/' || !window_current->target) /* if empty or command, just mark as modified */
		ncurses_typing_mod		= 1;
	else { /* if message, assume that its' handler has already disabled <composing/> */
		ncurses_typing_win		= NULL;
	}

	if (xwcscmp(line, TEXT(""))) {
		if (config_history_savedups || xwcscmp(line, history[1]))
			add_to_history();
	} else {
		if (config_enter_scrolls)
			print("none", "");
	}

	history[0] = line;
	history_index = 0;
	*line = 0;
	line_index = line_start = 0;
}

static BINDING_FUNCTION(binding_line_discard)
{
#if 0
	if (!ncurses_noecho) { /* we don't want to yank passwords */
		xfree(yanked);
		yanked = xwcsdup(line);
	}
#endif
	*line = 0;
	line_index = line_start = 0;

	if (lines && lines_index < g_strv_length((char **) lines) - 1) {
		int i;

		xfree(lines[lines_index]);

		for (i = lines_index; i < g_strv_length((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (g_strv_length((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_adjust();
	}
}

static BINDING_FUNCTION(binding_quoted_insert)
{
/* XXX
 * naprawi�
 */
}

static BINDING_FUNCTION(binding_word_rubout)
{
	int eaten;
	CHAR_T *p;

	if (!line_index)
		return;

	if ((eaten = line_index - input_backward_word()) == 0)
		return;

	p = line + line_index - eaten;

	xfree(yanked);
	yanked = xcalloc(eaten + 1, sizeof(CHAR_T));
	xwcslcpy(yanked, p, eaten + 1);

	memmove(p, line + line_index, (xwcslen(line) - line_index + 1) * sizeof(CHAR_T));
	line_index -= eaten;
}

static void show_completions() {
	int maxlen, cols, complcount, rows, i;
	char *tmp;

	if (!ekg2_completions || (complcount = g_strv_length(ekg2_completions)) == 0)
		return;

	maxlen = 0;
	for (i = 0; ekg2_completions[i]; i++) {
		size_t compllen = xstrlen(ekg2_completions[i]) + 2;	/* XXX xstrlen_pl() ? */
		if (compllen > maxlen)
			maxlen = compllen;
	}

	cols = (window_current->width - 6) / maxlen;
	if (cols == 0)
			cols = 1;

	rows = complcount / cols + 1;

	tmp = xmalloc((cols * maxlen + 2)*sizeof(char));

	for (i = 0; i < rows; i++) {
		int j;

		tmp[0] = 0;
		for (j = 0; j < cols; j++) {
			int cell = j * rows + i;

			if (cell < complcount) {
				int k;

				xstrcat(tmp, ekg2_completions[cell]);

				for (k = xstrlen(ekg2_completions[cell]); k < maxlen; k++)
					xstrcat(tmp, (" "));
			}
		}
		if (tmp[0])
			print("none", tmp);
	}
	xfree(tmp);
}

static BINDING_FUNCTION(binding_complete)
{
	if (!lines) {
		int complete_result = 0;
		GString *linebuf = g_string_sized_new(LINE_MAXLEN + 1);
		gchar *tmp;
		int line_start_tmp, line_index_tmp;
		int i, j, nlen;

#if USE_UNICODE
		wctomb(NULL, 0); /* reset */
		for (i = 0; line[i]; i++) {
			char buf[MB_LEN_MAX+1];
			int tmp;

			tmp = wctomb(buf, line[i]);

			if (tmp <= 0 || tmp > MB_CUR_MAX) {
				debug_error("binding_complete() wctomb() failed (%d) [%d]\n", tmp, MB_CUR_MAX);
				return;
			}

			g_string_append_len(linebuf, buf, tmp);
		}
#endif

		tmp = ekg_recode_from_locale(
#if USE_UNICODE
				linebuf->str
#else
				line
#endif
				);
		g_string_assign(linebuf, tmp);
		g_free(tmp);

		/* recalc offsets */

#if !USE_UNICODE
		if (console_charset_is_utf8) {
			line_start_tmp = line_start;
			line_index_tmp = line_index;
		} else
#endif
		{
			line_start_tmp = (int) (g_utf8_offset_to_pointer(linebuf->str, line_start) - linebuf->str);
			line_index_tmp = (int) (g_utf8_offset_to_pointer(linebuf->str, line_index) - linebuf->str);
		}

			/* XXX: rewrite ekg2_complete() to use GString directly */
		complete_result = ekg2_complete(&line_start_tmp, &line_index_tmp,
				linebuf->str, linebuf->allocated_len);

		/* warning: ATM linebuf->len can not be trusted */

#if !USE_UNICODE
		if (console_charset_is_utf8) {
			line_start = line_start_tmp;
			line_index = line_index_tmp;
		} else
#endif
		{
			line_start = g_utf8_pointer_to_offset(linebuf->str, &linebuf->str[line_start_tmp]);
			line_index = g_utf8_pointer_to_offset(linebuf->str, &linebuf->str[line_index_tmp]);
		}

		tmp = ekg_recode_to_locale(linebuf->str);
#if USE_UNICODE
		nlen = strlen(tmp); /* linebuf->len can't be used as we modified the buf! */
			/* XXX: mbstowcs()? */
		mbtowc(NULL, NULL, 0);
		for (i = 0, j = 0; tmp[j]; i++) {
			int ret;

			ret = mbtowc(&line[i], &tmp[j], nlen-j);

			if (ret <= 0) {
				debug_error("binding_complete() mbtowc() failed (%d)\n", tmp);
				break;	/* return; */
			}

			j += ret;
		}
		line[i] = '\0';
#else
		g_strlcpy(line, tmp, LINE_MAXLEN); /* XXX: use GString for line? */
#endif
		g_free(tmp);
		g_string_free(linebuf, TRUE);

		if (complete_result)
			show_completions();
	} else {
		int i, count = 8 - (line_index % 8);

		if (xwcslen(line) + count >= LINE_MAXLEN - 1)
			return;

		memmove(line + line_index + count, line + line_index, sizeof(CHAR_T) * (LINE_MAXLEN - line_index - count));

		for (i = line_index; i < line_index + count; i++)
			line[i] = CHAR(' ');

		line_index += count;
	}
}

static BINDING_FUNCTION(binding_end_of_line)
{
	const int width = input->_maxx - ncurses_current->prompt_len - 1;
	/* set cursor position to the end of the line */
	line_index = xwcslen(ncurses_line);
	/* show as much as possible */
	if (line_index > width)
		line_start = line_index - width;
	else
		line_start = 0;
}

static BINDING_FUNCTION(binding_beginning_of_line)
{
	line_index = 0;
	line_start = 0;
}

static BINDING_FUNCTION(binding_backward_char)
{
	if (lines) {
		if (line_index > 0)
			line_index--;
		else {
			if (lines_index > 0) {
				lines_index--;
				lines_adjust();
				binding_end_of_line(NULL);
			}
		}

		return;
	}

	if (line_index > 0)
		line_index--;
}

static BINDING_FUNCTION(binding_forward_char) {
	size_t linelen = xwcslen(line);
	if (lines) {
		if (line_index < linelen)
			line_index++;
		else {
			if (lines_index < g_strv_length((char **) lines) - 1) {
				lines_index++;
				line_index = 0;
				line_start = 0;
			}
			lines_adjust();
		}

		return;
	}

	if (line_index < linelen)
		line_index++;
}


static void get_history_lines() {
	if (xwcschr(history[history_index], ('\015'))) {
		CHAR_T **tmp;
		int i, count;

		if (input_size == 1) {
			input_size = MULTILINE_INPUT_SIZE;
			ncurses_input_update(0);
		}

		tmp = wcs_array_make(history[history_index], TEXT("\015"), 0, 0, 0);
		count = g_strv_length((char **) tmp);

		g_strfreev((char **) lines);
		lines = xmalloc((count + 2) * sizeof(CHAR_T *));

		for (i = 0; i < count; i++) {
			lines[i] = xmalloc(LINE_MAXLEN * sizeof(CHAR_T));
			xwcscpy(lines[i], tmp[i]);
		}

		g_strfreev((char **) tmp);

		line_index = 0;
		lines_index = 0;
		lines_adjust();
	} else {
		if (input_size != 1) {
			input_size = 1;
			ncurses_input_update(0);
		}
		xwcscpy(line, history[history_index]);
		binding_end_of_line(NULL);
	}
}

BINDING_FUNCTION(binding_previous_only_history)
{
	if (!history[history_index + 1])
		return;

	if (history_index == 0) {
		if (lines) {
			add_to_history();

			history_index = 1;

			input_size = 1;
			ncurses_input_update(0);
		} else
			history[0] = xwcsdup(line);
	}

	history_index++;
	get_history_lines();

	if (lines) {
		lines_index = g_strv_length((char **)lines) - 1;
		line_index = LINE_MAXLEN+1;
		lines_adjust();
	}
}

BINDING_FUNCTION(binding_next_only_history)
{
	if (history_index > 0) {
		history_index--;
		get_history_lines();
	} else {/* history_index == 0 */
		if (lines) {
			add_to_history();
			input_size = 1;
			ncurses_input_update(0);
		} else
			binding_accept_line(BINDING_HISTORY_NOEXEC);
	}
}


static BINDING_FUNCTION(binding_previous_history)
{
	ncurses_typingsend(window_current, EKG_CHATSTATE_ACTIVE);

	if (lines && (lines_index || lines_start)) {
		if (lines_index - lines_start == 0 && lines_start)
			lines_start--;

		if (lines_index)
			lines_index--;

		lines_adjust();

	} else
		binding_previous_only_history(NULL);
	ncurses_redraw_input(0);
}

static BINDING_FUNCTION(binding_next_history)
{
	if (lines && (lines_index < g_strv_length((char **) lines) - 1)) {
		lines_index++;
		lines_adjust();
	} else {
		ncurses_typingsend(window_current, EKG_CHATSTATE_ACTIVE);
		binding_next_only_history(NULL);
	}
	ncurses_redraw_input(0);
}

void binding_helper_scroll(window_t *w, int offset) {
	ncurses_window_t *n;

	if (!w || !(n = w->priv_data))
		return;

	if (offset < 0) {
		n->start += offset;
		if (n->start < 0)
			n->start = 0;

	} else {
		n->start += offset;

		if (n->start > n->lines_count - w->height + n->overflow)
			n->start = n->lines_count - w->height + n->overflow;

		if (n->start < 0)
			n->start = 0;

	/* old code from: binding_forward_page() need it */
		if (w == window_current) {
			if (ncurses_current->start == ncurses_current->lines_count - window_current->height + ncurses_current->overflow) {
				window_current->more = 0;
				update_statusbar(0);
			}
		}
	}

	ncurses_redraw(w);
	ncurses_commit();
}

static void binding_helper_scroll_page(window_t *w, int backward) {
	int offset;

	if (!w)
		return;

	offset = config_backlog_scroll_half_page ? (w->height / 2) : (w->height-1);
	if (backward)
		binding_helper_scroll(w, -offset);
	else
		binding_helper_scroll(w, +offset);
}

static BINDING_FUNCTION(binding_backward_page) {
	binding_helper_scroll_page(window_current, 1);
}

static BINDING_FUNCTION(binding_forward_page) {
	binding_helper_scroll_page(window_current, 0);
}

static BINDING_FUNCTION(binding_backward_lastlog_page) {
	binding_helper_scroll_page(window_exist(WINDOW_LASTLOG_ID), 1);
}

static BINDING_FUNCTION(binding_forward_lastlog_page) {
	binding_helper_scroll_page(window_exist(WINDOW_LASTLOG_ID), 0);
}

static BINDING_FUNCTION(binding_backward_contacts_page) {
	binding_helper_scroll_page(window_exist(WINDOW_CONTACTS_ID), 1);
}

static BINDING_FUNCTION(binding_forward_contacts_page) {
	binding_helper_scroll_page(window_exist(WINDOW_CONTACTS_ID), 0);
}

static BINDING_FUNCTION(binding_backward_contacts_line) {
	binding_helper_scroll(window_exist(WINDOW_CONTACTS_ID), -1);
}

static BINDING_FUNCTION(binding_forward_contacts_line) {
	binding_helper_scroll(window_exist(WINDOW_CONTACTS_ID), 1);
}

static BINDING_FUNCTION(binding_ignore_query)
{
	if (!window_current->target)
		return;

	command_exec_format(window_current->target, window_current->session, 0, ("/ignore \"%s\""), window_current->target);
}

static BINDING_FUNCTION(binding_quick_list_wrapper)
{
	binding_quick_list(0, 0);
}

static BINDING_FUNCTION(binding_toggle_contacts_wrapper)
{
	static int last_contacts = -1;

	if (!config_contacts) {
		if ((config_contacts = last_contacts) == -1)
			config_contacts = 2;
	} else {
		last_contacts = config_contacts;
		config_contacts = 0;
	}

	ncurses_contacts_changed("contacts");
}

BINDING_FUNCTION(binding_next_contacts_group) {
	window_t *w;

	contacts_group_index++;

	if ((w = window_exist(WINDOW_CONTACTS_ID))) {
		ncurses_contacts_update(w, 0);
/*		ncurses_resize(); */
		ncurses_commit();
	}
}

static BINDING_FUNCTION(binding_ui_ncurses_debug_toggle)
{
	if (++ncurses_debug > 3)
		ncurses_debug = 0;

	update_statusbar(1);
}

static BINDING_FUNCTION(binding_cycle_sessions)
{
	if (window_session_cycle(window_current) == 0)
		update_statusbar(1);
}

/*
 * binding_parse()
 *
 * analizuje dan� akcj� i wype�nia pola struct binding odpowiedzialne
 * za dzia�anie.
 *
 *  - b - wska�nik wype�nianej skruktury,
 *  - action - akcja,
 */
static void binding_parse(struct binding *b, const char *action)
{
	char **args;

	if (!b || !action)
		return;

	b->action = xstrdup(action);

	args = array_make(action, (" \t"), 1, 1, 1);

	if (!args[0]) {
		g_strfreev(args);
		return;
	}

#define __action(x,y) \
	if (!xstrcmp(args[0], (x))) { \
		b->function = y; \
		b->arg = xstrdup(args[1]); \
	} else

	__action("backward-word", binding_backward_word)
	__action("forward-word", binding_forward_word)
	__action("kill-word", binding_kill_word)
	__action("toggle-input", binding_toggle_input)
	__action("cancel-input", binding_cancel_input)
	__action("backward-delete-char", binding_backward_delete_char)
	__action("beginning-of-line", binding_beginning_of_line)
	__action("end-of-line", binding_end_of_line)
	__action("delete-char", binding_delete_char)
	__action("backward-page", binding_backward_page)
	__action("forward-page", binding_forward_page)
	__action("kill-line", binding_kill_line)
	__action("window-kill", binding_window_kill)
	__action("yank", binding_yank)
	__action("accept-line", binding_accept_line)
	__action("line-discard", binding_line_discard)
	__action("quoted-insert", binding_quoted_insert)
	__action("word-rubout", binding_word_rubout)
	__action("backward-char", binding_backward_char)
	__action("forward-char", binding_forward_char)
	__action("previous-history", binding_previous_history)
	__action("previous-only-history", binding_previous_only_history)
	__action("next-history", binding_next_history)
	__action("next-only-history", binding_next_only_history)
	__action("complete", binding_complete)
	__action("quick-list", binding_quick_list_wrapper)
	__action("toggle-contacts", binding_toggle_contacts_wrapper)
	__action("next-contacts-group", binding_next_contacts_group)
	__action("ignore-query", binding_ignore_query)
	__action("ui-ncurses-debug-toggle", binding_ui_ncurses_debug_toggle)
	__action("cycle-sessions", binding_cycle_sessions)
	__action("forward-contacts-page", binding_forward_contacts_page)
	__action("backward-contacts-page", binding_backward_contacts_page)
	__action("forward-contacts-line", binding_forward_contacts_line)
	__action("backward-contacts-line", binding_backward_contacts_line)
	__action("forward-lastlog-page", binding_forward_lastlog_page)
	__action("backward-lastlog-page", binding_backward_lastlog_page)
	; /* no/unknown action */


#undef __action

	g_strfreev(args);
}

/*
 * binding_key()
 *
 * analizuje nazw� klawisza i wpisuje akcj� do odpowiedniej mapy.
 *
 * 0/-1.
 */
static int binding_key(struct binding *b, const char *key, int add)
{
	/* debug("Key: %s\n", key); */
	if (!xstrncasecmp(key, ("Alt-"), 4)) {
		unsigned char ch;

#define __key(x, y, z) \
	if (!xstrcasecmp(key + 4, (x))) { \
		b->key = saprintf("Alt-%s", (x)); \
		if (add) { \
			ncurses_binding_map_meta[y] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map_meta[z] = ncurses_binding_map_meta[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Insert", KEY_IC, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

		if (xstrlen(key) != 5)
			return -1;

		ch = xtoupper(key[4]);

		b->key = saprintf(("Alt-%c"), ch);	/* XXX Alt-� ??? */

		if (add) {
			ncurses_binding_map_meta[ch] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding)));
			if (xisalpha(ch))
				ncurses_binding_map_meta[xtolower(ch)] = ncurses_binding_map_meta[ch];
		}

		return 0;
	}

	if (!xstrncasecmp(key, ("Ctrl-"), 5)) {
		unsigned char ch;

//		if (xstrlen(key) != 6)
//			return -1;
#define __key(x, y, z) \
	if (!xstrcasecmp(key + 5, (x))) { \
		b->key = saprintf("Ctrl-%s", (x)); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", KEY_CTRL_ENTER, 0);
	__key("Escape", KEY_CTRL_ESCAPE, 0);
	__key("Delete", KEY_CTRL_DC, 0);
	__key("Backspace", KEY_CTRL_BACKSPACE, 0);
	__key("Tab", KEY_CTRL_TAB, 0);

#undef __key

		ch = xtoupper(key[5]);
		b->key = saprintf(("Ctrl-%c"), ch);

		if (add) {
			if (xisalpha(ch))
				ncurses_binding_map[ch - 64] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding)));
			else
				return -1;
		}

		return 0;
	}

	if (xtoupper(key[0]) == 'F' && atoi(key + 1)) {
		int f = atoi(key + 1);

		if (f < 1 || f > 63)
			return -1;

		b->key = saprintf(("F%d"), f);

		if (add)
			ncurses_binding_map[KEY_F(f)] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding)));

		return 0;
	}

#define __key(x, y, z) \
	if (!xstrcasecmp(key, (x))) { \
		b->key = xstrdup((x)); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, g_memdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Escape", 27, 0);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Insert", KEY_IC, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Tab", 9, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

	return -1;
}

/*
 * ncurses_binding_set()
 *
 * it sets some sequence to the given key
 */
void ncurses_binding_set(int quiet, const char *key, const char *sequence)
{
	struct binding *d;
	binding_added_t *b;
	struct binding *binding_orginal = NULL;
	char *joined = NULL;
	int count = 0;

	for (d = bindings; d; d = d->next) {
		if (!xstrcasecmp(key, d->key)) {
			binding_orginal = d;
			break;
		}
	}

	if (!binding_orginal) {
		printq("bind_doesnt_exist", key);
		return;
	}

	if (!sequence) {
		char **chars = NULL;
		char ch;
		printq("bind_press_key");
		nodelay(input, FALSE);
		while ((ch = wgetch(input)) != ERR) {
			array_add(&chars, xstrdup(ekg_itoa(ch)));
			nodelay(input, TRUE);
			count++;
		}
		joined = g_strjoinv(" ", chars);
		g_strfreev(chars);
	} else
		joined = xstrdup(sequence);

	for (b = bindings_added; b; b = b->next) {
		if (!xstrcasecmp(b->sequence, joined)) {
			b->binding = binding_orginal;
			xfree(joined);
			goto end;
		}
	}

	b = xmalloc(sizeof(binding_added_t));
	b->sequence = joined;
	b->binding = binding_orginal;
	LIST_ADD2(&bindings_added, b);
end:
	if (!in_autoexec)
		config_changed = 1;
	printq("bind_added");
	if (count > bindings_added_max)
		bindings_added_max = count;
}

/*
 * ncurses_binding_add()
 *
 * przypisuje danemu klawiszowi akcj�.
 *
 *  - key - opis klawisza,
 *  - action - akcja,
 *  - internal - czy to wewn�trzna akcja interfejsu?
 *  - quiet - czy by� cicho i nie wy�wietla� niczego?
 */
void ncurses_binding_add(const char *key, const char *action, int internal, int quiet)
{
	struct binding b, *c = NULL, *d;

	if (!key || !action)
		return;

	memset(&b, 0, sizeof(b));

	b.internal = internal;

	for (d = bindings; d; d = d->next) {
		if (!xstrcasecmp(key, d->key)) {
			if (d->internal) {
				c = d;
				break;
			}
			printq("bind_seq_exist", d->key);
			return;
		}
	}

	binding_parse(&b, action);

	if (internal) {
		b.default_action	= xstrdup(b.action);
		b.default_function	= b.function;
		b.default_arg		= xstrdup(b.arg);
	}

	if (binding_key(&b, key, (c) ? 0 : 1)) {
		printq("bind_seq_incorrect", key);
		xfree(b.action);
		xfree(b.arg);
		xfree(b.default_action);
		xfree(b.default_arg);
		xfree(b.key);
	} else {
		printq("bind_seq_add", b.key);

		if (c) {
			xfree(c->action);
			c->action = b.action;
			xfree(c->arg);
			c->arg = b.arg;
			c->function = b.function;
			xfree(b.default_action);
			xfree(b.default_arg);
			xfree(b.key);
			c->internal = 0;
		}

		if (!in_autoexec)
			config_changed = 1;
	}
}

/*
 * ncurses_binding_delete()
 *
 * usuwa akcj� z danego klawisza.
 */
void ncurses_binding_delete(const char *key, int quiet)
{
	struct binding *b;

	if (!key)
		return;

	for (b = bindings; b; b = b->next) {
		int i;

		if (!b->key || xstrcasecmp(key, b->key))
			continue;

		if (b->internal) {
			printq("bind_seq_incorrect", key);
			return;
		}

		xfree(b->action);
		xfree(b->arg);

		if (b->default_action) {
			b->action	= xstrdup(b->default_action);
			b->arg		= xstrdup(b->default_arg);
			b->function	= b->default_function;
			b->internal	= 1;
		} else {
			xfree(b->key);
			for (i = 0; i < KEY_MAX + 1; i++) {
				if (ncurses_binding_map[i] == b)
					ncurses_binding_map[i] = NULL;
				if (ncurses_binding_map_meta[i] == b)
					ncurses_binding_map_meta[i] = NULL;

			}

			LIST_REMOVE2(&bindings, b, NULL);
		}

		config_changed = 1;

		printq("bind_seq_remove", key);

		return;
	}

	printq("bind_seq_incorrect", key);
}

/*
 * ncurses_binding_default()
 *
 * ustawia lub przywraca domy�lne ustawienia przypisanych klawiszy.
 */
QUERY(ncurses_binding_default)
{
	ncurses_binding_add("Alt-`", "/window switch 0", 1, 1);
	ncurses_binding_add("Alt-1", "/window switch 1", 1, 1);
	ncurses_binding_add("Alt-2", "/window switch 2", 1, 1);
	ncurses_binding_add("Alt-3", "/window switch 3", 1, 1);
	ncurses_binding_add("Alt-4", "/window switch 4", 1, 1);
	ncurses_binding_add("Alt-5", "/window switch 5", 1, 1);
	ncurses_binding_add("Alt-6", "/window switch 6", 1, 1);
	ncurses_binding_add("Alt-7", "/window switch 7", 1, 1);
	ncurses_binding_add("Alt-8", "/window switch 8", 1, 1);
	ncurses_binding_add("Alt-9", "/window switch 9", 1, 1);
	ncurses_binding_add("Alt-0", "/window switch 10", 1, 1);
	ncurses_binding_add("Alt-Q", "/window switch 11", 1, 1);
	ncurses_binding_add("Alt-W", "/window switch 12", 1, 1);
	ncurses_binding_add("Alt-E", "/window switch 13", 1, 1);
	ncurses_binding_add("Alt-R", "/window switch 14", 1, 1);
	ncurses_binding_add("Alt-T", "/window switch 15", 1, 1);
	ncurses_binding_add("Alt-Y", "/window switch 16", 1, 1);
	ncurses_binding_add("Alt-U", "/window switch 17", 1, 1);
	ncurses_binding_add("Alt-I", "/window switch 18", 1, 1);
	ncurses_binding_add("Alt-O", "/window switch 19", 1, 1);
	ncurses_binding_add("Alt-P", "/window switch 20", 1, 1);
	ncurses_binding_add("Alt-K", "window-kill", 1, 1);
	ncurses_binding_add("Alt-N", "/window new", 1, 1);
	ncurses_binding_add("Alt-A", "/window active", 1, 1);
	ncurses_binding_add("Alt-G", "ignore-query", 1, 1);
	ncurses_binding_add("Alt-B", "backward-word", 1, 1);
	ncurses_binding_add("Alt-F", "forward-word", 1, 1);
	ncurses_binding_add("Alt-D", "kill-word", 1, 1);
	ncurses_binding_add("Alt-Enter", "toggle-input", 1, 1);
	ncurses_binding_add("Escape", "cancel-input", 1, 1);
	ncurses_binding_add("Ctrl-N", "/window next", 1, 1);
	ncurses_binding_add("Ctrl-P", "/window prev", 1, 1);
	ncurses_binding_add("Backspace", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-H", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-A", "beginning-of-line", 1, 1);
	ncurses_binding_add("Home", "beginning-of-line", 1, 1);
	ncurses_binding_add("Ctrl-D", "delete-char", 1, 1);
	ncurses_binding_add("Delete", "delete-char", 1, 1);
	ncurses_binding_add("Ctrl-E", "end-of-line", 1, 1);
	ncurses_binding_add("End", "end-of-line", 1, 1);
	ncurses_binding_add("Ctrl-K", "kill-line", 1, 1);
	ncurses_binding_add("Ctrl-Y", "yank", 1, 1);
	ncurses_binding_add("Enter", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-M", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-U", "line-discard", 1, 1);
	ncurses_binding_add("Ctrl-V", "quoted-insert", 1, 1);
	ncurses_binding_add("Ctrl-W", "word-rubout", 1, 1);
	ncurses_binding_add("Alt-Backspace", "word-rubout", 1, 1);
	ncurses_binding_add("Ctrl-L", "/window refresh", 1, 1);
	ncurses_binding_add("Tab", "complete", 1, 1);
	ncurses_binding_add("Right", "forward-char", 1, 1);
	ncurses_binding_add("Left", "backward-char", 1, 1);
	ncurses_binding_add("Up", "previous-history", 1, 1);
	ncurses_binding_add("Down", "next-history", 1, 1);
	ncurses_binding_add("PageUp", "backward-page", 1, 1);
	ncurses_binding_add("Ctrl-F", "backward-page", 1, 1);
	ncurses_binding_add("PageDown", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-G", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-X", "cycle-sessions", 1, 1);
	ncurses_binding_add("F1", "/help", 1, 1);
	ncurses_binding_add("F2", "quick-list", 1, 1);
	ncurses_binding_add("F3", "toggle-contacts", 1, 1);
	ncurses_binding_add("F4", "next-contacts-group", 1, 1);
	ncurses_binding_add("F12", "/window switch 0", 1, 1);
	ncurses_binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
	/* ncurses_binding_add("Ctrl-Down", "forward-contacts-page", 1, 1);
	ncurses_binding_add("Ctrl-Up", "backward-contacts-page", 1, 1); */
	return 0;
}

void ncurses_binding_init()
{
	va_list dummy;

	memset(ncurses_binding_map, 0, sizeof(ncurses_binding_map));
	memset(ncurses_binding_map_meta, 0, sizeof(ncurses_binding_map_meta));

	ncurses_binding_default(NULL, dummy);
	ncurses_binding_complete	= binding_complete;
	ncurses_binding_accept_line	= binding_accept_line;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
