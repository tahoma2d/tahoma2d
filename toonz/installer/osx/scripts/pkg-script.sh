#!/bin/sh
STUFF_DIR="Tahoma2D_stuff"
tar xzvf stuff.tar.bz2
mv stuff $STUFF_DIR
mkdir /Applications/Tahoma2D
cp -nr $STUFF_DIR /Applications/Tahoma2D
chmod -R 777 /Applications/Tahoma2D
rm -rf $STUFF_DIR

