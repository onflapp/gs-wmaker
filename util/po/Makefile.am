CATALOGS = @UTILMOFILES@

nlsdir = $(NLSDIR)

CLEANFILES = wmgenmenu.pot $(CATALOGS)

EXTRA_DIST = de.po

POTFILES  = \
	$(top_builddir)/util/wmgenmenu.c

SUFFIXES = .po .mo

.po.mo:
	msgfmt -c -o $@ $<


all-local: $(CATALOGS)

wmgenmenu.pot: $(POTFILES)
	xgettext --default-domain=wmgenmenu \
	--add-comments --keyword=_ --keyword=N_ $(POTFILES)
	if cmp -s wmgenmenu.po wmgenmenu.pot; then \
	    rm -f wmgenmenu.po; \
	else \
	    mv -f wmgenmenu.po wmgenmenu.pot; \
	fi


install-data-local:  $(CATALOGS)
	$(mkinstalldirs) $(DESTDIR)$(nlsdir)
	chmod 755 $(DESTDIR)$(nlsdir)
	for n in $(CATALOGS) __DuMmY ; do \
		if test "$$n" -a "$$n" != "__DuMmY" ; then \
			l=`basename $$n .mo`; \
			$(mkinstalldirs) $(DESTDIR)$(nlsdir)/$$l/LC_MESSAGES; \
			chmod 755 $(DESTDIR)$(nlsdir)/$$l; \
			chmod 755 $(DESTDIR)$(nlsdir)/$$l/LC_MESSAGES; \
			$(INSTALL_DATA) -m 644 $$n $(DESTDIR)$(nlsdir)/$$l/LC_MESSAGES/wmgenmenu.mo; \
		fi; \
	done