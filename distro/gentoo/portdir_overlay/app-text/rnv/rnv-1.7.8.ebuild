DESCRIPTION="A lightweight Relax NG Compact Syntax validator"
HOMEPAGE="http://www.davidashen.net/rnv.html"
SRC_URI="http://ftp.davidashen.net/PreTI/RNV/rnv-1.7.8.zip"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~arm"

RDEPEND="dev-libs/expat"
DEPEND="${RDEPEND}
	app-arch/unzip"

src_compile() {
	make -f Makefile.gnu || die
}

src_install() {
	dobin rnv rvp arx
	dodoc readme.txt license.txt
}
