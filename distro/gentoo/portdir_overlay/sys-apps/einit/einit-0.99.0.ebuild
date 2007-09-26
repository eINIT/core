# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

#
# eINIT SVN ebuild (v29)
#

inherit subversion

ESVN_REPO_URI="svn://svn.berlios.de/einit/trunk/${PN}"
SRC_URI=""

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.org/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="-*"
RESTRICT="strip"

IUSE="doc static debug nowtf externalise fbsplash aural dbus noxml baselayout2"

RDEPEND="dev-libs/expat
	sys-apps/iproute2
	>=dev-libs/libnl-1.0_pre6
	dbus? ( >=sys-apps/dbus-1.0.2-r2 )
	baselayout2? ( >=sys-apps/baselayout-2.0.0_rc2-r1 )
	!sys-apps/einit-modules-gentoo"
DEPEND="${RDEPEND}
	doc? ( app-text/docbook-sgml app-doc/doxygen )
	>=sys-apps/portage-2.1.2-r11"
PDEPEND="!noxml? ( sys-apps/einit-modules-xml )"

S=${WORKDIR}/${PN}

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

src_compile() {
	local myconf

	myconf="--ebuild --svn --prefix=/ --with-expat=/usr/lib/libexpat.a"

	if use static ; then
		local myconf="${myconf} --static"
	fi
	if use debug ; then
		local myconf="${myconf} --debug"
	fi
	if use nowtf ; then
		local myconf="${myconf} --nowtf"
	fi
	if use dbus ; then
		myconf="${myconf} --enable-ipc-dbus"
	fi
	if use baselayout2 ; then
		myconf="${myconf} --distro-support=gentoo"
	fi
	if use externalise ; then
		local myconf="${myconf} --externalise"
	fi
	if ! use fbsplash ; then
		local myconf="${myconf} --no-feedback-visual-fbsplash"
	fi
	if ! use aural ; then
		local myconf="${myconf} --no-feedback-aural --no-feedback-aural-festival"
	fi
	
	echo ${myconf}
	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation || die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}/${ROOT}" || die
	dodoc AUTHORS ChangeLog COPYING
	doman documentation/man/*.8
	keepdir /etc/einit/local
	keepdir /etc/einit/modules
	if use doc ; then
		dohtml build/documentation/html/*
	fi
}

pkg_postinst() {
	ewarn
	ewarn "This is a live SVN build and as such may be subject to weird errors."
	ewarn
	einfo "eINIT is now installed, but you will still need to configure it."
	if use doc ; then
		einfo
		einfo "Since you had the doc use-flag enabled, you should find the user's guide"
		einfo "in /usr/share/doc/einit-version/html/"
	fi
	einfo
	einfo "You can always find the latest documentation at"
	einfo "http://einit.org/"
	einfo
	einfo "I'm going to run 'einit --wtf' now, to see if there's anything you'll need"
	einfo "to set up."
	einfo
	chroot ${ROOT} /sbin/einit --wtf
	einfo
	einfo "Done; make sure you follow any advice given in the output of the command that"
	einfo "just ran. If you wish to have einit re-evaluate the current state, just run"
	einfo "'/sbin/einit --wtf' in a root-shell near you."
	einfo
}
