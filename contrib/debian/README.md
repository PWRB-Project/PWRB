
Debian
====================
This directory contains files used to package pwrbd/pwrb-qt
for Debian-based Linux systems. If you compile pwrbd/pwrb-qt yourself, there are some useful files here.

## pwrb: URI support ##


pwrb-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install pwrb-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your pwrb-qt binary to `/usr/bin`
and the `../../share/pixmaps/pwrb128.png` to `/usr/share/pixmaps`

pwrb-qt.protocol (KDE)

