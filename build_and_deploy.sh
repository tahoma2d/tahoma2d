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
# Regola: copiare SOLO le dylib che Ztoryc modifica + quelle che dipendono
# direttamente da esse con symbol compatibility issues.
# NON copiare dylib che dipendono da librerie di sistema (libtiff, libpng, ecc.)
# con rpath patchati nel bundle originale: libimage, libcolorfx, libtnzstdfx.
#
# libtoonzlib  — TXshSoundColumn (ColumnLevel overlap fix), TXsheet
# libtnzcore   — TSoundOutputDevice::processedUsecs() (audio master clock)
# libtnzbase   — dipende da libtnzcore; senza questa, m_unknownClassCode manca
# libsound     — audio runtime (stesso set della sessione precedente)
# libtnztools  — tooloptions.cpp, toonzrasterbrushtool.cpp (autofill)
cp "$BUILD/libtoonzlib.dylib"           "$APP/Contents/MacOS/"
cp "$BUILD/libtnzcore.dylib"            "$APP/Contents/MacOS/"
cp "$BUILD/tnzbase/libtnzbase.dylib"    "$APP/Contents/MacOS/"
cp "$BUILD/sound/libsound.dylib"        "$APP/Contents/MacOS/"
cp "$BUILD/tnztools/libtnztools.dylib"  "$APP/Contents/MacOS/"

echo "→ Copia helper LZO (richiesti da TRasterCodecLZO)..."
cp "$BUILD/lzocompress"   "$APP/Contents/MacOS/lzocompress"
cp "$BUILD/lzodecompress" "$APP/Contents/MacOS/lzodecompress"

echo "→ Firma codice..."
codesign --force --deep --sign - --entitlements "$WORKSPACE/Tahoma2D.entitlements" "$APP"

echo "✓ Fatto. Apertura app..."
open "$APP"
