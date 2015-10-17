AM_CPPFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\" $(GTK_CFLAGS) $(CURL_CFLAGS) $(JSON_GLIB_CFLAGS) -D_GNU_SOURCE
AM_LDFLAGS = $(GTK_LIBS) $(CURL_LIBS) $(JSON_GLIB_LIBS)
AM_CFLAGS = -std=c99 -Wall -Wextra -pedantic-errors -Wno-unused-parameter -Werror
bin_PROGRAMS = idiom
dist_idiom_SOURCES = main.c pathnames.h compat.c compat.h
