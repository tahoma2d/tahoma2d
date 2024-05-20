# Build Tahoma2D on Windows

This software can be built using Visual Studio 2019 or above and Qt 5 (min 5.15.2)

Throughout these instructions `$tahoma2d` represents the full path to your local Git repository for Tahoma2D.

## Required Software

### Microsoft Visual Studio 2019 or higher for Windows Desktop
- [ ] Install Microsoft Visual Studio 2019 or higher for Windows Desktop, https://www.visualstudio.com/vs/older-downloads/
  - Community and Professional versions of Visual Studio for Windows Desktop also work.
  - [ ] Make sure the architecture is "x64".
  - [ ] During the installation, make sure to select all the Visual C++ packages.

### Qt - cross-platform GUI framework.
- [ ] Install via **one** of the following methods.
  - If you want WinTab support, install the customized Qt 5.15.2 w/ WinTab support created by Shun-iwasawa. **(This is required if you plan on running on a Wacom tablet!)**
	- [ ] Download `Qt5.15.2_wintab.zip` from https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab
	- [ ] Extract the contents to `C:\Qt\5.15.2_wintab`

  - Install Qt 5.15.x (min 5.15.2) from https://www.qt.io/download-open-source/.
    - [ ] Click the `Download the Qt Online Installer` button at the bottom of the page.
    - [ ] Run the the installer and choose the Qt version to install.
      - If you do not see 5.15.x you may need to tick the Archive checkbox to see older versions in the list.
      - Include the following components
        - `MSVC 2019 64-bit`
        - `Qt Script (Deprecated)`
        - `Qt Charts`
        - `Qt Data Visualization`
        - `Qt Network Authorization`

### CMake
This will be used to create the `MSVC` project file.
- [ ] If CMake was installed with Qt, no additional installation is required.

  The typical location of CMake installed with Qt is: `C:\Qt\Tools\CMake_64\bin\cmake-gui.exe`
- [ ] If CMake was not installed download and install https://cmake.org/download/

### Boost
Boost 1.55.0 or later is required (tested up to 1.75.0).
- [ ] Download `boost_1_75_0.zip` from http://www.boost.org/users/history/version_1_75_0.html
- [ ] Extract contents to a directory.
  - For example: `C:\boost\boost_1_75_0`

### OpenCV
- [ ] Download OpenCV v4.5.1 or later from https://opencv.org/. 
- [ ] Install to a directory such as `C:\opencv\451`

  Recommend using the version currently used by Tahoma2D.

## Acquire the Source Code
You can use GitHub Desktop https://desktop.github.com/ or Git command line.
- [ ] Fork the base repository to your own GitHub account.
- [ ] Clone the base repository to your local environment.
- Visual Studio cannot properly recognize source code which is UTF-8 without BOM (Byte Order Mark). Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to also be treated as a comment by `Microsoft Visual Studio`. In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
  - [ ] From a command line navigate to `$tahoma2d` and execute: `git config core.safecrlf true`

