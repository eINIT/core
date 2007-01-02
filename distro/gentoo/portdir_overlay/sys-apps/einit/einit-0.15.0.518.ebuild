inherit eutils toolchain-funcs flag-o-matic subversion versionator

ESVN_REPO_URI="http://einit.svn.sourceforge.net/svnroot/einit/trunk/${PN}"
SRC_URI=""

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.sourceforge.net/"

ESVN_REVISION=$(get_version_component_range 4 ${PV})
ESVN_FETCH_CMD="svn co -r ${ESVN_REVISION}"
ESVN_UPDATE_CMD="svn up -r ${ESVN_REVISION}"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc ~arm"
IUSE="doc efl"

RDEPEND="dev-libs/expat
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}"
PDEPEND=""

S=${WORKDIR}/einit

src_unpack() {
        subversion_src_unpack
        cd "${S}"
}

src_compile() {
	local myconf

	myconf="--ebuild --enable-linux --use-posix-regex --prefix=${ROOT}"

	if use efl ; then
		myconf="${myconf} --enable-efl"
	fi
	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation-html ||die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog COPYING
	if use doc ; then
		dohtml build/documentation/html/*.html
	fi

}

pkg_postinst() {
	einfo
	einfo "eINIT is now installed, but you will still need to configure it."
	if use doc ; then
		einfo
		einfo "Since you had the doc use-flag enabled, you should find the user's guide"
		einfo "in /usr/share/doc/einit-version/html/"
	fi
	einfo
	einfo "You can always find the latest documentation at"
	einfo "http://einit.sourceforge.net/documentation/users/"
	einfo
}
