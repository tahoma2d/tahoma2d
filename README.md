# Tahoma2D

[![](https://ci.appveyor.com/api/projects/status/mnc3mepksux9kvap/branch/master?svg=true)](https://ci.appveyor.com/project/tahoma2d/tahoma2d)

## What is Tahoma2D?

Tahoma2D is a 2D and stop motion animation software.It is based on **Toonz Studio
Ghibli Version**, originally developed in Italy by
[Digital Video, Inc.](http://www.toonz.com/), and customized by
[Studio Ghibli](http://www.ghibli.jp/) over many years of production.


# Tahoma2D on FreeBSD — Build, Installation & Packaging Guide

Complete instructions for compiling, patching, integrating into XFCE/X11, and packaging **[Tahoma2D](https://tahoma2d.org)** on **FreeBSD** (tested with Clang 19 and Qt5).

---

## Table of Contents
- [1. Install Dependencies](#1-install-dependencies)
- [2. Clone & Patch Source Code](#2-clone--patch-source-code)
- [3. Configure, Compile & Install](#3-configure-compile--install)
- [4. Post-Install Setup (Assets, CLI & Desktop Menu)](#4-post-install-setup-assets-cli--desktop-menu)
- [5. Packaging & Distribution](#5-packaging--distribution)
  - [Option A: Native FreeBSD `.pkg` Archive](#option-a-native-freebsd-pkg-archive)
  - [Option B: Portable Self-Contained `.tar.zst` Bundle](#option-b-portable-self-contained-tarzst-bundle)
- [6. Uninstalling](#6-uninstalling)

---

## 1. Install Dependencies

FreeBSD splits Qt5 into individual module packages instead of `qt5-base` or `qt5-tools`. Install all required build tools, Qt5 components, linear algebra solvers, and multimedia libraries via `pkg`:

```sh
sudo pkg install git cmake pkgconf ninja boost-libs opencv \
  qt5-core qt5-gui qt5-widgets qt5-network qt5-opengl qt5-xml \
  qt5-printsupport qt5-multimedia qt5-script qt5-svg \
  qt5-serialport qt5-uitools qt5-buildtools qt5-qmake qt5-linguisttools \
  qt5-style-plugins superlu libmypaint libjpeg-turbo png \
  liblz4 lzo2 freeglut glew openblas ffmpeg
```

---

## 2.Patch Source Code

Applying three small compatibility patches for Clang on FreeBSD:

| File | Issue on FreeBSD | Fix |
| :--- | :--- | :--- |
| `toonz/sources/include/toonz4.6/machine.h` | Falls through to `@UNKNOWN PLATFORM @` when `LINUX` is undefined. | Add `defined(FREEBSD) \|\| defined(__FreeBSD__)` to the Linux preprocessor branch. |
| `toonz/sources/stopmotion/jpgconverter.h` | Uses MSVC-specific `unsigned __int64` rejected by Clang. | Replace `unsigned __int64` with standard `unsigned long long`. |
| `toonz/sources/toonz/crashhandler.cpp` | Uses glibc-only `dladdr1()` / `RTLD_DL_LINKMAP` and gates `printModuleInfo` behind `LINUX`. | Enable `FREEBSD` for `printModuleInfo` and subtract `(size_t)info.dli_fbase` from POSIX `dladdr()`. |

### Apply Patches

```sh
git clone https://github.com
cd tahoma2d

# 1. Recognize FreeBSD in legacy 4.6 machine headers
sed -i '' -E 's/defined\((LINUX\vert{}__linux__)\)/defined(\1) || defined(FREEBSD) || defined(__FreeBSD__)/g' \
  toonz/sources/include/toonz4.6/machine.h

# 2. Replace Windows __int64 types in stop-motion headers/sources
sed -i '' 's/unsigned __int64/unsigned long long/g' \
  toonz/sources/stopmotion/*.h toonz/sources/stopmotion/*.cpp

# 3. Replace glibc dladdr1 with standard POSIX dladdr in crash handler
sed -i '' \
  -e 's/^#ifdef LINUX\$/#if defined(LINUX) || defined(FREEBSD)/' \
  -e 's/link_map \*link_map;/if (infoFound) addr -= (size_t)info.dli_fbase;/' \
  -e '/dladdr1(buffer\[i\], &info, (void \*\*)&link_map, RTLD_DL_LINKMAP);/d' \
  -e '/addr -= link_map->l_addr;/d' \
  toonz/sources/toonz/crashhandler.cpp

# (Optional) Save a reusable patch file for future git updates
git diff > freebsd.patch
```

---

## 3. Configure, Compile & Install

Configure an out-of-source Ninja build inside `toonz/build`, pointing CMake to `/usr/local` and disabling Linux-specific stop-motion USB hooks (`-DWITH_STOPMOTION=OFF`):

```sh
mkdir -p toonz/build
cd toonz/build

cmake ../sources \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local/tahoma2d \
  -DCMAKE_PREFIX_PATH=/usr/local \
  -DWITH_STOPMOTION=OFF

ninja -j\$(sysctl -n hw.ncpu)
sudo ninja install
```

> **Note:** If your system is low on RAM during C++ compilation, reduce parallel jobs (e.g., `ninja -j4`).

---

## 4. Post-Install Setup (Assets, CLI & Desktop Menu)

### A. Copy the `stuff` Runtime Directory
`ninja install` does not copy the `stuff` folder into `/usr/local/tahoma2d/share/tahoma2d/stuff`. Without it, Tahoma2D launches unstyled and crashes on startup:

```sh
sudo mkdir -p /usr/local/tahoma2d/share/tahoma2d
sudo cp -R ../../stuff /usr/local/tahoma2d/share/tahoma2d/stuff

# If Tahoma2D was launched before copying stuff, reset the incomplete user config:
rm -rf ~/.config/Tahoma2D/stuff
```

### B. Create a `/usr/local/bin/tahoma2d` Wrapper Script
Because `/usr/local/tahoma2d/bin/tahoma2d` resolves relative paths using `$(dirname "$0")/..`, symlinking it into `/usr/local/bin` breaks binary discovery. Use a wrapper script instead:

```sh
sudo tee /usr/local/bin/tahoma2d > /dev/null << 'EOF'
#!/bin/sh
exec /usr/local/tahoma2d/bin/tahoma2d "\$@"
EOF
sudo chmod +x /usr/local/bin/tahoma2d
```

### C. Add Application Menu Entry & Icon (XFCE / X11)
Register the `.desktop` entry pointing to the installed 256×256 icon (`org.tahoma2d.Tahoma2D.png`) and refresh desktop caches:

```sh
sudo tee /usr/local/share/applications/tahoma2d.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=Tahoma2D
Comment=2D Animation Software
Exec=/usr/local/bin/tahoma2d
Icon=/usr/local/tahoma2d/share/icons/hicolor/256x256/apps/org.tahoma2d.Tahoma2D.png
Terminal=false
Type=Application
Categories=Graphics;2DGraphics;RasterGraphics;VectorGraphics;
EOF

sudo update-desktop-database /usr/local/share/applications
sudo gtk-update-icon-cache -f -t /usr/local/tahoma2d/share/icons/hicolor 2>/dev/null || true
```

Launch Tahoma2D from any terminal:

```sh
tahoma2d
```

---

## 5. Packaging & Distribution

### Option A: Native FreeBSD `.pkg` Archive

Build an installable `.pkg` archive (~20 MB) that tracks system dependencies via FreeBSD's package manager:

```sh
rm -rf ~/tahoma2d-pkg
mkdir -p ~/tahoma2d-pkg/stage/usr/local/bin ~/tahoma2d-pkg/stage/usr/local/share/applications

sudo cp -Rp /usr/local/tahoma2d ~/tahoma2d-pkg/stage/usr/local/
sudo cp -p /usr/local/bin/tahoma2d ~/tahoma2d-pkg/stage/usr/local/bin/
sudo cp -p /usr/local/share/applications/tahoma2d.desktop ~/tahoma2d-pkg/stage/usr/local/share/applications/
sudo chown -R (id -u):(id -g) ~/tahoma2d-pkg

cat << EOF > ~/tahoma2d-pkg/+MANIFEST
name: tahoma2d
version: "1.5.0"
origin: graphics/tahoma2d
comment: "2D animation production software based on OpenToonz"
desc: "Tahoma2D is an open-source 2D animation software forked from OpenToonz."
maintainer: "local@freebsd"
www: "https://tahoma2d.org"
prefix: /usr/local
deps: {
\$(for p in qt5-core qt5-gui qt5-widgets qt5-network qt5-opengl qt5-xml qt5-printsupport qt5-multimedia qt5-script qt5-svg qt5-serialport qt5-uitools qt5-style-plugins opencv superlu libmypaint libjpeg-turbo png liblz4 lzo2 freeglut glew openblas ffmpeg boost-libs; do
  pkg query '  %n: { origin: "%o", version: "%v" }' "\$p" 2>/dev/null
done)
}
EOF

(cd ~/tahoma2d-pkg/stage/usr/local && find . -type f -o -type l | sed 's|^\./||' | sort) > ~/tahoma2d-pkg/plist
pkg create -m ~/tahoma2d-pkg -p ~/tahoma2d-pkg/plist -r ~/tahoma2d-pkg/stage -o ~/tahoma2d-pkg
```

Install the generated package on any matching FreeBSD system:

```sh
sudo pkg add ~/tahoma2d-pkg/tahoma2d-1.5.0.pkg
```

---

### Option B: Portable Self-Contained `.tar.zst` Bundle

Bundle all non-base `/usr/local/lib` shared objects, FFmpeg binaries, and required Qt5 plugins (`platforms`, `xcbglintegrations` for OpenGL viewport rendering, `imageformats` for SVG/PNG icons, and `styles`) into a relocatable `.tar.zst` archive:

```sh
rm -rf ~/tahoma2d-portable
mkdir -p ~/tahoma2d-portable/bin/plugins
cp -Rp /usr/local/tahoma2d/* ~/tahoma2d-portable/

# 1. Copy Qt5 XCB, OpenGL GLX integration, imageformat, and style plugins
cp -Rp /usr/local/lib/qt5/plugins/platforms \
       /usr/local/lib/qt5/plugins/xcbglintegrations \
       /usr/local/lib/qt5/plugins/imageformats \
       /usr/local/lib/qt5/plugins/styles \
       ~/tahoma2d-portable/bin/plugins/

# 2. Bundle FFmpeg/FFprobe binaries
cp -p /usr/local/bin/ffmpeg /usr/local/bin/ffprobe ~/tahoma2d-portable/bin/

# 3. Copy third-party /usr/local shared libraries into lib/tahoma2d/
ldd ~/tahoma2d-portable/bin/Tahoma2D ~/tahoma2d-portable/lib/tahoma2d/*.so ~/tahoma2d-portable/bin/plugins/*/*.so ~/tahoma2d-portable/bin/ffmpeg 2>/dev/null | awk '/=> \/usr\/local\// {print \$3}' | sort -u | xargs -I {} cp -n {} ~/tahoma2d-portable/lib/tahoma2d/

# 4. Export QT_PLUGIN_PATH inside the portable launcher script
sed -i '' 's|^exec \$TAHOMA2D_BASE/bin/Tahoma2D|export QT_PLUGIN_PATH=\${TAHOMA2D_BASE}/bin/plugins\
exec \$TAHOMA2D_BASE/bin/Tahoma2D|' ~/tahoma2d-portable/bin/tahoma2d

# 5. Archive with Zstandard
tar -C ~ -cf - tahoma2d-portable | zstd -19 -o ~/Tahoma2D-FreeBSD-x86_64.tar.zst
```

Extract and run anywhere via the lowercase `tahoma2d` wrapper:

```sh
tar -xf Tahoma2D-FreeBSD-x86_64.tar.zst
./tahoma2d-portable/bin/tahoma2d
```

---

## 6. Uninstalling

If installed manually via `ninja install`:

```sh
sudo rm -rf /usr/local/tahoma2d /usr/local/bin/tahoma2d /usr/local/share/applications/tahoma2d.desktop
sudo update-desktop-database /usr/local/share/applications
```

If installed via `pkg add`:

```sh
sudo pkg delete tahoma2d
```
