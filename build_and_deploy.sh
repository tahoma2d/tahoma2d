#!/bin/zsh
# build_and_deploy.sh
# Compila Ztoryc e aggiorna Ztoryc.app
# Uso: ./build_and_deploy.sh [file.cpp opzionale da toccare]

WORKSPACE="/Volumes/ZioSam/tahoma2d-workspace/tahoma2d"
BUILD="$WORKSPACE/toonz/build"
APP="$WORKSPACE/toonz/Ztoryc.app"
MACOS="$APP/Contents/MacOS"

if [ -n "$1" ]; then
  echo "→ Touch $1..."
  touch "$1"
fi

echo "→ Compilazione..."
ninja -j4 -C "$BUILD" 2>&1 | grep -E "error:|Linking|up-to-date"

echo "→ Copia binario..."
cp "$BUILD/toonz/Ztoryc.app/Contents/MacOS/Ztoryc" "$MACOS/Ztoryc"

echo "→ Copia dylib Ztoryc..."
# Dylib modificate da Ztoryc — aggiornate ad ogni build:
cp "$BUILD/libtoonzlib.dylib"           "$MACOS/"
cp "$BUILD/libtnzcore.dylib"            "$MACOS/"
cp "$BUILD/tnzbase/libtnzbase.dylib"    "$MACOS/"
cp "$BUILD/sound/libsound.dylib"        "$MACOS/"
cp "$BUILD/tnztools/libtnztools.dylib"  "$MACOS/"

echo "→ Patch rpath libimage (libtiff: /usr/local/lib → @executable_path)..."
install_name_tool -change \
  /usr/local/lib/libtiff.5.dylib \
  @executable_path/libtiff.5.dylib \
  "$BUILD/image/libimage.dylib" 2>/dev/null || true
cp "$BUILD/image/libimage.dylib" "$MACOS/"

# Dylib secondarie (raramente cambiate) — aggiorna dal build tree se presente
for lib in libcolorfx.dylib libtnzstdfx.dylib libtoonzqt.dylib libtfarm.dylib libtnzext.dylib; do
  actual=$(find "$BUILD" -name "$lib" -not -path "*/Ztoryc.app/*" -not -path "*CMakeFiles*" 2>/dev/null | head -1)
  if [ -n "$actual" ]; then
    cp "$actual" "$MACOS/"
  fi
done

echo "→ Copia risorse..."
# SystemVar.ini: path assoluti alle risorse (stuff dir). Non viene generato da CMake
# nella Ztoryc.app di deploy — va copiato esplicitamente dalla source o dalla Tahoma2D.app.
SYSVAR="$APP/Contents/Resources/SystemVar.ini"
if [ ! -f "$SYSVAR" ]; then
  cp "$WORKSPACE/toonz/install/SystemVar.ini" "$SYSVAR" 2>/dev/null || \
  cp "$WORKSPACE/toonz/Tahoma2D.app/Contents/Resources/SystemVar.ini" "$SYSVAR" 2>/dev/null || \
  echo "⚠️  SystemVar.ini non trovato — l'app non si avvia senza"
fi

echo "→ Copia helper LZO..."
cp "$BUILD/lzocompress"   "$MACOS/lzocompress"
cp "$BUILD/lzodecompress" "$MACOS/lzodecompress"

echo "→ Rimozione xattr e runtime artifacts prima della firma..."
# Rimuove attributi estesi (com.apple.provenance ecc.) che invalidano il seal
xattr -cr "$APP" 2>/dev/null || true
# Rimuove directory create a runtime dall'app (profiles/, cache/ ecc.)
# che non fanno parte del bundle sigillato
rm -rf "$APP/profiles" "$APP/cache" "$APP/logs" 2>/dev/null || true

echo "→ Firma codice (dylib prima, poi bundle)..."
# Prima firma ogni dylib singolarmente
for f in "$MACOS"/*.dylib; do
  codesign --force --sign - "$f" 2>/dev/null
done
# Poi firma i binari helper
codesign --force --sign - "$MACOS/lzocompress"   2>/dev/null
codesign --force --sign - "$MACOS/lzodecompress" 2>/dev/null
# Infine firma il bundle completo (senza --deep per evitare re-firma ricorsiva)
codesign --force --sign - --entitlements "$WORKSPACE/Ztoryc.entitlements" "$APP"

echo "✓ Fatto. Apertura app..."
open "$APP"
