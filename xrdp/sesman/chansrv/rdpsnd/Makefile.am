# Copyright (C) 2010 Ulteo SAS
# http://www.ulteo.com
# Author Samuel BOVEE <samuel@ulteo.com> 2010
# Author David LECHEVALIER <david@ulteo.com> 2010
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

AM_CFLAGS = \
  -DHAVE_CONFIG_H

INCLUDES = \
  -I$(top_srcdir)/sesman/chansrv/rdpsnd/pulse_headers \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/vchannel

pkglib_LTLIBRARIES = \
  librdpsnd.la

HEADERS_PULSECORE = \
  pulse_headers/pulsecore/asyncmsgq.h pulse_headers/pulsecore/gccmacro.h \
  pulse_headers/pulsecore/memblockq.h pulse_headers/pulsecore/object.h \
  pulse_headers/pulsecore/sink-input.h pulse_headers/pulsecore/memblock.h \
  pulse_headers/pulsecore/asyncq.h pulse_headers/pulsecore/hashmap.h \
  pulse_headers/pulsecore/memchunk.h pulse_headers/pulsecore/once.h \
  pulse_headers/pulsecore/source.h pulse_headers/pulsecore/native-common.h \
  pulse_headers/pulsecore/atomic.h pulse_headers/pulsecore/hook-list.h \
  pulse_headers/pulsecore/modargs.h pulse_headers/pulsecore/queue.h \
  pulse_headers/pulsecore/thread.h pulse_headers/pulsecore/sink.h \
  pulse_headers/pulsecore/client.h pulse_headers/pulsecore/idxset.h \
  pulse_headers/pulsecore/modinfo.h pulse_headers/pulsecore/refcnt.h \
  pulse_headers/pulsecore/thread-mq.h pulse_headers/pulsecore/core-def.h \
  pulse_headers/pulsecore/llist.h pulse_headers/pulsecore/module.h \
  pulse_headers/pulsecore/resampler.h pulse_headers/pulsecore/core.h \
  pulse_headers/pulsecore/log.h pulse_headers/pulsecore/msgobject.h \
  pulse_headers/pulsecore/rtclock.h pulse_headers/pulsecore/core-subscribe.h \
  pulse_headers/pulsecore/macro.h pulse_headers/pulsecore/mutex.h \
  pulse_headers/pulsecore/rtpoll.h pulse_headers/pulsecore/fdsem.h

librdpsnd_la_SOURCES = \
  module-rdp-sink-symdef.h module-rdp-sink.c \
  sound_channel.h sound_channel.c \
  config.h $(HEADERS_PULSECORE)

#librdpsnd_la_LIBADD = \
#  $(top_builddir)/vchannel/libxrdp_vchannel.la \
#  $(top_builddir)/common/libxrdp_common.la 

alsaconfdir=$(sysconfdir)

alsaconf_DATA = \
  asound.conf

sndsysconfdir=$(sysconfdir)/xrdp

sndsysconf_DATA = \
  rdpsnd.conf rdpsnd.pa

# must be tab below
install-data-hook:
	pulsemoddir=$(DESTDIR)$(libdir)/pulse-0.9/modules; \
	mkdir -p $$pulsemoddir; \
	ln -sf $(DESTDIR)$(libdir)/xrdp/librdpsnd.so.0.0.0  $$pulsemoddir/module-rdp-sink.so
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/rdpsnd.pa

EXTRA_DIST = \
  $(sndsysconf_DATA) \
  $(alsaconf_DATA)
