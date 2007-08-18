inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="A small Notification Area Icon for eINIT"
HOMEPAGE="http://einit.org/"
SRC_URI="mirror://berlios/einit/${P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc"

IUSE=""

RDEPEND=">=sys-apps/einit-0.24.2
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

src_unpack() {
	unpack ${P}.tar.bz2
}

src_compile() {
	local myconf

	myconf="--ebuild --prefix=/"

	echo ${myconf}
	econf ${myconf} || die
	emake || die
}

src_install() {
	emake -j1 install DESTDIR="${D}/${ROOT}" || die
	dodoc AUTHORS ChangeLog COPYING README
}
