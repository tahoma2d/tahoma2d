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

echo "→ Copia libtoonzlib (modificata da Ztoryc)..."
# NON copiare tutti i dylib: quelli nel bundle sono già patchati con i rpath
# corretti per le dipendenze di sistema (libtiff, libpng, ecc.).
# Copiare SOLO i dylib che Ztoryc modifica effettivamente.
# Al momento solo libtoonzlib (contiene TXshSoundColumn e altri core types).
cp "$BUILD/libtoonzlib.dylib" "$APP/Contents/MacOS/"

echo "→ Copia helper LZO (richiesti da TRasterCodecLZO)..."
cp "$BUILD/lzocompress"   "$APP/Contents/MacOS/lzocompress"
cp "$BUILD/lzodecompress" "$APP/Contents/MacOS/lzodecompress"

echo "→ Firma codice..."
codesign --force --deep --sign - "$APP"

echo "✓ Fatto. Apertura app..."
open "$APP"
