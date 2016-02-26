This was the final Community Release of Ulteo OVD.

The install/compile instructions below we're used on a Ubuntu 12.04 x86 LTS machine.

1. Install a SubSystem
	Install a requirement package
		apt-get install curl

	cd Subsystem
	sudo ./autogen
		(set a environment variable - CHROOTDIR -)
		(check /etc/ulteo/subsystem.conf file)
		{ ---> if you see some error, check below files
			sudo cp ./Subsystem/script/uchroot /usr/sbin
			sudo chown root. /usr/sbin/uchroot

			chmod +x ./Subsystem/init/debian/ulteo-ovd-subsystem
			sudo cp ./Subsystem/init/debian/ulteo-ovd-subsystem /etc/init.d
			sudo chown root. /etc/init.d/ulteo-ovd-subsystem
		}


	cd packaging/debian/ovd-subsystem/lucid
	chmod +x ulteo-ovd-session-manager.*
	vi ulteo-ovd-subsystem.templates
		delete '_' (_Description -> Description)
	sudo ./ulteo-ovd-subsystem.postinst
		session manager IP : 127.0.0.1

2. mount a home directory
	. /etc/default/ulteo-ovd-subsystem
	mkdir $CHROOTDIR/home/$(whoami)
	sudo mount --bind ~ $CHROOTDIR/home/$(whoami)
	sudo uchroot
	cd

3. Install a xrdp
	Install requirement packages
		apt-get install libssl-dev libpam0g-dev libx11-dev libxfixes-dev libjpeg-dev libcups2-dev libxml2-dev libpulse-dev libfuse-dev libmagickwand-dev autoconf automake subversion libtool

	cd xrdp
	./autogen
		chmod +x configure

	./configure --prefix=/usr --localstatedir=/var --sysconfdir=/etc
	make && make install
	vi /etc/ld.so.conf.d/libxrdp.conf
		/usr/lib/xrdp
	ldconfig	<--- set a path of shared libraries

4. Install a OvdServer
	Install a requirement package
		apt-get install python-svn (pysvn module)

	cd OvdServer
	vi autogen
	./autogen
	./setup.py build
	./setup.py install --prefix=/usr

5. Install a OvdShell

	cd ApplicationServer/OvdShells
	./autogen
	./setup.py build
	./setup.py install --prefix=/usr
	
6. Run a ulteo-ovd-subsystem
	exit (from uchroot)
	service ulteo-ovd-subsystem start

Change Logo's:
A intro page's logo is "/usr/share/ulteo/sessionmanager/admin/media/image/main.png"
And a session manager's menu logo is "/usr/share/ulteo/sessionmanager/admin/media/image/menu/main.png"