### Download the 'lib' and 'dll' files stored with Large File Storage (LFS)
- If you used the GitHub Desktop UI to clone to your local environment the lib and dll files will already be downloaded and you can skip this section.
- Perform these steps if you are using the Git command line to acquire the code.
- `lib` and `dll` files are tracked by [Git Large File Storage](https://git-lfs.github.com/).
- [ ] Ensure git-lfs is installed in your local Git environment. The command `git lfs` should return Git LFS information.
- Installation instructions for git-lfs: https://docs.github.com/en/github/managing-large-files/installing-git-large-file-storage
- From Git command line execute: 
  - [ ] `git clone`
  - [ ] `git lfs pull`

### Use CMake to Create a Visual Studio Project
- This is only done 1x unless changing Qt, OpenCV or Boost version.
- [ ] Launch CMake GUI. You can find it in this Qt subfolder
- [ ] In `Where is the source code`, navigate to `$tahoma2d\toonz\sources`
- [ ] In `Where to build the binaries`, navigate to or enter `$tahoma2d\toonz\build`
- [ ] Click on `Configure`
- [ ] If this is the 1st time for this build directory, a pop-up will appear
  - Select the version of Visual Studio you are using. 
  - Select the `x64` Target Environment.
- [ ] Click on the value for `QT_PATH` in the top half of the CMake window and update the path to match where your QT library was installed.
    - For example: `C:\Qt\5.15.5_wintab\msvc2019_64` for Visual Studio 2019. 
- [ ] Select the following as desired
    - `WITH_CANON` - For Stop motion with Canon DSLR cameras. (Only use if you have the Canon SDK)
  	- `WITH_GPHOTO2` - For Stop Motion using non-Canon cameras.
  	- `WITH_WINTAB` - Only use if you installed the customized Qt 5.15.2 w/ WinTab, mentioned above
- [ ] For generating PDB files needed for detailed crash reporting, add `/Zi` to the following entries (example: `/MDd /Zi /Ob0 /Od /RTC1`)
  - `CMAKE_CXX_FLAGS_DEBUG`
  - `CMAKE_CXX_FLAGS_RELWITHDEBINFO`
  - `CMAKE_C_FLAGS_DEBUG`
  - `CMAKE_C_FLAGS_RELWITHDEBINFO`
- [ ] Rerun `Configure`.
- [ ] Click on `OpenCV_DIR` and update the path to match where your OpenCV library was installed.
  - For example: `C:\opencv\451\build`.
- [ ] Rerun `Configure`.
- [ ] Click on `BOOST_ROOT` and `Boost_INCLUDE_DIR` and update the path to match where your Boost library was installed.
  - For example: `C:\boost\boost_1_75_0`
- [ ] Rerun `Configure`.
- [ ] Review the log for any errors.  If any, resolve errors and reconfigure until no errors are reported.
- [ ] Click `Generate`.

## Set Up Libraries
Rename the following files:
- [ ] `$tahoma2d\thirdparty\tiff-4.2.0\libtiff\tif_config.vc.h` to
  - `$tahoma2d\thirdparty\tiff-4.2.0\libtiff\tif_config.h`
- [ ] `$tahoma2d\thirdparty\tiff-4.2.0\libtiff\tiffconf.vc.h` to
  - `$tahoma2d\thirdparty\tiff-4.2.0\libtiff\tiffconf.h`
- [ ] `$tahoma2d\thirdparty\libpng-1.6.21\scripts\pnglibconf.h.prebuilt` to
  - `$tahoma2d\thirdparty\libpng-1.6.21\pnglibconf.h` 
  - Note that the destination folder is different for this file.

## Library setup for stop motion with Canon DSLR cameras.
- You will need to get a copy of the Canon SDK.  This requires applying for the Canon developer program and downloading the SDK.

- [ ] Copy the following folders into the `$tahoma2d\thirdparty\canon` folder.
   - `Header`
   - `library`
     - Make sure that the library is the one from the `EDSDK_64` folder.

## Quick setup to build and run in Debug/RelWithDebInfo Mode
- `RelWithDebInfo` is recommended for developers because it allows you to debug with optimal performance.  Use `Debug` if you want `Asserts` to be triggered
- [ ] Obtain the latest Tahoma2D portable release from https://tahoma2d.org/ if you don't have it already
- [ ] Create the folder `$tahoma2d\toonz\build\Debug` or `RelWithDebInfo`
- [ ] Copy all files and subfolders from the latest release into the `Debug`/`RelWithDebInfo` folder, except `tahomastuff`, `ffmpeg` and `rhubarb`
- [ ] Copy the `$tahoma2d\stuff` folder to `$tahoma2d\toonz\build\toonz` and rename it `tahomastuff`
- [ ] Copy the `ffmpeg` and `rhubarb` folders from the release to `$tahoma2d\toonz\build\toonz`

## Build/Debug
- [ ] Start Visual Studio by double-clicking on`$tahoma2d\toonz\build\Tahoma2D.sln`.
- [ ] Change the Solution Configuration to `Debug`, `RelWithDebInfo` or, `Release` in the top bar.
- [ ] To build: `Build` -> `Build Solution`
- [ ] To debug:
  - [ ] In the Solution Explorer window, right click on `Tahoma2D` and choose `Set as StartUp Project`.
  - [ ]  `Debug` -> `Start Debugging`

## Build Release package
- [ ] Create a `Tahoma2D` folder somewhere (i.e `C:\Tahoma2D`)
- [ ] Copy the entire contents of `$tahoma2d\toonz\build\Release` or `RelWithDebInfo` to the `C:\Tahoma2D`
- [ ] Do **one** of:
    - (option 1) Open a Command Prompt and navigate to `QT_DIR\msvc2019_64\bin`. Run the Qt program `windeployqt.exe` `C:\Tahoma2D\Tahoma2D.exe` as an argument. 

     - (option 2) Another way to do this is to open two windows in Windows Explorer. In the first window navigate to the folder containing `windeployqt.exe`. In a second window navigate to and drag and drop `C:\Tahoma2D\Tahoma2D.exe` onto `windeployqt.exe` in the other window.

- [ ] Confirm the following files and folders have been added to `C:\Tahoma2D`
  - `Qt5Core.dll`
  - `Qt5Gui.dll`
  - `Qt5Multimedia.dll`
  - `Qt5Network.dll`
  - `Qt5OpenGL.dll`
  - `Qt5PrintSupport.dll`
  - `Qt5Script.dll`
  - `Qt5SerialPort.dll`
  - `Qt5Svg.dll`
  - `Qt5Widgets.dll`
  - `Qt5Xml.dll`
  - `audio\`
  - `bearer\`
  - `iconengines\`
  - `imageformats\`
  - `mediaservice\`
  - `platforms\`
  - `playlistformats\`
  - `printsupport\`
  - `translations\`
  - `concrt140.dll`
  - `msvcp140.dll`
  
- [ ] Copy the following files and folders to `C:\Tahoma2D`
  - `$tahoma2d\thirdparty\freeglut\bin\x64\freeglut.dll`
  - `$tahoma2d\thirdparty\glew\glew-1.9.0\bin\64bit\glew32.dll`
  - `$tahoma2d\thirdparty\libmypaint\dist\64\libiconv-2.dll`
  - `$tahoma2d\thirdparty\libmypaint\dist\64\libintl-8.dll`
  - `$tahoma2d\thirdparty\libmypaint\dist\64\libjson-c-2.dll`
  - `$tahoma2d\thirdparty\libmypaint\dist\64\libmypaint-1-4-0.dll`
  - `$tahoma2d\thirdparty\libgphoto2\bin\*`
  - `$OpenCV_DIR\build\x64\vc15\bin\opencv_world451.dll`
  
- [ ] Copy the following files and folders from a recent Tahoma2D release to `C:\Tahoma2D`
  - `EDSDK.dll` (if you compiled with Canon support)
  - `EdsImage.dll` (if you compiled with Canon support)
  - `ffmpeg\`
  - `rhubarb\`

- [ ] Create the stuff Folder
  - Copy the files from `$tahoma2d\stuff` to `C:\Tahoma2D` and rename it `tahomastuff`

### Run
`Tahoma2D.exe` can now be run.  Congratulations!

## Create Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
