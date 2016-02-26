# Copyright (C) 2010 Ulteo SAS
# http://www.ulteo.com
# Author Samuel BOVEE <samuel@ulteo.com>
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

#TODO: les droits lp dans printer
#TODO: améliorer le packaging global

###############################################
Name: xrdp
###############################################

Summary: RDP server for Linux
Version: 1.0
Release: 1
License: GPL2
Vendor: Ulteo SAS
URL: http://www.ulteo.com
Packager: Samuel Bovée <samuel@ulteo.com>
Group: Applications/System
ExclusiveArch: i386 x86_64
Buildroot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: libtool, gcc, cups-devel, libxml2-devel, libX11-devel, libXfixes-devel, openssl-devel, pam-devel, pulseaudio-lib-devel, libtool-ltdl-devel
Requires: python, tightvnc, glibc, xorg-x11-libX11, xorg-x11-fonts, libcups2, cups-libs, libgnutls26, krb5, pam, libopenssl0_9_8, libxml2, zlib

%description
Xrdp is a RDP server for Linux. It provides remote display of a desktop and
many other features such as:
 * seamless display
 * printer and local device mapping

%changelog
* Mon Mar 26 2010 Samuel Bovée <samuel@ulteo.com> 1.0~svn00210
- Initial release
%prep
cd ..
svn up SOURCES/xrdp
svn export --force SOURCES/xrdp BUILD


%build
./autogen.sh --mandir=/usr/share/man
make
%{__python} setup.py build

%install
rm -fr $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
%{__python} setup.py install --prefix=%{_prefix} --root=%{buildroot} --record-rpm=INSTALLED_FILES
sed -i '/^SESSION/c\SESSION="x-session-manager"' $RPM_BUILD_ROOT/etc/xrdp/startwm.sh
ln -sf /usr/lib/xrdp/librdpsnd.so.0.0.0 $RPM_BUILD_ROOT/usr/lib/pulse-0.9/modules/module-rdp-sink.so

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/sbin/*
/usr/share/xrdp/*
/usr/lib/xrdp/libmc.so*
/usr/lib/xrdp/librdp.so*
/usr/lib/xrdp/libscp.so*
/usr/lib/xrdp/libvnc.so*
/usr/lib/xrdp/libxrdp.so*
/usr/lib/xrdp/libxup.so*
/usr/lib/*.so*
/usr/bin/logoff
/usr/bin/startapp
/usr/bin/xrdp-genkeymap
/usr/bin/xrdp-keygen
/usr/bin/xrdp-sesadmin
/usr/bin/xrdp-sesrun
/usr/bin/xrdp-sestest
%config /etc/xrdp/rdpdr.conf
%config /etc/xrdp/startwm.sh
%config /etc/xrdp/*.ini
%config /etc/xrdp/Xserver/*
%config /etc/pam.d/*
%config /etc/init.d/*
%doc /usr/share/man/man1/logoff.1.gz
%doc /usr/share/man/man1/rdpdr_printer.1.gz
%doc /usr/share/man/man1/startapp.1.gz
%doc /usr/share/man/man1/vchannel_rdpdr.1.gz
%doc /usr/share/man/man1/xrdp-*.1.gz
%doc /usr/share/man/man5/*
%doc /usr/share/man/man8/*
%define _unpackaged_files_terminate_build 0

%post
groupadd tsusers
LOG=/var/log/xrdp
if [ ! -d $LOG ]
then
    mkdir -p $LOG
fi
chgrp tsusers $LOG
SPOOL=/var/spool/xrdp
if [ ! -d $SPOOL ]
then
    mkdir -p $SPOOL
fi
chgrp tsusers $SPOOL

%postun
groupdel tsusers
rm -rf /var/log/xrdp /var/spool/xrdp

###########################################
%package seamrdp
###########################################

Summary: Seamless XRDP Shell
Group: Applications/System
Requires: xrdp, glibc, xorg-x11-libX11

%description seamrdp
Seamlessrdpshell is a rdp addon offering the possibility to have an
application without a desktop

%files seamrdp
%defattr(-,root,root)
/usr/bin/seamlessrdpshell
/usr/bin/XHook
%config /etc/xrdp/seamrdp.conf
%doc /usr/share/man/man1/seamlessrdpshell.1.gz
%doc /usr/share/man/man1/XHook.1.gz

###########################################
%package printer
###########################################

Summary: cups file converter to ps format
Group: Applications/System
Requires: xrdp, python, ghostscript, cups

%description printer
Xrdpi-Printer convert a ps file from cups in ps

%files printer
%defattr(-,root,root)
%config /etc/cups/xrdp_printer.conf
/usr/lib/cups/backend/xrdpprinter
/usr/share/cups/model/PostscriptColor.ppd.gz
%defattr(-,lp,root)
%dir /var/spool/xrdp_printer/SPOOL

###########################################
%package sound
###########################################

Summary: XRDP plugin for PulseAudio
Group: Applications/System
Requires: xrdp, pulseaudio, alsa-utils, alsa-lib

%description sound
This package contains the XRDP plugin for PulseAudio, a sound server for POSIX
and WIN32 systems

%files sound
%defattr(-,root,root)
%config /etc/asound.conf
%config /etc/xrdp/rdpsnd.*
/usr/lib/xrdp/librdpsnd.so*
/usr/lib/pulse-0.9/modules/module-rdp-sink.so

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
