/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *			    Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <glib/gstdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

msg_queue_t *msgs_queue = NULL;

static LIST_FREE_ITEM(list_msg_queue_free, msg_queue_t *) { xfree(data->session); xfree(data->rcpts); xfree(data->message); xfree(data->seq); }

DYNSTUFF_LIST_DECLARE(msgs_queue, msg_queue_t, list_msg_queue_free,
		__DYNSTUFF_LIST_ADD,		/* msgs_queue_add() */
		__DYNSTUFF_LIST_REMOVE_ITER,	/* msgs_queue_removei() */
		__DYNSTUFF_LIST_DESTROY)	/* msgs_queue_destroy() */

/*
 * msg_queue_add()
 *
 * dodaje wiadomo�� do kolejki wiadomo�ci.
 * 
 *  - session - sesja, z kt�rej wysy�ano
 *  - rcpts - lista odbiorc�w
 *  - message - tre�� wiadomo�ci
 *  - seq - numer sekwencyjny
 *
 * 0/-1
 */
int msg_queue_add(const char *session, const char *rcpts, const char *message, const char *seq, msgclass_t mclass)
{
	msg_queue_t *m = xmalloc(sizeof(msg_queue_t));

	m->session	= xstrdup(session);
	m->rcpts	= xstrdup(rcpts);
	m->message	= xstrdup(message);
	m->seq		= xstrdup(seq);
	m->time		= time(NULL);
	m->mclass	= mclass;

	msgs_queue_add(m);
	return 0;
}


/*
 * msg_queue_remove_uid()
 *
 * usuwa wiadomo�� z kolejki wiadomo�ci dla danego
 * u�ytkownika.
 *
 *  - uin.
 *
 * 0 je�li usuni�to, -1 je�li nie ma takiej wiadomo�ci.
 */
