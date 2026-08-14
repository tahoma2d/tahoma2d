#!/bin/bash
cd thirdparty

echo ">>> Cloning libgphoto2"
git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src

cd libgphoto2_src

git checkout tahoma2d-version-2.5.34

export GETTEXT_PATH=`brew --prefix gettext`

echo ">>> Configuring libgphoto2"
autoreconf --install --symlink

./configure --prefix=/usr/local \
  CPPFLAGS="-I$GETTEXT_PATH/include" \
  LDFLAGS="-L$GETTEXT_PATH/lib -lintl"

if [ $? != 0 ]
then
   exit 1
fi

echo ">>> Making libgphoto2"
make

if [ $? != 0 ]
then
   exit 1
fi

echo ">>> Installing libgphoto2"
sudo make install

cd ..
