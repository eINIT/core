# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit subversion

ESVN_REPO_URI="svn://svn.berlios.de/einit/trunk/modules/xml"
SRC_URI=""

DESCRIPTION=".xml modules for eINIT"
HOMEPAGE="http://einit.org/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~ppc ~x86"
IUSE="doc"

RDEPEND=">=sys-apps/einit-0.24.1_rc1816
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/gentoo

SVN_REVISION="${PV/*_rc}"

ESVN_OPTIONS="-r${SVN_REVISION}"

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

src_compile() {
	local myconf

	myconf="--ebuild --svn --prefix=${ROOT}"

	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation || die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog COPYING
	if use doc ; then
		dohtml build/documentation/html/*
	fi
}
