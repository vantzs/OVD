xserverdir=$(sysconfdir)/xrdp/Xserver

xserver_DATA = \
  rgb.txt \
  SecurityPolicy

xserverbindir=$(bindir)/

xserverbin_DATA = \
  logoff \
  seamlessrdpshell \
  startapp


install-data-hook:
	chmod 755 $(DESTDIR)$(bindir)/seamlessrdpshell
	chmod 755 $(DESTDIR)$(bindir)/logoff
	chmod 755 $(DESTDIR)$(bindir)/startapp

EXTRA_DIST = \
  $(xserverbin_DATA) \
  $(xserver_DATA)
