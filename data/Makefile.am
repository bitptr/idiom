SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files=org.bitptr.idiom.desktop.in
desktop_DATA = org.bitptr.idiom.desktop

edit = sed -e 's|@bindir[@]|$(bindir)|g'

EXTRA_DIST = $(desktop_in_files)

CLEANFILES = $(desktop_DATA)

SUFFIXES = .desktop .desktop.in

.desktop.in.desktop:
	$(edit) $< > $@

UPDATE_DESKTOP = update-desktop-database $(DESTDIR)$(desktopdir) || :

install-data-hook: 
	$(UPDATE_DESKTOP)

uninstall-hook: 
	$(UPDATE_DESKTOP)
	rm -f $(DESTDIR)$(desktopdir)/mimeinfo.cache
