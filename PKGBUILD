# Maintainer: Mark Busking <markbusking@omarchy.xyz>
pkgname=om-rec
pkgver=1.0.0
pkgrel=1
pkgdesc="Minimalist, native C screen recorder for Omarchy/Hyprland (Wayland)"
arch=('x86_64')
url="https://github.com/markbusking/om-rec"
license=('MIT')
depends=('wf-recorder' 'slurp' 'libnotify')
makedepends=('gcc' 'make')
source=("${url}/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('SKIP') # Se actualiza con 'updpkgsums' antes de subir

build() {
    cd "$pkgname-$pkgver"
    make
}

package() {
    cd "$pkgname-$pkgver"
    # Instalamos en /usr en lugar de ~/.local
    make DESTDIR="$pkgdir/" PREFIX="/usr" install
}
