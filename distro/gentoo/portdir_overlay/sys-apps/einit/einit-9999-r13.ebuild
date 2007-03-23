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

IUSE_EINIT_EXTMOD="
	einit_extmod_testa"
IUSE_EINIT_INTMOD="
	einit_intmod_textb"
IUSE="${IUSE_EINIT_EXTMOD}
      ${IUSE_EINIT_INTMOD}
      doc efl static"

RDEPEND="dev-libs/expat
	efl? ( media-libs/edje x11-libs/evas x11-libs/ecore )"
DEPEND="${RDEPEND}
	doc? ( app-text/docbook-sgml app-doc/doxygen )
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/${PN}

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

function einit_get_modules () {
	if test -z "$(echo $@|grep -w default)"; then
		for i in $@; do
			if test "${i}" = "-*"; then
				current=""
			elif test "${i}" = "*"; then
				current=${ALLMODULES}
			elif test "${i}" = "-.*"; then
				current=""
			elif test "${i}" = ".*"; then
				current=${ALLMODULES}
			elif test "${i:0:1}" = "-"; then
				newcur="";
				for z in ${current}; do
					if ! echo ${z}|egrep ${i:1}>/dev/null; then newcur="${newcur} ${z}"; fi
				done
				current="${newcur}";
			else
				newcur=${current};
				for z in ${ALLMODULES}; do
					if echo ${z}|egrep ${i}>/dev/null; then newcur="${newcur} ${z}"; fi
				done
				current="${newcur}";
			fi
		done;
		
		echo ${current}
	else
		echo "default"
	fi
}

src_compile() {
	local myconf

	myconf="--ebuild --svn --enable-linux --use-posix-regex --prefix=${ROOT}"

	if use efl ; then
		local myconf="${myconf} --enable-efl"
	fi
	if use static ; then
		local myconf="${myconf} --static"
	fi
	echo ${myconf}
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
	keepdir /etc/einit/local
	keepdir /etc/einit/modules
	if use doc ; then
		dohtml build/documentation/html/*
	fi
        insinto /usr/share/eselect/modules
        doins ${FILESDIR}/einit.eselect
}

pkg_postinst() {
	ewarn
	ewarn "This is a live SVN build and as such may be subject to weird errors."
	ewarn
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
