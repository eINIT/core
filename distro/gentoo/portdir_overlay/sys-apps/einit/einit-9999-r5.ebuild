# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit subversion

ESVN_REPO_URI="http://einit.svn.sourceforge.net/svnroot/einit/trunk/${PN}"
SRC_URI=""

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.sourceforge.net/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="-*"
IUSE="doc efl"

RDEPEND="dev-libs/expat
	doc? ( app-text/docbook-sgml app-doc/doxygen )
	efl? ( media-libs/edje x11-libs/evas x11-libs/ecore )"
DEPEND="${RDEPEND}"

S=${WORKDIR}/${PN}

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

src_compile() {
	local myconf

	myconf="--ebuild --svn"

	if use efl ; then
		myconf="${myconf} --enable-linux --use-posix-regex --prefix=${ROOT} --enable-efl"
	else
		myconf="${myconf} --enable-linux --use-posix-regex --prefix=${ROOT}"
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
	doman documentation/man/*.8
	if use doc ; then
		dohtml build/documentation/html/*
	fi
        insinto /usr/share/eselect/modules
        doins ${FILESDIR}/einit.eselect
}
