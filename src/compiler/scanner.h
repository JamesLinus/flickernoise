/*
 * Milkymist SoC (Software)
 * Copyright (C) 2007, 2008, 2009 Sebastien Bourdeauducq
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SCANNER_H
#define __SCANNER_H

#define TOK_ERROR	(-1)
#define TOK_EOF		0

#include "parser.h"

enum scanner_cond {
	yycN = 1,
	yycFNAME1,
	yycFNAME2,
	yycFNS1,
	yycFNS2,
};

enum fns_state {
	fns_idle,	/* not near a file name list */
	fns_latent,	/* after a file name, will drop unless seeing comma */
	fns_comma,	/* test for comma */
};

struct scanner {
	enum scanner_cond cond;
	unsigned char *marker;
	unsigned char *old_cursor;
	unsigned char *cursor;
	unsigned char *limit;
	enum fns_state fns_state;
	int lineno;
};

struct scanner *new_scanner(unsigned char *input);
void delete_scanner(struct scanner *s);

/* get to the next token and return its type */
int scan(struct scanner *s);

/* get the symbol for the unique string comprising the current token
 */
struct sym *get_symbol(struct scanner *s);

struct sym *get_tag(struct scanner *s);

/* malloc'ed non-unique string */
const char *get_name(struct scanner *s);

/* malloc'ed quoted string */
const char *get_string(struct scanner *s);

float get_constant(struct scanner *s);

#endif /* __SCANNER_H */

