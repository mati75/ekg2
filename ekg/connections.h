/*
 *  Asynchronous read/write handling for connections
 *
 *  (C) Copyright 2011 EKG2 team
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

#ifndef __EKG_CONNECTIONS_H
#define __EKG_CONNECTIONS_H

#define EKG_CONNECTION_ERROR ekg_connection_error_quark()
G_GNUC_CONST GQuark ekg_connection_error_quark();

enum ekg_connection_error {
	EKG_CONNECTION_ERROR_EOF
};

typedef void (*ekg_input_callback_t) (
		GDataInputStream *instream,
		gpointer data);
typedef void (*ekg_failure_callback_t) (
		GDataInputStream *instream,
		GError *err,
		gpointer data);
typedef void (*ekg_connection_callback_t) (
		GSocketConnection *conn,
		GInputStream *instream,
		GOutputStream *outstream,
		gpointer data);
typedef void (*ekg_connection_failure_callback_t) (
		GError *err,
		gpointer data);

GDataOutputStream *ekg_connection_add(
		GSocketConnection *conn,
		GInputStream *rawinstream,
		GOutputStream *rawoutstream,
		ekg_input_callback_t callback,
		ekg_failure_callback_t failure_callback,
		gpointer priv_data);

void ekg_disconnect_by_outstream(GDataOutputStream *f);

void ekg_connection_write_buf(GDataOutputStream *f, gconstpointer buf, gsize len);

void ekg_connection_write(
		GDataOutputStream *f,
		const gchar *format,
		...) G_GNUC_PRINTF(2,3);

typedef struct ekg_connection_starter *ekg_connection_starter_t;

ekg_connection_starter_t ekg_connection_starter_new(guint16 defport);
void ekg_connection_starter_free(ekg_connection_starter_t cs);

void ekg_connection_starter_bind(
		ekg_connection_starter_t cs,
		const gchar *hostname);
void ekg_connection_starter_set_srv_resolver(
		ekg_connection_starter_t cs,
		const gchar *service,
		const gchar *domain);
void ekg_connection_starter_set_servers(
		ekg_connection_starter_t cs,
		const gchar *servers);
void ekg_connection_starter_set_use_tls(
		ekg_connection_starter_t cs,
		gboolean use_tls); /* XXX */

GCancellable *ekg_connection_starter_run(
		ekg_connection_starter_t cs,
		GSocketClient *sock,
		ekg_connection_callback_t callback,
		ekg_connection_failure_callback_t failure_callback,
		gpointer priv_data);

#endif /* __EKG_CONNECTIONS_H */
