inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.org/"
SRC_URI="mirror://berlios/einit/${P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc"

IUSE=""

RDEPEND=">=sys-apps/einit-0.25.0
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

src_unpack() {
	unpack ${P}.tar.bz2
}

src_compile() {
	local myconf

	myconf="--ebuild --enable-linux --prefix=/ --libdir-name=$(get_libdir)"

	echo ${myconf}
	econf ${myconf} || die
	emake || die
}

src_install() {
	emake -j1 install DESTDIR="${D}/${ROOT}" || die
	dodoc AUTHORS ChangeLog COPYING README
}

pkg_postinst() {
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
