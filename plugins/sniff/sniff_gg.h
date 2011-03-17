/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Woźny <speedy@ziew.org>
 *			    Arkadiusz Miśkiewicz <arekm@pld-linux.org>
 *			    Tomasz Chiliński <chilek@chilan.com>
 *			    Piotr Wysocki <wysek@linux.bydg.org>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *
 *			& many others look at: http://ekg.chmurka.net/docs/protocol.html
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/* struct data && magic constants copied from libgadu (http://toxygen.net/libgadu) && 
 *	gg protocol documentation (http://ekg.chmurka.net/docs/protocol.html)
 *	great job guys! thx.
 */

#include <sys/types.h>

#define GG_PACKED __attribute__ ((packed))

typedef struct {
	guint32 type;			/* typ pakietu */
	guint32 len;		/* długość reszty pakietu */
	char data[];
} GG_PACKED gg_header;

#define GG_RECV_MSG 0x000a
typedef struct {
	guint32 sender;
	guint32 seq;
	guint32 time;
	guint32 msgclass;
	char msg_data[];
} GG_PACKED gg_recv_msg;

#define GG_SEND_MSG 0x000b
typedef struct {
	guint32 recipient;
	guint32 seq;
	guint32 msgclass;
	char msg_data[];
} GG_PACKED gg_send_msg;

#define GG_WELCOME 0x0001
typedef struct {
	guint32 key;
} GG_PACKED gg_welcome;

#define GG_SEND_MSG_ACK 0x0005
typedef struct {
	guint32 status;
	guint32 recipient;
	guint32 seq;
} GG_PACKED gg_send_msg_ack;

#define GG_PING 0x0008
#define GG_PONG 0x0007

#define GG_STATUS 0x0002
typedef struct {
	guint32 uin;			/* numerek */
	guint32 status;		/* nowy stan */
	char status_data[];
} GG_PACKED gg_status;

#define GG_NEW_STATUS 0x0002
typedef struct {
	guint32 status;			/* na jaki zmienić? */
	char status_data[];
} GG_PACKED gg_new_status;

#define GG_LOGIN_OK 0x0003
#define GG_LIST_EMPTY 0x0012

#define GG_STATUS60 0x000f
	
typedef struct {
	guint32 uin;			/* numerek plus flagi w MSB */
	guint8 status;			/* status danej osoby */
	guint32 remote_ip;		/* adres ip delikwenta */
	guint16 remote_port;		/* port, na którym słucha klient */
	guint8 version;		/* wersja klienta */
	guint8 image_size;		/* maksymalny rozmiar grafiki w KiB */
	guint8 dunno1;			/* 0x00 */
	char status_data[];
} GG_PACKED gg_status60;

#define GG_NEED_EMAIL 0x0014

#define GG_LOGIN60 0x0015

typedef struct {
	guint32 uin;			/* mój numerek */
	guint32 hash;			/* hash hasła */
	guint32 status;		/* status na dzień dobry */
	guint32 version;		/* moja wersja klienta */
	guint8 dunno1;			/* 0x00 */
	guint32 local_ip;		/* mój adres ip */
	guint16 local_port;		/* port, na którym słucham */
	guint32 external_ip;		/* zewnętrzny adres ip */
	guint16 external_port;		/* zewnętrzny port */
	guint8 image_size;		/* maksymalny rozmiar grafiki w KiB */
	guint8 dunno2;			/* 0xbe */
	char status_data[];
} GG_PACKED gg_login60;

#define GG_ADD_NOTIFY 0x000d
#define GG_REMOVE_NOTIFY 0x000e

typedef struct {
	guint32 uin;			/* numerek */
	guint8 dunno1;			/* bitmapa */
} GG_PACKED gg_add_remove;

#define GG_NOTIFY_REPLY60 0x0011
typedef struct {
	guint32 uin;			/* numerek plus flagi w MSB */
	guint8 status;			/* status danej osoby */
	guint32 remote_ip;		/* adres ip delikwenta */
	guint16 remote_port;		/* port, na którym słucha klient */
	guint8 version;		/* wersja klienta */
	guint8 image_size;		/* maksymalny rozmiar grafiki w KiB */
	guint8 dunno1;			/* 0x00 */
	unsigned char next[];		/* nastepny, lub DLUGOSC_OPISU+OPIS */
} GG_PACKED gg_notify_reply60;

#define GG_NOTIFY_FIRST 0x000f
#define GG_NOTIFY_LAST 0x0010

#define GG_NOTIFY 0x0010
	
typedef struct {
	guint32 uin;				/* numerek danej osoby */
	guint8 dunno1;				/* rodzaj wpisu w liście */
	char data[];
} GG_PACKED gg_notify;

