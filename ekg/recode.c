/*
 *  (C) Copyright 2009-2010 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *			Wies�aw Ochmi�ski <wiechu@wiechu.com>
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


/* NOTES/THINK/BUGS:
 * 	- do we need any #define? 
 * 	- if we stop using ekg_convert_string_init() in plugins this file could be smaller.
 * 	- don't use gg_*() funcs, always use iconv? lite iconv in compat/ ?
 * 	- create:
 * 		static struct ekg_converter same_enc;
 * 		
 * 		we should know if iconv_open() failed, or we have good console_charset..
 * 		give info to user, if this first happen.
 *
 * 	- we should also reinit encodings, if user changed console_charset.
 * 	- implement ekg_any_to_core(), ekg_locale_to_any()
 *
 * 	- Check if this code works OK.
 */

#include "ekg2.h"

#include <errno.h>
#include <string.h>

struct ekg_encoding_pair {
	gchar *from;
	gchar *to;
};

/**
 * ekg_convert_string_init()
 *
 * Initialize string conversion thing for two given charsets.
 *
 * @param from		- input encoding (will be duped; if NULL, console_charset will be assumed).
 * @param to		- output encoding (will be duped; if NULL, console_charset will be assumed).
 * @param rev		- pointer to assign reverse conversion into; if NULL, no reverse converter will be initialized.
 * 
 * @return	Pointer that should be passed to other ekg_convert_string_*(), even if it's NULL.
 *
 * @sa ekg_convert_string_destroy()	- deinits charset conversion.
 * @sa ekg_convert_string_p()		- main charset conversion function.
 */
void *ekg_convert_string_init(const char *from, const char *to, void **rev) {
	struct ekg_encoding_pair *enc;

	if (rev) {
		enc = g_new(struct ekg_encoding_pair, 1);
		enc->from = g_strdup(to);
		enc->to = g_strdup(from);
		*rev = enc;
	}

	enc = g_new(struct ekg_encoding_pair, 1);
	enc->from = g_strdup(from);
	enc->to = g_strdup(to);
	return enc;
}

/**
 * ekg_convert_string_destroy()
 *
 * Frees internal data associated with given pointer, and uninitalizes iconv, if it's not needed anymore.
 *
 * @note If 'rev' param was used with ekg_convert_string_init(), this functions must be called two times
 *	- with returned value, and with rev-associated one.
 *
 * @param ptr		- pointer returned by ekg_convert_string_init().
 *
 * @sa ekg_convert_string_init()	- init charset conversion.
 * @sa ekg_convert_string_p()		- main charset conversion function.
 */

void ekg_convert_string_destroy(void *ptr) {
	struct ekg_encoding_pair *e = ptr;
	g_free(e->from);
	g_free(e->to);
	g_free(ptr);
}

/**
 * ekg_convert_string_p()
 *
 * Converts string to specified encoding, using pointer returned by ekg_convert_string_init().
 * Invalid characters in input will be replaced with question marks.
 *
 * @param ps		- string to be converted (won't be freed).
 * @param ptr		- pointer returned by ekg_convert_string_init().
 *
 * @return	Pointer to allocated result or NULL, if some failure has occured or no conversion
 *			is needed (i.e. resulting string would be same as input).
 *
 * @sa ekg_convert_string_init()	- init charset conversion.
 */

char *ekg_convert_string_p(const char *ps, void *ptr) {
	struct ekg_encoding_pair *e = ptr;
	return ekg_convert_string(ps, e->from, e->to);
}

/**
 * ekg_convert_string()
 *
 * Converts string to specified encoding, replacing invalid chars with question marks.
 *
 * @note Deprecated, in favour of ekg_convert_string_p(). Should be used only on single
 *	conversions, where charset pair won't be used again.
 *
 * @param ps		- string to be converted (it won't be freed).
 * @param from		- input encoding (if NULL, console_charset will be assumed).
 * @param to		- output encoding (if NULL, console_charset will be assumed).
 *
 * @return	Pointer to allocated result on success, NULL on failure
 *			or when both encodings are equal.
 *
 * @sa ekg_convert_string_p()	- more optimized version.
 */
