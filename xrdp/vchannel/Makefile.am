
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libxrdp_vchannel.la

libxrdp_vchannel_la_SOURCES = \
  vchannel.h vchannel.c 

libxrdp_vchannel_la_LIBADD = \
  -lpthread \
  -ldl \
  $(top_builddir)/common/libxrdp_common.la 
