/*
 * wmmenugen - Window Maker PropList menu generator
 *
 * Copyright (c) 2010. Tamas Tevesz <ice@extreme.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <WINGs/WUtil.h>

/* flags attached to a particular WMMenuEntry */
#define F_TERMINAL		(1 << 0)
#define F_RESTART		(1 << 1)


/* a representation of a Window Maker menu entry. all menus are
 * transformed into this form.
 */
typedef struct {
	char		*Name;		/* display name; submenu path of submenu */
	char		*CmdLine;	/* command to execute, NULL if submenu */
	char		*SubMenu;	/* submenu to place entry in; only used when an entry is */
					/* added to the tree by the parser; new entries created in */
					/* main (submenu creation) should set this to NULL */
	int		 Flags;		/* flags */
} WMMenuEntry;

/* the abstract menu tree
 */
WMTreeNode *menu;

char *env_lang, *env_ctry, *env_enc, *env_mod;

/* wmmenu_misc.c
 */
void  parse_locale(const char *what, char **env_lang, char **env_ctry, char **env_enc, char **env_mod);
char *find_terminal_emulator(void);

/* implemented parsers
 */
void parse_xdg(const char *file, void (*addWMMenuEntryCallback)(WMMenuEntry *aEntry));
void parse_wmconfig(const char *file, void (*addWMMenuEntryCallback)(WMMenuEntry *aEntry));