char *ekg_convert_string(const char *ps, const char *from, const char *to) {
	char *res;
	gsize written;

	if (!ps) /* compat, please do not rely on it */
		return NULL;
	if (!from)
		from = "utf8";
	if (!to)
		to = "utf8";

	res = g_convert_with_fallback(ps, -1, to, from, NULL, NULL, &written, NULL);

	if (!res) {
		res = g_strdup(ps);
		ekg_fix_utf8(res);
	}

	return res;
}

string_t ekg_convert_string_t_p(string_t s, void *ptr) {
	struct ekg_encoding_pair *e = ptr;
	return ekg_convert_string_t(s, e->from, e->to);
}

string_t ekg_convert_string_t(string_t s, const char *from, const char *to) {
	char *res;
	string_t ret;
	gsize written;

	if (!from)
		from = "utf8";
	if (!to)
		to = "utf8";

	res = g_convert_with_fallback(s->str, s->len, to, from, NULL, NULL, &written, NULL);
	ret = string_init(NULL);

	if (!res)
		string_append_raw(ret, g_memdup(s->str, s->len), s->len);
	else
		string_append_raw(ret, res, written);

	return ret;
}

void ekg_recode_inc_ref(const gchar *enc) {
}

void ekg_recode_dec_ref(const gchar *enc) {
}

char *ekg_recode_from_core(const gchar *enc, gchar *buf) {
	char *res = ekg_recode_to(enc, buf);
	g_free(buf);
	return res;
}

gchar *ekg_recode_to_core(const gchar *enc, char *buf) {
	gchar *res = ekg_recode_from(enc, buf);
	g_free(buf);
	return res;
}

char *ekg_recode_from_core_dup(const gchar *enc, const gchar *buf) {
	return ekg_recode_to(enc, buf);
}

gchar *ekg_recode_to_core_dup(const gchar *enc, const char *buf) {
	return ekg_recode_from(enc, buf);
}

const char *ekg_recode_from_core_use(const gchar *enc, const gchar *buf) {
	return ekg_recode_to(enc, buf);
}

const gchar *ekg_recode_to_core_use(const gchar *enc, const char *buf) {
	return ekg_recode_from(enc, buf);
}

/**
 * ekg_recode_from()
 *
 * Convert complete string str from given encoding to ekg2 internal
 * encoding (utf8). If the conversion fails, fallback to duplicating
 * and utf8-cleaning str.
 *
 * @param enc - source encoding (e.g. "iso-8859-2").
 * @param str - string to recode [may be NULL].
 *
 * @return A newly-allocated string which is guaranteed to be correct
 * utf8 and needs to be freed using g_free(), or NULL if !str.
 */
gchar *ekg_recode_from(const gchar *enc, const char *str) {
	/* -- temporary, please do not rely on !enc */
	if (G_UNLIKELY(!enc))
		return ekg_recode_from_locale(str);

	return ekg_convert_string(str, enc, NULL);
}

/**
 * ekg_recode_to()
 *
 * Convert complete string str from ekg2 internal encoding (utf8)
 * to given encoding. If the conversion fails, fallback to duplicating
 * the string.
 *
 * @param enc - target encoding (e.g. "iso-8859-2").
 * @param str - string to recode [may be NULL].
 *
 * @return A newly-allocated string which must be freed using g_free(),
 * or NULL if !str.
 */
char *ekg_recode_to(const gchar *enc, const gchar *str) {
	/* -- temporary, please do not rely on !enc */
	if (G_UNLIKELY(!enc))
		return ekg_recode_to_locale(str);

	return ekg_convert_string(str, NULL, enc);
}

