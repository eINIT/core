inherit subversion

ESVN_REPO_URI="svn://svn.berlios.de/einit/trunk/util/einit-notify"
SRC_URI=""

DESCRIPTION="A small Notification Area Icon for eINIT"
HOMEPAGE="http://einit.org/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="-*"
IUSE="doc"

RDEPEND=">=sys-apps/einit-0.24.1_rc1816
	>=dev-cpp/gtkmm-2.10
	>=x11-libs/libnotify-0.4.3
	doc? ( app-text/docbook-sgml app-doc/doxygen )"
DEPEND="${RDEPEND}
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/einit-notify

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
