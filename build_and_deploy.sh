#!/bin/zsh
# build_and_deploy.sh
# Compila Ztoryc e aggiorna Tahoma2D.app
# Uso: ./build_and_deploy.sh [file.cpp opzionale da toccare]

WORKSPACE="/Volumes/ZioSam/tahoma2d-workspace/tahoma2d"
BUILD="$WORKSPACE/toonz/build"
APP="$WORKSPACE/toonz/Tahoma2D.app"

if [ -n "$1" ]; then
  echo "→ Touch $1..."
  touch "$1"
fi

echo "→ Compilazione..."
ninja -j4 -C "$BUILD" 2>&1 | grep -E "error:|Linking|up-to-date"

echo "→ Copia binario..."
cp "$BUILD/toonz/Tahoma2D.app/Contents/MacOS/Tahoma2D" "$APP/Contents/MacOS/Tahoma2D"

echo "→ Copia dylib Ztoryc..."
# Regola: copiare le dylib che Ztoryc modifica + dipendenze ABI.
# libimage: richiede patch rpath libtiff (/usr/local/lib non esiste su questo Mac).
#   Patch applicata ogni build con install_name_tool prima della copia.
# libcolorfx, libtnzstdfx: NON copiare (dipendono da libimage via rpath diversi).
#
# libtoonzlib  — TXshSoundColumn (ColumnLevel overlap fix), TXsheet
# libtnzcore   — TSoundOutputDevice::processedUsecs() (audio master clock)
# libtnzbase   — dipende da libtnzcore; ABI m_unknownClassCode
# libsound     — audio runtime
# libtnztools  — tooloptions.cpp, toonzrasterbrushtool.cpp (autofill)
# libimage     — TLevelWriterTzl: deve essere coerente con libtnzcore (stessa build)
cp "$BUILD/libtoonzlib.dylib"           "$APP/Contents/MacOS/"
cp "$BUILD/libtnzcore.dylib"            "$APP/Contents/MacOS/"
cp "$BUILD/tnzbase/libtnzbase.dylib"    "$APP/Contents/MacOS/"
cp "$BUILD/sound/libsound.dylib"        "$APP/Contents/MacOS/"
cp "$BUILD/tnztools/libtnztools.dylib"  "$APP/Contents/MacOS/"

echo "→ Patch rpath libimage (libtiff: /usr/local/lib → @executable_path)..."
install_name_tool -change \
  /usr/local/lib/libtiff.5.dylib \
  @executable_path/libtiff.5.dylib \
  "$BUILD/image/libimage.dylib" 2>/dev/null || true
cp "$BUILD/image/libimage.dylib"        "$APP/Contents/MacOS/"

echo "→ Copia helper LZO (richiesti da TRasterCodecLZO)..."
cp "$BUILD/lzocompress"   "$APP/Contents/MacOS/lzocompress"
cp "$BUILD/lzodecompress" "$APP/Contents/MacOS/lzodecompress"

echo "→ Firma codice..."
codesign --force --deep --sign - --entitlements "$WORKSPACE/Tahoma2D.entitlements" "$APP"

echo "✓ Fatto. Apertura app..."
open "$APP"
