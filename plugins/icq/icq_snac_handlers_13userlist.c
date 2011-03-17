/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wiesław Ochmiński <wiechu@wiechu.com>
 *
 * Protocol description with author's permission from: http://iserverd.khstu.ru/oscar/
 *  (C) Copyright 2000-2005 Alexander V. Shutko <AVShutko@mail.khstu.ru>
 *
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

#include "icq.h"
#include "misc.h"
#include "icq_caps.h"
#include "icq_const.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"


static SNAC_SUBHANDLER(icq_snac_userlist_error) {
	struct {
		guint16 error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		return -1;

	if (s->connected == 0)
		icq_session_connected(s);

	icq_snac_error_handler(s, "userlist", pkt.error);
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_reply) {
	/* SNAC(13,03) SRV_SSI_RIGHTS_REPLY	Service parameters reply
	 *
	 * Server replies with this SNAC to SNAC(13,02) - client SSI service parameters request.
	 */
#if ICQ_DEBUG_UNUSED_INFORMATIONS
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;

	debug_function("icq_snac_userlist_reply()\n");

	tlvs = icq_unpack_tlvs(&buf, &len, 0);

	if ((t = icq_tlv_get(tlvs, 0x06)) && (t->len == 2))
		debug_white("icq_snac_userlist_reply() length limit for server-list item's name = %d\n", t->nr);
	if ((t = icq_tlv_get(tlvs, 0x0C)) && (t->len == 2))
		debug_white("icq_snac_userlist_reply() max number of contacts in a group = %d\n", t->nr);
	if ((t = icq_tlv_get(tlvs, 0x04))) {
		guint16 a, b, c, d, e;
		if (icq_unpack_nc(t->buf, t->len, "WWWW 20 W", &a, &b, &c, &d, &e))
			debug_white("icq_snac_userlist_reply() Max %d contacts, %d groups; %d visible contacts, %d invisible contacts, %d ignore items.\n", a, b, c, d, e);
	}

	icq_tlvs_destroy(&tlvs);
#endif

	return 0;
}

static void set_userinfo_from_tlv(userlist_t *u, const char *var, icq_tlv_t *t) {
	char *tmp;

	if (!u)
		return;

	tmp = (t && t->len) ? ekg_utf8_to_core(xstrndup((char *) t->buf, t->len)) : NULL;

	user_private_item_set(u, var, tmp);
}

static int icq_userlist_parse_entry(session_t *s, struct icq_tlv_list *tlvs, const char *name, guint16 type, guint16 item_id, guint16 group_id) {
	switch (type) {
		case 0x0000:	/* Buddy record (name: uin for ICQ and screenname for AIM) */
		{
			icq_tlv_t *t_nick = icq_tlv_get(tlvs, 0x131);
			icq_tlv_t *t_auth = icq_tlv_get(tlvs, 0x66);

			char *nick, *tmp;
			char *uid = icq_uid(name);
			userlist_t *u;

			tmp = (t_nick && t_nick->len) ? xstrndup((char *) t_nick->buf, t_nick->len) : xstrdup(uid);
			nick = ekg_utf8_to_core(tmp);

			if (!(u = userlist_find(s, uid)) && !(u = userlist_add(s, uid, nick)) ) {
				debug_error("icq_userlist_parse_entry() userlist_add(%s, %s) failed!\n", uid, nick);
				goto cleanup_user;
			}

			if (!u->nickname)
				u->nickname = xstrdup(nick);

			set_userinfo_from_tlv(u, "email",	icq_tlv_get(tlvs, 0x0137));
			set_userinfo_from_tlv(u, "phone",	icq_tlv_get(tlvs, 0x0138));	// phone number
			set_userinfo_from_tlv(u, "cellphone",	icq_tlv_get(tlvs, 0x0139));	// cellphone number
			set_userinfo_from_tlv(u, "mobile",	icq_tlv_get(tlvs, 0x013A));	// SMS number
			set_userinfo_from_tlv(u, "comment",	icq_tlv_get(tlvs, 0x013C));

			if (group_id) {
				user_private_item_set_int(u, "iid", item_id);
				user_private_item_set_int(u, "gid", group_id);
			}

			if (t_auth) {
				user_private_item_set_int(u, "auth", 1);
				u->status = EKG_STATUS_UNKNOWN;
			} else
				user_private_item_set_int(u, "auth", 0);
cleanup_user:
			xfree(nick);
			xfree(uid);
			break;
		}

		case 0x0001:	/* Group record */
		{
			if (item_id == 0) {
				if ((group_id == 0)) {
					/* list of groups. wTlvType=1, data is TLV(C8) containing list of WORDs which */
					/* is the group ids */
					/* we don't need to use this. Our processing is on-the-fly */
					/* this record is always sent first in the first packet only, */
				} else {
					/* wGroupId != 0: a group record */
					/* no item ID: this is a group */
					/* XXX ?wo? more groups ??? */
					icq_private_t *j;
					if (s && (j = s->priv) && (!j->default_group_id)) {
						j->default_group_id = group_id;
						j->default_group_name = xstrdup(name);
					}
				}
			} else {
				debug_error("icq_userlist_parse_entry() Unhandled ROSTER_TYPE_GROUP wItemID != 0\n");
			}

			break;
		}

		case 0x0002:	/* Permit record ("Allow" list in AIM, and "Visible" list in ICQ) */
		case 0x0003:	/* Deny record ("Block" list in AIM, and "Invisible" list in ICQ) */
		{
			char *uid = icq_uid(name);
			userlist_t *u;
			if (!(u = userlist_find(s, uid)))
				u = userlist_add(s, uid, NULL);
			xfree(uid);

			if (!u)
				break;

			if (type == 2) {
				user_private_item_set_int(u, "visible", item_id);
				user_private_item_set_int(u, "invisible", 0);
				ekg_group_add(u, "__online");
				ekg_group_remove(u, "__offline");
			} else {
				user_private_item_set_int(u, "visible", 0);
				user_private_item_set_int(u, "invisible", item_id);
				ekg_group_add(u, "__offline");
				ekg_group_remove(u, "__online");
			}
			break;
		}

		case 0x0004: /* Permit/deny settings or/and bitmask of the AIM classes */
		{
			/* XXX */
			break;
		}

		case 0x0005: /* Presence info (if others can see your idle status, etc) */
		{
			/* XXX */
			break;
		}

		case 0x0009: /* ICQ2k shortcut bar items */
		{
			if (group_id == 0) {
				/* data is TLV(CD) text */
			}
			break;
		}

		case 0x000e:	/* Ignore list record */
		{
			char *uid = icq_uid(name);
			userlist_t *u;

			if (!(u = userlist_find(s, uid)))
				u = userlist_add(s, uid, NULL);

			if (u) {
				user_private_item_set_int(u, "block", item_id);
				ekg_group_add(u, "__blocked");
			}

			xfree(uid);
			break;
		}

		case 0x000F:	/* Last update date (name: "LastUpdateDate") */
		case 0x0010:	/* Non-ICQ contact (to send SMS). Name: 1#EXT, 2#EXT, etc */
		{
			/* XXX */
			break;
		}

		case 0x0013:	/* Item that contain roster import time (name: "Import time") */
		{
			if (group_id == 0)
			{
				/* time our list was first imported */
				/* pszRecordName is "Import Time" */
				/* data is TLV(13) {TLV(D4) {time_t importTime}} */

				/* XXX */
			}
			break;
		}

		case 0x0014:	/* Own icon (avatar) info. Name is an avatar id number as text */
		{
			if (group_id == 0)
			{
				/* our avatar MD5-hash */
				/* pszRecordName is "1" */
				/* data is TLV(D5) hash */
				/* we ignore this, just save the id */
				/* cause we get the hash again after login */

				/* XXX */
			}
			break;
		}

		case 0x0019: /* deleted */
		{
#ifdef ICQ_DEBUG_UNUSED_INFORMATIONS
			icq_tlv_t *t;
			if ((t = icq_tlv_get(tlvs, 0x6d)) && (t->len == 8)) {
				int t1,t2;
				if (icq_unpack_nc(t->buf, t->len, "II", &t1, &t2)) {
					debug_white("'%s' was deleted %s\n", name, timestamp_time("%Y-%m-%d %H:%M:%S", t1));
				}
			}
#endif
			break;
		}

		case 0x001d:
		case 0x0020:
		case 0x0028:
		{ /* XXX unknown, but friendly types */
			break;
		}

		default:
			 debug_error("icq_userlist_parse_entry() unknown type: 0x%.4x\n", type);
	}
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_roster) {
	/*
	 * Handle SNAC(0x13, 0x6) -- Server contact list reply
	 *
	 * This is the server reply to client roster requests: SNAC(13,04), SNAC(13,05).
	 *
	 * Server can split up the roster in several parts. This is indicated with
	 * SNAC flags bit 1 as usual, however the "SSI list last change time" only
	 * exists in the last packet. And the "Number of items" field indicates the
	 * number of items in the current packet, not the entire list.
	 *
	 */

	struct {
		guint8 unk1;		/* empty */	/* possible version */
		guint16 count;		/* COUNT */
		unsigned char *data;
	} pkt;
	int i;

	/* XXX, here we have should check if we send roster request with that ref.... if not, "Unrequested roster packet received.\n" return; */

	if (!ICQ_UNPACK(&pkt.data, "CW", &pkt.unk1, &pkt.count))
		return -1;

	debug_function("icq_snac_userlist_roster() contacts count: %d\n", pkt.count);
	buf = pkt.data;

	for (i = 0; i < pkt.count; i++) {
		char *orgname;
		guint16 item_id, item_type;
		guint16 group_id;
		guint16 tlv_len;
		struct icq_tlv_list *tlvs;
		char *name;

		if (!ICQ_UNPACK(&buf, "UWWWW", &orgname, &group_id, &item_id, &item_type, &tlv_len))
			return -1;

		if (len < tlv_len) {
			debug_error("smth bad!\n");
			return -1;
		}

		name = ekg_utf8_to_core_dup(orgname);
		debug_white("%sName:'%s'\tgroup:%u item:%u type:0x%x tlvLEN:%u\n", item_type==1?"Group ":"", name, group_id, item_id, item_type, tlv_len);

		tlvs = icq_unpack_tlvs_nc(buf, tlv_len, 0);
		icq_userlist_parse_entry(s, tlvs, name, item_type, item_id, group_id);
		icq_tlvs_destroy(&tlvs);

		xfree(name);

		buf += tlv_len; len -= tlv_len;

	}

	if (len >= 4) {
		guint32 last_update;

		if (!ICQ_UNPACK(&buf, "I", &last_update))
				return -1;

		debug("icq_snac_userlist_roster() Last update of server list was (%u) %s\n",
					last_update, timestamp_time("%d/%m/%y %H:%M:%S", last_update));

			/* sendRosterAck() */
		icq_send_snac(s, 0x13, 0x07, 0, 0, "");

		icq_session_connected(s);

	} else {
		debug("icq_snac_userlist_roster() Waiting for more packets...");
	}

	if (len>0)
		debug_error("icq_snac_userlist_roster() left: %u bytes\n", len);

	return 0;
}

static char *icq_snac_userlist_edit_ack_msg(int error) {
	char *msg;

	switch (error) {
		case 0x0000: msg = "OK!"; break;
		case 0x0002: msg = "Item you want to modify not found in list"; break;
		case 0x0003: msg = "Item you want to add allready exists"; break;
		case 0x000A: msg = "Error adding item (invalid id, allready in list, invalid data)"; break;
		case 0x000C: msg = "Can't add item. Limit for this type of items exceeded"; break;
		case 0x000D: msg = "Trying to add ICQ contact to an AIM list"; break;
		case 0x000E: msg = "Can't add this contact because it requires authorization";	break;
		default:     msg = "Unknown error";
	}

	return msg;
}

static SNAC_SUBHANDLER(icq_snac_userlist_edit_ack) {
	/*
	 * SNAC(13,0E)	SRV_SSIxMODxACK -- SSI edit server ack
	 *
	 * This SNAC is an ack sent by the server when adding a buddy,
	 * deleting a buddy, or otherwise modifying a group.
	 */
	guint16 err;

	debug_function("icq_snac_userlist_edit_ack()\n");
	while (len>=2) {
		if (!ICQ_UNPACK(&buf, "W", &err))
			break;
		if (!err)
			debug_white("icq_snac_userlist_edit_ack() err:0 // OK!\n");
		else
			debug_error("icq_snac_userlist_edit_ack() Error code:0x%x // %s\n", err, icq_snac_userlist_edit_ack_msg(err));
	}

	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_up_to_date) {
	/*
	 * SNAC(13,0F)	SRV_SSI_UPxTOxDATE -- client local SSI is up-to-date
	 *
	 * Server send this snac as reply for SNAC(13,05) when server-stored
	 * information has the same modification date and items number.
	 */
	/* XXX "I" - modification date/time of client local SSI copy, "W" - number of items in client local SSI copy */
	debug_function("icq_snac_userlist_up_to_date()\n");
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_modifystart) {
	/*
	 * SNAC(13,11)	CLI_SSI_EDIT_BEGIN -- Contacts edit start (begin transaction)
	 *
	 * Use this before server side information (SSI) modification. Also
	 * you should send SNAC(13,12) after SSI modification. You could also
	 * use "import" transaction mode to add contacts requiring
	 * authorization. Just add 0x00010000 to snac data to start import
	 * transaction.
	 */
	/* XXX empty (or 0x00010000 for import transaction) */
	debug_white("icq_snac_userlist_modifystart() Server is modifying contact list\n");
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_modifyentry) {
	/*
	 * SNAC(13,09)  	CLI_SSIxUPDATE
	 *
	 * This can be used to modify either the name or additional data for
	 * any items that are already in your server-stored information. It
	 * is most commonly used after adding or removing a buddy: you should
	 * either add or remove the buddy ID# from the type 0x00c9 TLV in the
	 * additional data of the parent group, and then send this SNAC
	 * containing the modified data. Server should reply via SNAC(13,0E).
	 */
	debug_function("icq_snac_userlist_modifyentry() Server updated our contact on list\n");

	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_removeentry) {
	/*
	 * SNAC(13,0A)	CLI_SSIxDELETE -- SSI edit: remove item
	 *
	 * Client use this to delete items from server-side info. Server
	 * should reply via SNAC(13,0E).
	 */
	debug_function("icq_snac_userlist_removeentry() Server updated our contact on list\n");
#if ICQ_DEBUG_UNUSED_INFORMATIONS
{
	struct {
		char *uid;
		guint16 group, item;
		guint16 type;
	} pkt;
	if (ICQ_UNPACK(&buf, "UWWW", &pkt.uid, &pkt.group, &pkt.item, &pkt.type)) {
		debug("icq_snac_userlist_removeentry() Details: delete '%s' (item id:%d, type:0x%x) from group %d\n", pkt.uid, pkt.item, pkt.type, pkt.group);
	}
}
#endif
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_modifyend) {
	/*
	 * SNAC(13,12)	CLI_SSI_EDIT_END -- Contacts edit end (finish transaction)
	 *
	 * This snac used after SSI modification to commit transaction started by SNAC(13,11).
	 * See also snac list for SSI service here.
	 */
	debug_white("icq_snac_userlist_modifyend() End of server modification\n");
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_future_auth) {
	/*
	 * SNAC(13,15) SRV_SSI_FUTURExAUTHxGRANTED -- Future authorization granted
	 *
	 * You'll receive this when somebody grants future authorization to
	 * you. You can use SNAC(13,14) to send such authorization grant.
	 */
	struct {
		char *uid;
		char *reason;
	} pkt;

	if (!ICQ_UNPACK(&buf, "u", &pkt.uid))
		return -1;

	if (!ICQ_UNPACK(&buf, "U", &pkt.reason))
		pkt.reason = "";

	/* XXX, pkt.reason recode */
	debug_function("icq_snac_userlist_future_auth() %s reason: %s\n", pkt.uid, pkt.reason);
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_auth_req) {
	/*
	 * SNAC(13,19)	SRV_SSI_AUTHxREQUEST -- Authorization request
	 *
	 * This is the authorization request from another client because it
	 * wants to add you to its contact list. You may just ignore this
	 * request or you could reply via SNAC(13,1A) - authorization reply.
	 */
	struct {
		char *uid;
		char *reason;
	} pkt;
	char *uid;

	if (!ICQ_UNPACK(&buf, "u", &pkt.uid))
		return -1;

	if (!ICQ_UNPACK(&buf, "U", &pkt.reason))
		pkt.reason = "";

	/* XXX, pkt.reason recode */

	uid = icq_uid(pkt.uid);
	debug_function("icq_snac_userlist_auth_req() %s reason: %s\n", uid, pkt.reason);
	print("icq_auth_subscribe", session_name(s), uid, pkt.reason);
	xfree(uid);
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_auth_reply) {
	/*
	 * SNAC(13,1B)	SRV_SSI_AUTHxREPLY -- Authorization reply
	 *
	 * This snac received when someone grant or declines you
	 * authorization, sent via SNAC(13,18). It contain screen name (uin)
	 * of the user, result flag and reason string.
	 */
	struct {
		char *uid;
		guint8 flag;
		char *reason;
	} pkt;
	char *uid;

	if (!ICQ_UNPACK(&buf, "u", &pkt.uid))
		return -1;

	uid = icq_uid(pkt.uid);

	if (ICQ_UNPACK(&buf, "c", &pkt.flag)) {
		if (!ICQ_UNPACK(&buf, "U", &pkt.reason))
			pkt.reason = "";
		/* XXX, pkt.reason recode */
		/* XXX, spammer */
		switch (pkt.flag) {
			case 0:
			case 1:
			{
				userlist_t *u = userlist_find(s, uid);
				if (u && pkt.flag)
					user_private_item_set_int(u, "auth", 0);

				print_info(uid, s, pkt.flag ? "icq_auth_grant" : "icq_auth_decline", session_name(s), format_user(s, uid), pkt.reason);

				break;
			}
			default:
				debug_error("icq_snac_userlist_auth_reply() unknown response: %u from %s. (Reason: %s)\n", pkt.flag, uid, pkt.reason);
				break;
		}
	} else
		debug_error("icq_snac_userlist_auth_reply() (%s) Corrupt answer from %s\n", session_name(s), uid);

	xfree(uid);
	return 0;
}

static SNAC_SUBHANDLER(icq_snac_userlist_you_were_added) {
	/*
	 * SNAC(13,1C)	SRV_SSI_YOUxWERExADDED -- "You were added" message
	 *
	 * Server send this snac to clients, that announced the use of family
	 * 0x13 in SNAC(01,17). This is the "you-were-added" message meaning
	 * that somebody (snac contain his/her screenname) added you to his/her roster.
	 */
	struct {
		char *uid;
	} pkt;
	char *uid;
	userlist_t *u;

	if (!ICQ_UNPACK(&buf, "u", &pkt.uid))
		return -1;

	uid = icq_uid(pkt.uid);

	print_info(uid, s, "icq_you_were_added", session_name(s), format_user(s, uid));

	if ( config_auto_user_add && !(u = userlist_find(s, uid)) )
		u = userlist_add(s, uid, uid);				/* XXX "/addssi uid" */

	xfree(uid);

	return 0;
}

SNAC_HANDLER(icq_snac_userlist_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_userlist_error; break;
		case 0x03: handler = icq_snac_userlist_reply; break;		/* Miranda: OK */
		case 0x06: handler = icq_snac_userlist_roster; break;		/* Miranda: 1/3 OK */	/* XXX, handleServerCList() */
		case 0x09: handler = icq_snac_userlist_modifyentry; break;
		case 0x0a: handler = icq_snac_userlist_removeentry; break;
		case 0x0E: handler = icq_snac_userlist_edit_ack; break;
		case 0x0F: handler = icq_snac_userlist_up_to_date; break;
		case 0x11: handler = icq_snac_userlist_modifystart; break;	/* Miranda: OK */
		case 0x12: handler = icq_snac_userlist_modifyend; break;	/* Miranda: OK */
		case 0x15: handler = icq_snac_userlist_future_auth; break;
		case 0x19: handler = icq_snac_userlist_auth_req; break;
		case 0x1B: handler = icq_snac_userlist_auth_reply; break;
		case 0x1C: handler = icq_snac_userlist_you_were_added; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_userlist_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

SNAC_SUBHANDLER(icq_cmd_addssi_ack) {
	userlist_t *u;
	const char *nick = private_item_get(&data, "nick");
	const char *cmd  = private_item_get(&data, "cmd");
	int quiet = private_item_get_int(&data, "quiet");
	guint16 error;
	char *uid;

	if (!ICQ_UNPACK(&buf, "W", &error))
		return -1;

	uid = icq_uid(private_item_get(&data, "uid"));

	if (error) {
		/* XXX ?wo? format for it */
		char *tmp = saprintf("Can't add %s/%s", nick, uid);
		printq("icq_user_info_generic", tmp, icq_snac_userlist_edit_ack_msg(error));
		xfree(tmp);
		xfree(uid);
		return -1;
	}

	if (!xstrcmp(cmd, "del")) {
		if ( (u = userlist_find(s, uid)) ) {
			char *tmp = xstrdup(u->nickname);
			printq("user_deleted", u->nickname, session_name(s));

			tabnick_remove(u->uid);
			tabnick_remove(u->nickname);

			userlist_remove(s, u);

			query_emit(NULL, "userlist-removed", &tmp, &uid);
			query_emit(NULL, "remove-notify", &s->uid, &uid);

			xfree(tmp);
		}
	} else {
		// add or modify
		if (!xstrcmp(cmd, "add")) {
			if ( (u = userlist_add(s, uid, nick)) ) {
				printq("user_added", u->nickname, session_name(s));
				query_emit(NULL, "userlist-added", &u->uid, &u->nickname, &quiet);
				query_emit(NULL, "add-notify", &s->uid, &u->uid);
			}
		} else {
			// modify
			if ( (u = userlist_find(s, uid)) ) {
				const char *nick = private_item_get(&data, "nick");
				if (nick) {

					query_emit(NULL, "userlist-renamed", &u->nickname, &nick);

					xfree(u->nickname);
					u->nickname = xstrdup(nick);

					userlist_replace(s, u);
					query_emit(NULL, "userlist-refresh");
				}
			}
		}
		if (u) {
			user_private_item_set_int(u, "iid", private_item_get_int(&data, "iid"));
			user_private_item_set_int(u, "gid", private_item_get_int(&data, "gid"));
			const char *p;
			p = private_item_get(&data, "mobile"); if (p) user_private_item_set(u, "mobile", p);
			p = private_item_get(&data, "email"); if (p) user_private_item_set(u, "email", p);
			p = private_item_get(&data, "comment"); if (p) user_private_item_set(u, "comment", p);
		}
	}

	xfree(uid);

	return 0;
}

// vim:syn=c
