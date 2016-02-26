# Copyright (C) 2010-2011 Ulteo SAS
# http://www.ulteo.com
# Author Samuel BOVEE <samuel@ulteo.com> 2010-2011
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

Name: xrdp
Version: @VERSION@
Release: @RELEASE@

Summary: RDP server for Linux
License: GPL2
Group: Applications/System
Vendor: Ulteo SAS
URL: http://www.ulteo.com
Packager: Samuel Bovée <samuel@ulteo.com>
Distribution: OpenSUSE 11.3

Source: %{name}-%{version}.tar.gz
ExclusiveArch: i586 x86_64
BuildRequires: libtool, gcc, libxml2-devel, xorg-x11-libX11-devel, xorg-x11-libXfixes-devel, openssl-devel, pam-devel, pulseaudio-devel, cups-devel, fuse-devel
Requires: python, tightvnc, cups-libs, libcom_err2, libgcrypt11, libgnutls26, krb5, pam, libopenssl1_0_0, xorg-x11-libX11, libxml2, zlib

%description
Xrdp is a RDP server for Linux. It provides remote display of a desktop and
many other features such as:
 * seamless display
 * printer and local device mapping

%changelog
* Wed Jan 26 2011 Samuel Bovée <samuel@ulteo.com> 99.99.svn00681
- Initial release

%prep
%setup -q

%build
ARCH=$(getconf LONG_BIT)
if [ "$ARCH" = "32" ]; then
    LIBDIR=/usr/lib
elif [ "$ARCH" = "64" ]; then
    LIBDIR=/usr/lib64
fi
./configure --mandir=/usr/share/man --libdir=$LIBDIR
make -j 4
%{__python} setup.py build

%install
rm -fr $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
%{__python} setup.py install --prefix=%{_prefix} --root=%{buildroot} --record-rpm=INSTALLED_FILES
mkdir -p $RPM_BUILD_ROOT/var/log/xrdp $RPM_BUILD_ROOT/var/spool/xrdp
sed -i '/^SESSIONS=/c\SESSION="/usr/bin/startkde"' $RPM_BUILD_ROOT/etc/xrdp/startwm.sh
install -D instfiles/init/suse/xrdp $RPM_BUILD_ROOT/etc/init.d/xrdp

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%config /etc/xrdp/xrdp-log.conf
%config /etc/xrdp/startwm.sh
%config /etc/xrdp/*.ini
%config /etc/xrdp/Xserver/*
%config /etc/pam.d/*
%config /etc/init.d/*
/usr/lib*/xrdp/libmc.so.*
/usr/lib*/xrdp/librdp.so.*
/usr/lib*/xrdp/libscp.so.*
/usr/lib*/xrdp/libvnc.so.*
/usr/lib*/xrdp/libxrdp.so.*
/usr/lib*/xrdp/libxup.so.*
/usr/lib*/*.so.*
/usr/sbin/xrdp*
/usr/share/xrdp/*
/usr/bin/logoff
/usr/bin/xrdp-*
%doc /usr/share/man/man1/logoff.1.gz
%doc /usr/share/man/man1/xrdp-*.1.gz
%doc /usr/share/man/man5/*
%doc /usr/share/man/man8/*
%dir /var/log/xrdp
%dir /var/spool/xrdp

%post
getent group tsusers >/dev/null || groupadd tsusers
chgrp tsusers /var/spool/xrdp

ldconfig
chkconfig --add xrdp > /dev/null
service xrdp start

%preun
service xrdp stop
chkconfig --del xrdp > /dev/null

%postun
rm -rf /var/log/xrdp /var/spool/xrdp
getent group tsusers >/dev/null && groupdel tsusers

ldconfig

###########################################
%package seamrdp
###########################################

Summary: Seamless XRDP Shell
Group: Applications/System
Requires: xrdp, xorg-x11-libX11

%description seamrdp
Seamlessrdpshell is a rdp addon offering the possibility to have an
application without a desktop

%files seamrdp
%defattr(-,root,root)
%config /etc/xrdp/seamrdp.conf
/usr/bin/seamlessrdpshell
/usr/bin/startapp
/usr/bin/XHook
%doc /usr/share/man/man1/seamlessrdpshell.1.gz
%doc /usr/share/man/man1/startapp.1.gz
%doc /usr/share/man/man1/XHook.1.gz

###########################################
%package rdpdr
###########################################

Summary: XRDP disks redirection
Group: Applications/System
Requires: xrdp, fuse, libxml2

%description rdpdr
XRDP channel that handle disks redirection.

%files rdpdr
%defattr(-,root,root)
%config /etc/xrdp/rdpdr.conf
/usr/sbin/vchannel_rdpdr
%doc /usr/share/man/man1/rdpdr_disk.1.gz
%doc /usr/share/man/man1/vchannel_rdpdr.1.gz

%post rdpdr
sed -ri "s/^# *(user_allow_other)/\1/" /etc/fuse.conf

###########################################
%package clipboard
###########################################

Summary: XRDP clipboard
Group: Applications/System
Requires: xrdp, xorg-x11-libX11

%description clipboard
XRDP channel providing copy/past text functionnality.

%files clipboard
%defattr(-,root,root)
%config /etc/xrdp/cliprdr.conf
/usr/sbin/vchannel_cliprdr

###########################################
%package sound
###########################################

Summary: XRDP plugin for PulseAudio
Group: Applications/System
Requires: xrdp, pulseaudio, alsa-utils, libasound2

%description sound
This package contains the XRDP plugin for PulseAudio, a sound server for POSIX
and WIN32 systems

%files sound
%defattr(-,root,root)
%config /etc/asound.conf
%config /etc/xrdp/rdpsnd.*
/usr/sbin/vchannel_rdpsnd

###########################################
%package printer
###########################################

Summary: cups file converter to ps format
Group: Applications/System
Requires: xrdp-rdpdr, python, ghostscript, cups

%description printer
Xrdpi-Printer convert a ps file from cups in ps

%files printer
%defattr(-,root,root)
%config /etc/cups/xrdp_printer.conf
/usr/lib*/cups/backend/xrdpprinter
/usr/share/cups/model/PostscriptColor.ppd.gz
%doc /usr/share/man/man1/rdpdr_printer.1.gz
%defattr(-,lp,root)
%dir /var/spool/xrdp_printer/SPOOL

###########################################
%package python
###########################################

Summary: Python API for XRDP
Group: Applications/System
Requires: xrdp, python

%description python
XRDP-Python is a Python wrapper for XRDP

%files -f INSTALLED_FILES python
%defattr(-,root,root)

###########################################
%package devel
###########################################

Summary: Developpement files for XRDP
Group: Development
Requires: xrdp

# TODO: headers missing

%description devel
Developpement files for XRDP

%files devel
%defattr(-,root,root)
/usr/lib*/*.a
/usr/lib*/*.la
/usr/lib*/*.so
/usr/lib*/xrdp/*.a
/usr/lib*/xrdp/*.la
/usr/lib*/xrdp/*.so