#define GG_LOGIN70 0x19
#define GG_LOGIN_HASH_GG32 0x01
#define GG_LOGIN_HASH_SHA1 0x02

typedef struct {
	guint32 uin;			/* mój numerek */
	guint8 hash_type;		/* rodzaj hashowania hasła */
	guint8 hash[64];		/* hash hasła dopełniony zerami */
	guint32 status;		/* status na dzień dobry */
	guint32 version;		/* moja wersja klienta */
	guint8 dunno1;			/* 0x00 */
	guint32 local_ip;		/* mój adres ip */
	guint16 local_port;		/* port, na którym słucham */
	guint32 external_ip;		/* zewnętrzny adres ip (???) */
	guint16 external_port;		/* zewnętrzny port (???) */
	guint8 image_size;		/* maksymalny rozmiar grafiki w KiB */
	guint8 dunno2;			/* 0xbe */
	char status_data[];
} GG_PACKED gg_login70;

#define GG_USERLIST_REPLY 0x0010
typedef struct {
	guint8 type;
	char data[];
} GG_PACKED gg_userlist_reply;

#define GG_USERLIST_REQUEST 0x0016

typedef struct {
	guint8 type;
	char data[];
} GG_PACKED gg_userlist_request;

#define GG_PUBDIR50_REQUEST 0x0014

typedef struct {
	guint8 type;			/* GG_PUBDIR50_* */
	guint32 seq;			/* czas wysłania zapytania */
	char data[];
} GG_PACKED gg_pubdir50_request;

#define GG_PUBDIR50_REPLY 0x000e

typedef struct {
	guint8 type;			/* GG_PUBDIR50_* */
	guint32 seq;			/* czas wysłania zapytania */
	char data[];
} GG_PACKED gg_pubdir50_reply;

#define GG_DISCONNECTING 0x000b

#define GG_STATUS77 0x17
typedef struct {
	guint32 uin;			/* [gg_status60] numerek plus flagi w MSB */
	guint8 status;			/* [gg_status60] status danej osoby */
	guint32 remote_ip;		/* [XXX] adres ip delikwenta */
	guint16 remote_port;		/* [XXX] port, na którym słucha klient */
	guint8 version;		/* [gg_status60] wersja klienta */
	guint8 image_size;		/* [gg_status60] maksymalny rozmiar grafiki w KiB */
	guint8 dunno1;			/* 0x00 */
	guint32 dunno2;		/* 0x00 */
	char status_data[];
} GG_PACKED gg_status77;

#define GG_NOTIFY_REPLY77 0x0018
typedef struct {
	guint32 uin;			/* [gg_notify_reply60] numerek plus flagi w MSB */
	guint8 status;			/* [gg_notify_reply60] status danej osoby */
	guint32 remote_ip;		/* [XXX] adres ip delikwenta */
	guint16 remote_port;		/* [XXX] port, na którym słucha klient */
	guint8 version;		/* [gg_notify_reply60] wersja klienta */
	guint8 image_size;		/* [gg_notify_reply60] maksymalny rozmiar grafiki w KiB */
	guint8 dunno1;			/* 0x00 */
	guint32 dunno2;		/* 0x00000000 */
	unsigned char next[];		/* [like gg_notify_reply60] nastepny (gg_notify_reply77), lub DLUGOSC_OPISU+OPIS + nastepny (gg_notify_reply77) */
} GG_PACKED gg_notify_reply77;

#define GG_DCC_ACCEPT 0x21

typedef struct {
	guint32 uin;			/* uin */
	unsigned char code1[8];		/* kod polaczenia */
	guint32 seek;			/* od ktorego miejsca chcemy/mamy wysylac. */
	guint32 empty;
} GG_PACKED gg_dcc7_accept;

#define GG_DCC7_REJECT 0x22
typedef struct {
	guint32 uid;
	unsigned char code1[8];
	guint32 reason;		/* known values: 0x02 -> rejected, 0x06 -> invalid version (6.x) 
							 0x01 -> niemozliwe teraz? [jak ktos przesyla inny plik do Ciebie?] */
} GG_PACKED gg_dcc7_reject;

#define GG_DCC7_FILENAME_LEN	255	/**< Maksymalny rozmiar nazwy pliku w połączeniach bezpośrednich */

#define GG_DCC7_NEW 0x20
typedef struct {
	unsigned char code1[8];
	guint32 uin_from;		/* numer nadawcy */
	guint32 uin_to;		/* numer odbiorcy */
	guint32 type;			/* rodzaj transmisji */
	unsigned char filename[GG_DCC7_FILENAME_LEN];
	guint32 size;			/* rozmiar, LE */
	guint32 dunno1;		/* 00 00 00 00 */
	unsigned char hash[20];		/* hash w sha1 */
} GG_PACKED gg_dcc7_new;