/**
 * ekg_recode_from_locale()
 *
 * Convert complete string str from locale to ekg2 internal encoding
 * (utf8). If the conversion fails, fallback to duplicating
 * and utf8-cleaning the string.
 *
 * @param str - string to recode. May be NULL.
 *
 * @return A newly-allocated string which must be freed using g_free(),
 * or NULL if !str.
 */
gchar *ekg_recode_from_locale(const char *str) {
	if (console_charset_is_utf8) {
		gchar *tmp = g_strdup(str);
		if (tmp)
			ekg_fix_utf8(tmp);
		return tmp;
	} else
		return ekg_recode_from(console_charset, str);
}

/**
 * ekg_recode_to_locale()
 *
 * Convert complete string str from ekg2 internal encoding (utf8)
 * to locale. If the conversion fails, fallback to duplicating
 * the string.
 *
 * @param str - string to recode. May be NULL.
 *
 * @return A newly-allocated string which must be freed using g_free(),
 * or NULL if !str.
 */
char *ekg_recode_to_locale(const gchar *str) {
	if (console_charset_is_utf8)
		return g_strdup(str);
	else
		return ekg_recode_to(console_charset, str);
}

static gboolean gstring_recode_helper(GString *s, const gchar *from, const gchar *to, gboolean fixutf) {
	char *res;
	gsize written;

	res = g_convert_with_fallback(s->str, s->len, to, from, NULL, NULL, &written, NULL);

	if (G_LIKELY(res)) {
		g_string_truncate(s, 0);
		g_string_append_len(s, res, written);
	} else if (G_LIKELY(fixutf))
		ekg_fix_utf8(s->str);

	return !!res;
}

/**
 * ekg_recode_gstring_from()
 *
 * Convert complete GString in-place from given encoding to ekg2
 * internal encoding (utf8). If the conversion fails, fallback to
 * utf8-cleaning the string.
 *
 * @param enc - source encoding.
 * @param s - GString to recode and to write the result into. After
 *	the call to this function, it is guaranteed to contain correct utf8.
 *
 * @return TRUE if conversion succeeded, FALSE otherwise.
 */
gboolean ekg_recode_gstring_from(const gchar *enc, GString *s) {
	return gstring_recode_helper(s, enc, "utf8", TRUE);
}

/**
 * ekg_try_recode_gstring_from()
 *
 * Convert complete GString in-place from given encoding to ekg2
 * internal encoding (utf8). If the conversion fails, leave string
 * unchanged.
 *
 * @param enc - source encoding.
 * @param s - GString to recode and to write the result into
 *	if the conversion succeeds.
 *
 * @return TRUE if conversion succeeded, FALSE otherwise.
 */
gboolean ekg_try_recode_gstring_from(const gchar *enc, GString *s) {
	return gstring_recode_helper(s, enc, "utf8", FALSE);
}

/**
 * ekg_recode_gstring_to()
 *
 * Convert complete GString in-place from ekg2 internal encoding (utf8)
 * to given encoding. If the conversion fails, leave string unchanged.
 *
 * @param enc - target encoding.
 * @param s - GString to recode and to write the result into
 *	if the conversion succeeds.
 *
 * @return TRUE if conversion succeeded, FALSE otherwise.
 */
gboolean ekg_recode_gstring_to(const gchar *enc, GString *s) {
	return gstring_recode_helper(s, "utf8", enc, FALSE);
}

/**
 * ekg_fix_utf8()
 *
 * Ensure correct utf8 in buffer, replacing incorrect sequences.
 *
 * @param buf - writable, null-terminated, utf8 string.
 *
 * @note Currently, this function replaces incorrect bytes with ASCII
 * SUB (0x1a). This may change in future.
 */
void ekg_fix_utf8(gchar *buf) {
	const gchar *p = buf;

	while (G_UNLIKELY(!g_utf8_validate(p, -1, &p)))
		*((gchar*) p++) = 0x1a; /* substitute, UTR#36 suggests it as byte replacement */
}

