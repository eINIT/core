inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.sourceforge.net/"
SRC_URI="mirror://sourceforge/einit/${P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc"
IUSE="doc"

RDEPEND="dev-libs/expat
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"
PDEPEND=""

src_unpack() {
	unpack ${P}.tar.bz2
}

src_compile() {
	local myconf

	myconf="--ebuild --enable-linux --use-posix-regex --prefix=${ROOT}"

	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation-html ||die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog COPYING
        doman documentation/man/*.8
	if use doc ; then
		dohtml build/documentation/html/*.html
	fi
        insinto /usr/share/eselect/modules
        doins ${FILESDIR}/einit.eselect
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
