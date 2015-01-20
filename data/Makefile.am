SUBDIRS = icons

desktopdir = $(datadir)/applications
dist_desktop_DATA = org.bitptr.idiom.desktop

edit = sed -e 's|@bindir[@]|$(bindir)|g'

org.bitptr.idiom.desktop: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@

org.bitptr.idiom.desktop: $(srcdir)/org.bitptr.idiom.desktop.in

UPDATE_DESKTOP = update-desktop-database $(datadir)/applications || :

install-data-hook: 
	$(UPDATE_DESKTOP)

uninstall-hook: 
	$(UPDATE_DESKTOP)