static void fstr_mark_linebreaks(gchar *s, fstr_attr_t *a) {
	gchar *p;

	/* XXX: use pango */

	g_assert(g_utf8_validate(s, -1, NULL));
	for (p = s; *p; p = g_utf8_next_char(p)) {
		/* if we're already mangling fstring_t, suit SUBs as well */
		if (G_UNLIKELY(*p == 0x1a)) {
			*p = '?';
			a[p - s] |= FSTR_REVERSE;
		}

		switch (g_unichar_break_type(g_utf8_get_char(p))) {
				/* these should cause line break themselves
				 * but we don't support unicode that well,
				 * so just use them as wrap opportunity */
			case G_UNICODE_BREAK_MANDATORY:
			case G_UNICODE_BREAK_CARRIAGE_RETURN:
			case G_UNICODE_BREAK_LINE_FEED:
			case G_UNICODE_BREAK_NEXT_LINE:
				/* typical break opportunities */
			case G_UNICODE_BREAK_ZERO_WIDTH_SPACE:
			case G_UNICODE_BREAK_SPACE:
			case G_UNICODE_BREAK_BEFORE_AND_AFTER:
			case G_UNICODE_BREAK_AFTER:
				/* not always but use it anyway */
			case G_UNICODE_BREAK_HYPHEN:
				{
					const gsize startpos = p - s;
					const gsize endpos = g_utf8_next_char(p) - s;
					gsize i;

					for (i = startpos; i < endpos; i++)
						a[i] |= FSTR_LINEBREAK;
				}
				break;

				/* we do not support breaking before */
			case G_UNICODE_BREAK_BEFORE:
			default:
				break;
		}
	}
}

/**
 * ekg_recode_fstr_to_locale()
 *
 * Recode fstring_t from ekg2 internal encoding (utf8) to locale,
 * adjusting attributes as necessary. Set attributes based on special
 * unicode character properties (e.g. FSTR_LINEBREAK).
 *
 * @param fstr - input fstring_t.
 *
 * @return Newly-allocated fstring_t, which needs to be freed using
 *	fstring_free().
 */
fstring_t *ekg_recode_fstr_to_locale(const fstring_t *fstr) {
	if (console_charset_is_utf8) {
		fstring_t *s = fstring_dup(fstr);
		fstr_mark_linebreaks(s->str, s->attr);
		return s;
	} else {
		gchar *s;
		fstr_attr_t *a, *dupattr;
		gssize len;
		const gssize inpsize = strlen(fstr->str);
		GString *outs = g_string_sized_new(inpsize);
		GByteArray *outa = g_byte_array_sized_new(inpsize * sizeof(fstr_attr_t));
		fstring_t *out = g_memdup(fstr, sizeof(fstring_t)); /* XXX: move to slice alloc */

		fstring_iter(fstr, &s, &a, &len);
			/* we need to have a modifiable copy to set linebreaks */
		a = dupattr = g_memdup(a, inpsize * sizeof(fstr_attr_t));
		fstr_mark_linebreaks(s, a);
		while (fstring_next(&s, &a, &len, NULL)) {
			char *ls;
			gsize ob;

			ls = g_convert_with_fallback(s, len, console_charset, "utf8",
					NULL, NULL, &ob, NULL);

			if (ls) {
				g_string_append_len(outs, ls, ob);
				g_free(ls);
			} else {
				/* XXX: is that really a good idea? */
				g_string_append_len(outs, s, len);
				ob = len;
			}

			/* we can assume 'a' has len identical 'fstr_attr_t's */
			while (ob > len) {
				g_byte_array_append(outa, (gpointer) a, len * sizeof(fstr_attr_t));
				ob -= len;
			}
			if (ob > 0)
				g_byte_array_append(outa, (gpointer) a, ob * sizeof(fstr_attr_t));
		}

		g_free(dupattr);
		out->str = g_string_free(outs, FALSE);
		out->attr = (fstr_attr_t*) g_byte_array_free(outa, FALSE);
		/* XXX: margins and stuff get outdated */
		return out;
	}

	g_assert_not_reached();
}
