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

update-desktop-database:
	@-if test -z "$(DESTDIR)"; then \
	        echo "Updating .desktop database."; \
	        update-desktop-database; \
	else \
	        echo "*** .desktop database not updated.  After (un)install, run:"; \
	        echo "***   update-desktop-database"; \
	fi

install-data-hook: update-desktop-database

uninstall-hook: update-desktop-database
