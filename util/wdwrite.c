/* wdwrite.c - write key/value to defaults database
 *
 *  WindowMaker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef __GLIBC__
#define _GNU_SOURCE		/* getopt_long */
#endif

/*
 * WindowMaker defaults DB writer
 */
#include "config.h"

#include <X11/Xproto.h>

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_STDNORETURN
#include <stdnoreturn.h>
#endif

#include <WINGs/WUtil.h>

#include "../src/wconfig.h"

static const char *prog_name;

static void send_reconfigure()
{
  Display *dpy;
	XEvent ev;
	memset(&ev, 0, sizeof(XEvent));

  dpy = XOpenDisplay(NULL);

	ev.xclient.type = ClientMessage;
	ev.xclient.message_type = XInternAtom(dpy, "_WINDOWMAKER_COMMAND", False);
	ev.xclient.window = DefaultRootWindow(dpy);
	ev.xclient.format = 8;
	strncpy(ev.xclient.data.b, "Reconfigure", sizeof(ev.xclient.data.b));

	XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask, &ev);
	XFlush(dpy);
}

static noreturn void print_help(int print_usage, int exitval)
{
	printf("Usage: %s [OPTIONS] <domain> <key> <value> [-r]\n", prog_name);
	if (print_usage) {
		puts("Write <value> for <key> in <domain>'s database");
		puts("");
		puts("  -h, --help        display this help message");
		puts("  -v, --version     output version information and exit");
		puts("  -r, --reconfigure notify WindowMaker to re-read the config");
	}
	exit(exitval);
}

int main(int argc, char **argv)
{
	char path[PATH_MAX];
	WMPropList *key, *value, *dict;
	int ch;
  int rc;

	struct option longopts[] = {
		{ "version",	no_argument,		NULL,			'v' },
		{ "help",	no_argument,		NULL,			'h' },
		{ "reconfigure",	no_argument,		NULL,			'r' },
		{ NULL,		0,			NULL,			0 }
	};

  rc = 0;
	prog_name = argv[0];
	while ((ch = getopt_long(argc, argv, "hvr", longopts, NULL)) != -1)
		switch(ch) {
			case 'v':
				printf("%s (Window Maker %s)\n", prog_name, VERSION);
				return 0;
				/* NOTREACHED */
			case 'h':
				print_help(1, 0);
				/* NOTREACHED */
			case 'r':
        rc = 1;
        break;
			case 0:
				break;
			default:
				print_help(0, 1);
				/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (argc < 3)
		print_help(0, 1);

	key = WMCreatePLString(argv[1]);
	value = WMCreatePropListFromDescription(argv[2]);
	if (!value) {
		printf("%s: syntax error in value \"%s\"", prog_name, argv[2]);
		return 1;
	}

	snprintf(path, sizeof(path), "%s", wdefaultspathfordomain(argv[0]));

	dict = WMReadPropListFromFile(path);
	if (!dict) {
		dict = WMCreatePLDictionary(key, value, NULL);
	} else {
		WMPutInPLDictionary(dict, key, value);
	}

	WMWritePropListToFile(dict, path);

  if (rc)
   send_reconfigure();

	return 0;
}
