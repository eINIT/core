# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit subversion

ESVN_REPO_URI="http://einit.svn.sourceforge.net/svnroot/einit/trunk/modules/gentoo"
SRC_URI=""

DESCRIPTION="Optional Gentoo compatibility modules for eINIT"
HOMEPAGE="http://einit.sourceforge.net/"

LICENSE="BSD GPL-2"
SLOT="0"
KEYWORDS="-*"
IUSE="doc static"

RDEPEND=">=sys-apps/einit-0.16.0
         >=sys-apps/baselayout-1.13.0_alpha12
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/gentoo

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

src_compile() {
	local myconf

	myconf="--ebuild --svn --enable-linux --use-posix-regex --prefix=${ROOT}"

	if use static ; then
		myconf="${myconf} --static"
	fi

	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation || die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog COPYING
#	doman documentation/man/*.8
	if use doc ; then
		dohtml build/documentation/html/*
	fi
}

pkg_postinst() {
	einfo
	einfo "The eINIT Gentoo compatibility modules are now installed, but you will still"
	einfo "need to enable the functionality you need."
	einfo "Edit /etc/einit/modules/gentoo.xml and uncomment the functionality you want."
	einfo
	ewarn
	ewarn "This module currently bundles GPL'd prerelease code from Gentoo's baselayout,"
	ewarn "which will be removed once baselayout-2 goes stable and hits the portage tree."
	ewarn "The license of the module itself is and always will be BSD if at all possible."
	ewarn
}
