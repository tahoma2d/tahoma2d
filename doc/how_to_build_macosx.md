# Setting Up the Development Environment on MacOSX

## Necessary Software

- git
- brew
- Xcode
- cmake
  - Version 3.2.2 confirmed to work.
- Qt
  - http://download.qt.io/official_releases/qt/5.5/5.5.1/
    - qt-opensource-mac-x64-clang-5.5.1.dmg
- boost
  - http://www.boost.org/users/history/version_1_55_0.html

## Building on MacOSX

### Using brew to Install Necessary Packages

```
$ brew install glew lz4 libjpeg libpng lzo pkg-config
```

### Cloning the Repository

```
$ git clone https://github.com/opentoonz/opentoonz
```

### (Optional) Creating the stuff Directory

If the directory `/Applications/OpenToonz/OpenToonz_1.0_stuff` does not exist, enter the following command:

```
$ sudo cp -r opentoonz/stuff /Applications/OpenToonz/OpenToonz_1.0_stuff
```

### Building tiff in thirdparty

```
$ cd opentoonz/thirdparty/tiff-4.0.3
$ ./configure && make
```

### Installing boost to thirdpaty
The following assumes `boost_1_55_0.tar.bz2` was downloaded to `~/Downloads`.

```
$ cd ../boost
$ mv ~/Downloads/boost_1_55_0.tar.bz2 .
$ tar xjvf boost_1_55_0.tar.bz2
```

### Building Everything Together

```
$ cd ../../toonz
$ mkdir build
$ cd build
$ cmake ../sources
$ make
```

Please be patient as the install will take a while.

### After Building

```
$ open ./toonz/OpenToonz_1.0.app
```