int msg_queue_remove_uid(const char *uid)
{
	msg_queue_t *m;
	int res = -1;

	for (m = msgs_queue; m; m = m->next) {
		if (!xstrcasecmp(m->rcpts, uid)) {
			m = msgs_queue_removei(m);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_remove_seq()
 *
 * usuwa wiadomo�� z kolejki wiadomo�� o podanym numerze sekwencyjnym.
 *
 *  - seq
 *
 * 0/-1
 */
int msg_queue_remove_seq(const char *seq)
{
	int res = -1;
	msg_queue_t *m;

	if (!seq) 
		return -1;

	for (m = msgs_queue; m; m = m->next) {
		if (!xstrcasecmp(m->seq, seq)) {
			m = msgs_queue_removei(m);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_flush()
 *
 * wysy�a wiadomo�ci z kolejki.
 *
 * 0 je�li wys�ano, -1 je�li nast�pi� b��d przy wysy�aniu, -2 je�li
 * kolejka pusta.
 */
int msg_queue_flush(const char *session)
{
	msg_queue_t *m;
	int ret = -1;

	if (!msgs_queue)
		return -2;

	for (m = msgs_queue; m; m = m->next)
		m->mark = 1;

	for (m = msgs_queue; m; m = m->next) {
		session_t *s;
		char *cmd = "/msg \"%s\" %s";

		/* czy wiadomo�� dodano w trakcie opr�niania kolejki? */
		if (!m->mark)
			continue;

		if (session && xstrcmp(m->session, session)) 
			continue;
				/* wiadomo�� wysy�ana z nieistniej�cej ju� sesji? usuwamy. */
		else if (!(s = session_find(m->session))) {
			m = msgs_queue_removei(m);
			continue;
		}

		switch (m->mclass) {
			case EKG_MSGCLASS_SENT_CHAT:
				cmd = "/chat \"%s\" %s";
				break;
			case EKG_MSGCLASS_SENT:
				break;
			default:
				debug_error("msg_queue_flush(), unsupported message mclass in query: %d\n", m->mclass);
		}
		command_exec_format(NULL, s, 1, cmd, m->rcpts, m->message);

		m = msgs_queue_removei(m);
		ret = 0;
	}

	return ret;
}

/*
 * msg_queue_count_session()
 *
 * zwraca liczb� wiadomo�ci w kolejce dla danej sesji.
 *
 * - uin.
 */
int msg_queue_count_session(const char *uid)
{
	msg_queue_t *m;
	int count = 0;

	for (m = msgs_queue; m; m = m->next) {
		if (!xstrcasecmp(m->session, uid))
			count++;
	}

	return count;
}

/*
 * msg_queue_write()
 *
 * zapisuje niedostarczone wiadomo�ci na dysku.
 *
 * 0/-1
 */
int msg_queue_write()
{
	msg_queue_t *m;
	int num = 0;

	if (!msgs_queue)
		return -1;

	if (mkdir_recursive(prepare_pathf("queue"), 1))		/* create ~/.ekg2/[PROFILE/]queue/ */
		return -1;

	for (m = msgs_queue; m; m = m->next) {
		const char *fn;
		GFile *f;
		GFileOutputStream *fs;

		if (!(fn = prepare_pathf("queue/%ld.%d", (long) m->time, num++)))	/* prepare_pathf() ~/.ekg2/[PROFILE/]queue/TIME.UNIQID */
			continue;

		f = g_file_new_for_path(fn);
		fs = g_file_replace(f, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL);
		g_object_unref(f);

		if (!fs)
			continue;

		ekg_fprintf(G_OUTPUT_STREAM(fs), "v2\n%s\n%s\n%ld\n%s\n%d\n%s", m->session, m->rcpts, m->time, m->seq, m->mclass, m->message);
		g_object_unref(fs);
	}

	return 0;
}

/**
 * msg_queue_read()
 *
 * Read msgqueue of not sended messages.<br>
 * msgqueue is subdir ("queue") in ekg2 config directory.
 *
 * @todo	return count of readed messages?
 *
 * @todo	code which handle errors is awful and it need rewriting.
 *
 * @return	-1 if fail to open msgqueue directory<br>
 *		 0 on success.
 */

int msg_queue_read() {
	struct dirent *d;
	DIR *dir;

	if (!(dir = opendir(prepare_pathf("queue"))))		/* opendir() ~/.ekg2/[PROFILE/]/queue */
		return -1;

	while ((d = readdir(dir))) {
		const char *fn;

		msg_queue_t m;
		string_t msg;
		char *buf;
		GFile *f;
		GFileInputStream *fs;
		GDataInputStream *fd;
		int filever = 0;

		if (!(fn = prepare_pathf("queue/%s", d->d_name)))
			continue;

		f = g_file_new_for_path(fn);
		fs = g_file_read(f, NULL, NULL);
		g_object_unref(f);

		if (!fs)
			continue;

		fd = g_data_input_stream_new(G_INPUT_STREAM(fs));

		memset(&m, 0, sizeof(m));

		do {
			buf = read_line(fd);
		} while (buf && (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/')));
		/* Allow leading comments */
		
		if (buf && *buf == 'v')
			filever = atoi(buf+1);
		if (!filever || filever > 2) {
			g_object_unref(fd);
			continue;
		}

		if (!(m.session = g_strdup(read_line(fd)))) {
			g_object_unref(fd);
			continue;
		}
	
		if (!(m.rcpts = g_strdup(read_line(fd)))) {
			xfree(m.session);
			g_object_unref(fd);
			continue;
		}

		if (!(buf = read_line(fd))) {
			xfree(m.session);
			xfree(m.rcpts);
			g_object_unref(fd);
			continue;
		}

		m.time = atoi(buf);

		if (!(m.seq = g_strdup(read_line(fd)))) {
			xfree(m.session);
			xfree(m.rcpts);
			g_object_unref(fd);
			continue;
		}
	
		if (filever == 2) {
			if (!(buf = read_line(fd))) {
				xfree(m.session);
				xfree(m.rcpts);
				g_object_unref(fd);
				continue;
			}

			m.mclass = atoi(buf);
		} else
			m.mclass = EKG_MSGCLASS_SENT;

		msg = string_init(NULL);

		buf = read_line(fd);

		while (buf) {
			string_append(msg, buf);
			buf = read_line(fd);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.message = string_free(msg, 0);

		msgs_queue_add(g_memdup(&m, sizeof(m)));

		g_object_unref(fd);
		g_unlink(fn);
	}

	closedir(dir);

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
