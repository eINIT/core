inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="Optional Gentoo compatibility modules for eINIT"
HOMEPAGE="http://einit.sourceforge.net/"
SRC_URI="mirror://sourceforge/einit/einit-gentoo-compat-${PV}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

RDEPEND=">=sys-apps/einit-0.22.0
         >=sys-apps/baselayout-2.0.0_alpha1"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/einit-gentoo-compat-${PV}

src_unpack() {
	if [ "${A}" != "" ]; then
		unpack ${A}
	fi
}

src_compile() {
	local myconf

	myconf="--ebuild --enable-linux --use-posix-regex --prefix=${ROOT}"

	econf ${myconf} || die
	emake || die
}

src_install() {
	emake -j1 install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog COPYING
}

pkg_postinst() {
	einfo
	einfo "Edit /etc/einit/modules/gentoo.xml and (un-)comment the functionality you want."
	einfo
}