#define GG_DCC7_ID_REQUEST 0x23

typedef struct {
	guint32 type;
} GG_PACKED gg_dcc7_id_request;

#define GG_DCC7_ID_REPLY 0x23

typedef struct {
	guint32 type;
	unsigned char code1[8];
} GG_PACKED gg_dcc7_id_reply;

#define GG_DCC7_TYPE_VOICE 0x00000001
#define GG_DCC7_TYPE_FILE 0x00000004

#define GG_SEND_MSG80 0x002d

typedef struct {
	guint32 recipient;
	guint32 seq;
	guint32 msgclass;
	guint32 offset_plain;
	guint32 offset_attr;
	char html_data[];

	/* '\0' */
	/* plaintext msg */
	/* '\0' */
	/* guint32 dunno3; */						/* { 02 06 00 00 } */
	/* guint8 dunno4; */						/* { 00 } */
	/* guint32 dunno5; */		/* like msgclass? */		/* { 08 00 00 00 } */
} GG_PACKED gg_send_msg80;

#define GG_RECV_MSG80 0x002e

typedef struct gg_recv_msg80 {
	guint32 sender;
	guint32 seq;
	guint32 time;
	guint32 msgclass;
	guint32 offset_plain;
	guint32 offset_attr;
	char html_data[];

	/* '\0' */
	/* plaintext msg */
	/* '\0' */
	/* guint32 dunno3; */						/* { 02 06 00 00 } */
	/* guint8 dunno4; */						/* { 00 } */
	/* guint32 dunno5; */		/* like msgclass? */		/* { 08 00 00 00 } */
} GG_PACKED gg_recv_msg80;

#define GG_STATUS_NOT_AVAIL 0x0001		/* niedostępny */
#define GG_STATUS_NOT_AVAIL_DESCR 0x0015	/* niedostępny z opisem (4.8) */
#define GG_STATUS_AVAIL 0x0002			/* dostępny */
#define GG_STATUS_AVAIL_DESCR 0x0004		/* dostępny z opisem (4.9) */
#define GG_STATUS_BUSY 0x0003			/* zajęty */
#define GG_STATUS_BUSY_DESCR 0x0005		/* zajęty z opisem (4.8) */
#define GG_STATUS_INVISIBLE 0x0014		/* niewidoczny (4.6) */
#define GG_STATUS_INVISIBLE_DESCR 0x0016	/* niewidoczny z opisem (4.9) */
#define GG_STATUS_BLOCKED 0x0006		/* zablokowany */

#define GG_STATUS_FRIENDS_MASK 0x8000		/* tylko dla znajomych (4.6) */
#define GG_STATUS_VOICE_MASK 0x20000		/* czy ma wlaczone audio (7.7) */

#define GG_USERLIST_PUT 0x00
#define GG_USERLIST_PUT_MORE 0x01
#define GG_USERLIST_GET 0x02

#define GG_USERLIST_PUT_REPLY 0x00
#define GG_USERLIST_PUT_MORE_REPLY 0x02
#define GG_USERLIST_GET_REPLY 0x06
#define GG_USERLIST_GET_MORE_REPLY 0x04

#define GG_PUBDIR50_WRITE 0x01
#define GG_PUBDIR50_READ 0x02
#define GG_PUBDIR50_SEARCH_REQUEST 0x03
#define GG_PUBDIR50_SEARCH_REPLY 0x05

#define GG_PUBDIR50_UIN "FmNumber"
#define GG_PUBDIR50_STATUS "FmStatus"
#define GG_PUBDIR50_FIRSTNAME "firstname"
#define GG_PUBDIR50_LASTNAME "lastname"
#define GG_PUBDIR50_NICKNAME "nickname"
#define GG_PUBDIR50_BIRTHYEAR "birthyear"
#define GG_PUBDIR50_CITY "city"
#define GG_PUBDIR50_GENDER "gender"
#define GG_PUBDIR50_ACTIVE "ActiveOnly"
#define GG_PUBDIR50_START "fmstart"
#define GG_PUBDIR50_FAMILYNAME "familyname"
#define GG_PUBDIR50_FAMILYCITY "familycity"

#define GG_ACK_BLOCKED 0x0001
#define GG_ACK_DELIVERED 0x0002
#define GG_ACK_QUEUED 0x0003
#define GG_ACK_MBOXFULL 0x0004
#define GG_ACK_NOT_DELIVERED 0x0006
