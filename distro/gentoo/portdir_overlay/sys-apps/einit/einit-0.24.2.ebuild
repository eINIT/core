inherit eutils toolchain-funcs flag-o-matic

DESCRIPTION="eINIT - an alternate /sbin/init"
HOMEPAGE="http://einit.org/"
SRC_URI="mirror://berlios/${PN}/${P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~ppc"

IUSE="doc static debug nowtf externalise fbsplash aural dbus"

RDEPEND="dev-libs/expat
        sys-apps/iproute2
        >=dev-libs/libnl-1.0_pre6
        dbus? ( >=sys-apps/dbus-1.0.2-r2 )"
DEPEND="${RDEPEND}
        doc? ( app-text/docbook-sgml app-doc/doxygen )
        >=sys-apps/portage-2.1.2-r11"
PDEPEND=">=sys-apps/einit-modules-xml-0.61.0"

src_unpack() {
	unpack ${P}.tar.bz2
}

src_compile() {
	local myconf

        myconf="--ebuild --prefix=/ --with-expat=/usr/lib/libexpat.a"

        if use static ; then
                local myconf="${myconf} --static"
        fi
        if use debug ; then
                local myconf="${myconf} --debug"
        fi
        if use nowtf ; then
                local myconf="${myconf} --nowtf"
        fi
        if use dbus ; then
                myconf="${myconf} --enable-ipc-dbus"
        fi
        if use externalise ; then
                local myconf="${myconf} --externalise"
        fi
        if ! use fbsplash ; then
                local myconf="${myconf} --no-feedback-visual-fbsplash"
        fi
        if ! use aural ; then
                local myconf="${myconf} --no-feedback-aural --no-feedback-aural-festival"
        fi
	
	echo ${myconf}
	econf ${myconf} || die
	emake || die

	if use doc ; then
		make documentation || die
	fi
}

src_install() {
	emake -j1 install DESTDIR="${D}/${ROOT}" || die
	dodoc AUTHORS ChangeLog COPYING
	doman documentation/man/*.8
	keepdir /etc/einit/local
	keepdir /etc/einit/modules
	if use doc ; then
		dohtml build/documentation/html/*
	fi
}

pkg_postinst() {
	ewarn
	einfo "eINIT is now installed, but you will still need to configure it."
	if use doc ; then
		einfo
		einfo "Since you had the doc use-flag enabled, you should find the user's guide"
		einfo "in /usr/share/doc/einit-version/html/"
	fi
	einfo
	einfo "You can always find the latest documentation at"
	einfo "http://einit.org/"
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
