SUBDIRS = 16x16 22x22 24x24 256x256 32x32 48x48 8x8 scalable

UPDATE_ICON_CACHE = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor || :

install-data-hook: 
	$(UPDATE_ICON_CACHE)
uninstall-hook: 
	$(UPDATE_ICON_CACHE)
