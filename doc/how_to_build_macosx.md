
# Setting Up the Development Environment on macOS

## Necessary Software

- git
- brew
- Xcode
- cmake (3.2.2 or later)
- Qt (5.9.2 or later)
- boost (1.55.0 or later)
- OpenCV
- Jpeg-turbo

## Building on macOS
### Download boost from https://boost.org

Download the .bz2 mac (unix if mac not specified) version 1_55_0 or later (last tested with 1_72_0)

Save for later step.

### Download and install Xcode from Apple

When downloading Xcode, you should use the appropriate version for your OS version.  You can refer to the Version Comparison Table on https://en.wikipedia.org/wiki/Xcode to find out which version you should use.

Apple store usually provides for the most recent macOS version.  For older versions, you will need to go to the Apple Developer site.

After installing the application, you will need to start it in order to complete the installation.


### Install Homebrew from https://brew.sh

Check site for any changes in installation instructions, but they will probably just be this:

1. Open a Terminal window
2. Execute the following statement:
```
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### Install required software using brew

In a Terminal window, execute the following statements:
```
$ brew install glew lz4 libjpeg libpng lzo pkg-config libusb cmake git-lfs libmypaint qt opencv jpeg-turbo
$ git lfs install
```

NOTE: This will install the latest version of QT which may not be compatible with older OS versions.

If you cannot use the most recent version, download the online installer from https://www.qt.io/download and install the appropriate `macOS` version (min 5.9.2).  If installing via this method, be sure to install the `Qt Script (Deprecated)` libraries.

### Remove incompatible symbolic directory
Check to see if this symbolic glew directory exists. If so, remove it:
```
$ ls -l /usr/local/lib/cmake/glew
$ rm /usr/local/lib/cmake/glew
```

### Set up OpenToonz repository

These steps will put the OpenToonz repository under /Users/yourlogin/Documents.
```
$ cd ~/Documents   #or where you want to store the repository#
$ git clone https://github.com/opentoonz/opentoonz
$ cd opentoonz
$ git lfs pull
$ cd thirdparty/boost
$ mv ~/Downloads/boost_1_72_0.tar.bz2 .   #or whatever the boost filename you downloaded is#
$ tar xvjf boost_1_72_0.tar.bz2
$ cd ../lzo
$ cp -r 2.03/include/lzo driver
$ cd ../tiff-4.0.3
$ ./configure --disable-lzma && make
```

### Configure build for QT version

Edit the `/Users/yourlogin/Documents/opentoonz/toonz/sources/CMakeLists.txt` file at line 160 (64-bit OS) or 172 (32-bit OS) and modify the root path for the QT lib directory

If you installed QT using `brew`, you can get the version and path to use with: `$ brew info qt`.  The lib path will look something like this: `/usr/local/Cellar/qt/5.12.2/lib`
If you downloaded the QT installer and installed to `/Users/yourlogin/Qt`, your lib path may look something like this: `~/Qt/5.12.2/clang_64/lib` or `~/Qt/5.12.2/clang_32/lib`

### Configure environment and Build OpenToonz

1. Create the build directory with the following:
```
$ cd ~/Documents/opentoonz/toonz
$ mkdir build
$ cd build
$ export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/opt/jpeg-turbo/lib/pkgconfig"
```

2. Set up build environment

To build from command line, do the following:
```
$ CMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.12.2 cmake ../sources   #replace QT path with your installed QT version#
$ make
```

To build using Xcode, do the following:
```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
$ CMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.12.2 cmake -G Xcode ../sources -B.   #replace QT path with your installed QT version#
```

- Open Xcode app and open project /Users/yourlogin/Documents/opentoonz/toonz/build/OpenToonz.xcodeproj
- Change `ALL_BUILD` to `OpenToonz`
- Start build with: Product -> Build

    - NOTE about rebuilding in Xcode: The initial build should succeed without any errors.  There after, the build will succeed but the following 3 errors can be ignored:

```
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/install_name_tool: for: /Users/yourlogin/Documents/opentoonz/toonz/build/toonz/Debug/OpenToonz.app/Contents/MacOS/OpenToonz (for architecture x86_64) option "-add_rpath @executable_path/." would duplicate path, file already has LC_RPATH for: @executable_path/.
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/install_name_tool: for: /Users/yourlogin/Documents/opentoonz/toonz/build/toonz/Debug/OpenToonz.app/Contents/MacOS/OpenToonz (for architecture x86_64) option "-add_rpath /usr/local/Cellar/qt/5.12.2/lib/" would duplicate path, file already has LC_RPATH for: /usr/local/Cellar/qt/5.12.2/lib/
Command /bin/sh emitted errors but did not return a nonzero exit code to indicate failure
```

Side note: If you want the option to build by command line and Xcode, create a separate build directory for each.

### Create the stuff Directory

If you have installed OpenToonz on the machine already, you can skip this.  Otherwise, you need to create the stuff folder with the following:
```
$ cd ~/Documents/opentoonz
$ sudo mkdir /Applications/OpenToonz
$ sudo cp -r stuff /Applications/OpenToonz/OpenToonz_stuff
$ sudo chmod -R 777 /Applications/OpenToonz
```
## Building with extended stop motion support for webcams and Canon DSLR cameras.
You will need one additional library:
  - The Canon SDK.  This requires applying for the Canon developer program and downloading the SDK.

Copy the Header and Framework folders from the Canon SDK to `$opentoonz/thirdparty/canon`

Edit the `/Users/yourlogin/Documents/opentoonz/toonz/sources/CMakeLists.txt` file at the end of line 104, changing the `WITH_CANON` build option from `OFF` to `ON`.

To run the program with stop motion support, you will need to copy the EDSDK.framework directory from the Canon SDK and place it in `Macintosh HD/Library/Frameworks`. It could also be placed in the same folder as `OpenToonz.app`. If opening OpenToonz.app fails because "EDSDK.framework can't be opened", go to `Preferences -> Security & Privacy -> General` and select "Open Anyway". Open OpenToonz again and select "Open" when the "EDSDK.framework can't be opened" warning appears.

### Running the build

- If built using command line, run the following:
```
$ open ~/Documents/opentoonz/build/toonz/OpenToonz.app
```

- If built using Xcode, do the following:

    - Open Scheme editor for OpenToonz: Product -> Scheme -> Edit Scheme
    - Uncheck: Run -> Options -> Document Versions
    - Run in Debug mode: Product -> Run

    - To open with command line or from Finder window, the application is found in `/Users/yourlogin/Documents/opentoonz/toonz/build/Debug/OpenToonz.app`
