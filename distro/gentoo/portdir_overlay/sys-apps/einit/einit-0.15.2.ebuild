inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.sourceforge.net/"
SRC_URI="mirror://sourceforge/einit/${P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc ~arm"
IUSE="doc efl static"

RDEPEND="dev-libs/expat
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}"
PDEPEND=""

src_unpack() {
	unpack ${P}.tar.bz2
}

src_compile() {
	local myconf

	myconf="--ebuild --svn --enable-linux --use-posix-regex --prefix=${ROOT}"

	if use efl ; then
		myconf="${myconf} --enable-efl"
	fi
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
	doman documentation/man/*.8
	if use doc ; then
		dohtml build/documentation/html/*
	fi
        insinto /usr/share/eselect/modules
        doins ${FILESDIR}/einit.eselect
}
