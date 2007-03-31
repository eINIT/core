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

IUSE_EINIT_CORE="module-so module-logic-v3 bootstrap-configuration-xml-expat bootstrap-configuration-stree log linux-sysconf linux-mount linux-process"
IUSE_EINIT_MODULES="feedback-visual-textual feedback-aural hostname external exec ipc module-exec module-daemon mount tty process parse-sh ipc-configuration shadow-exec module-transformations ipc-core-helpers scheduler compatibility-sysv-utmp compatibility-sysv-initctl"
IUSE_EINIT_EXPERIMENTAL="feedback-visual-fbsplash readahead"

IUSE="doc static debug"

for iuse_einit in ${IUSE_EINIT_CORE}; do
        IUSE="${IUSE} einit_core_${iuse_einit}"
done
for iuse_einit in ${IUSE_EINIT_MODULES}; do
        IUSE="${IUSE} einit_modules_${iuse_einit}"
done
for iuse_einit in ${IUSE_EINIT_EXPERIMENTAL}; do
        IUSE="${IUSE} einit_experimental_${iuse_einit}"
done

RDEPEND="dev-libs/expat"
DEPEND="${RDEPEND}
	doc? ( app-text/docbook-sgml app-doc/doxygen )
	>=sys-apps/portage-2.1.2-r11"

S=${WORKDIR}/${PN}

src_unpack() {
	subversion_src_unpack
	cd "${S}"
}

warn_about_use_expand() {
	einfo "We're trying to remodel the module selection using USE-Flags."
	einfo "To get the most out of that, add these lines to your make.conf:"
	einfo
	einfo "USE_EXPAND=\"EINIT_MODULES EINIT_CORE\""
	einfo "EINIT_CORE=\"module-so module-logic-v3 bootstrap-configuration-xml-expat bootstrap-configuration-stree log linux-sysconf linux-mount linux-process\""
	einfo "EINIT_MODULES=\"feedback-visual-textual feedback-aural hostname external exec ipc module-exec module-daemon mount tty process parse-sh ipc-configuration shadow-exec module-transformations ipc-core-helpers scheduler compatibility-sysv-utmp compatibility-sysv-initctl\""
	einfo
	einfo "not specifying this will just build everything, so you're not technically"
	einfo "\"missing out\" on anything."
}

src_compile() {
	local myconf internalmodules externalmodules

	if test -n "$(echo ${USE_EXPAND}|grep EINIT_)"; then
		if test -n "${EINIT_CORE}"; then
			for module in ${EINIT_CORE}; do
				if has einit_core_${module} ${IUSE}; then
					internalmodules="${internalmodules} ${module}"
				fi
			done
		else
			einfo "EINIT_CORE empty, building all modules"
		fi

                if test -n "${EINIT_MODULES}"; then
			for module in ${EINIT_MODULES}; do
				if has einit_modules_${module} ${IUSE}; then
					externalmodules="${externalmodules} ${module}"
				fi
			done
		else
			einfo "EINIT_MODULES empty, building all modules"
		fi
                if test -n "${EINIT_EXPERIMENTAL}"; then
			for module in ${EINIT_EXPERIMENTAL}; do
				if has einit_experimental_${module} ${IUSE}; then
					externalmodules="${externalmodules} ${module}"
				fi
			done
		fi
		internalmodules=`echo ${internalmodules} | sed 's/^[ \t]*//'`
		externalmodules=`echo ${externalmodules} | sed 's/^[ \t]*//'`
		echo "export INTERNALMODULES=\"${internalmodules}\"" >> configure.overrides
		echo "export EXTERNALMODULES=\"${externalmodules}\"" >> configure.overrides
	else
		warn_about_use_expand;
	fi

	myconf="--ebuild --svn --enable-linux --use-posix-regex --prefix=${ROOT}"

	if use static ; then
		local myconf="${myconf} --static"
	fi
	if use debug ; then
		local myconf="${myconf} --debug"
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

	if test -z "$(echo ${USE_EXPAND}|grep EINIT_)"; then
		warn_about_use_expand;
	fi
}
