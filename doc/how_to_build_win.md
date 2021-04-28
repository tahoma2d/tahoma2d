# Build Tahoma2D on Windows

This software can be built using Visual Studio 2015 or above and Qt 5.9 (later Qt versions still not working correctly.)

Throughout these instructions `$tahoma2d` represents the full path to your local Git repository for Tahoma2D.

## Required Software

### Visual Studio Express 2015 or higher for Windows Desktop
- [ ] Install Visual Studio Express 2015 or higher for Windows Desktop, https://www.visualstudio.com/vs/older-downloads/
  - Community and Professional versions of Visual Studio for Windows Desktop also work.
  - [ ] Make sure the target platform is "for Windows Desktop", not "for Windows".
  - [ ] During the installation, make sure to select all the Visual C++ packages.

### Qt - cross-platform GUI framework.
- [ ] Install via **one** of the following methods.
  - (option 1) Install Qt 5.9.9 from here: https://www.qt.io/download-open-source/. Click the `Download the Qt Online Installer` button at the bottom of the page. It should recommend the Windows installer, if not, select it. The installer is a small file and any additional needed files are downloaded during the install.
    - [ ] Run the the downloaded file. 
    - [ ] On the filter tick the Archive checkbox to see older verions in the list. In the list choose version `Qt 5.9.9`. 

  - (option 2) Install using the [Qt 5.9.9 Offline Installer](http://download.qt.io/official_releases/qt/5.9/5.9.9/qt-opensource-windows-x86-5.9.9.exe). It is a large installer file which can be run offline.

From the site: *"We recommend you use the Qt Online Installer for first time installations and the Qt Maintenance Tool for changes to a current install."*
The Maintenance Tool for Qt is in the Qt installation folder, `C:\Qt\MaintenanceTool.exe`,

### CMake
This will be used to create the `MSVC` project file.
- [ ] If CMake was installed with Qt, no additional installation is required.

  The typical location of CMake installed with Qt is: `C:\Qt\Tools\CMake_64\bin\cmake-gui.exe`
- [ ] If CMake was not installed then get it here and install it: https://cmake.org/download/

### boost
Boost 1.55.0 or later is required (tested up to 1.75.0).
- [ ] Download boost_1_75_0.zip from http://www.boost.org/users/history/version_1_75_0.html
- [ ] Extract all contents to the - '$tahoma2d\thirdparty\boost' directory. The result will be something like this: `$tahoma2d\thirdparty\boost\boost_1_75_0`

### OpenCV
- [ ] Install OpenCV. (https://opencv.org/) (v4.4.0 and later). 

  OpenCV version 4.4.0 is the version distributed with Tahoma2D 1.1.

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
- [ ] Launch CMake GUI. You can find it in this Qt subfolder: `C:\Qt\Tools\CMake_64\bin\cmake-gui.exe`, or wherever you installed it.
- [ ] In `Where is the source code`, navigate to `$tahoma2d/toonz/sources`
- [ ] In `Where to build the binaries`, navigate to `$tahoma2d/toonz/build` or to wherever you usually build to.
  - [ ] If the build directory is in the git repository, be sure to add the directory to .gitignore.
  - If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
- [ ] Click on Configure, a pop-up should appear the first time this is done.
- If no pop-up appears then a cached version from a prior run of CMake was found at the locations you selected above. 
  - [ ] Clear the cache using: File -> Delete Cache.
  - [ ] Click on Configure, the pop-up should now appear.
- In the pop-up that appears:
  - [ ] Select the version of Visual Studio you are using. 
  - [ ] Select the x64 Target Environment.
- If Qt was installed to a directory other than the default, and the error `Specify QT_PATH properly` appears:
  - Click on the value for `QT_PATH` in the top half of the CMake window.
  - [ ] navigate to the `QT_DIR` install folder and down to the subfolder that most closely matches your version of Visual Studio, for example: `C:\Qt\5.9.9\msvc2017_64` for Visual Studio 2017. 
  - [ ] Rerun Configure.
- If OpenCV was installed to a directory other than the default and a message about `set "OpenCV_DIR"` appears:
  - Click on the value for `OpenCV_DIR` in the top half of the CMake window.
  - [ ] navigate to the OpenCV install folder and down to the level of the `build` folder. Example: `C:\opencv_4.4.0\build`.
  - [ ] Rerun Configure.
- If red warning lines appear in the bottom box, you can safely ignore them.
- [ ] Click Generate.
- Should the CMakeLists.txt file change, such as during automatic build cleanup in Visual Studio, there is no need to rerun CMake.

## Set Up Libraries
Rename the following files:
- [ ] `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.vc` to
  - `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.h`
- [ ] `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` to
  - `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
- [ ] `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` to
  - `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
- [ ] `$tahoma2d/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt` to
  - `$tahoma2d/thirdparty/libpng-1.6.21/pnglibconf.h` 
  - Note that the destination folder is different for this file.

## Build
- [ ] Start Visual Studio and open the solution `$tahoma2d/toonz/build/Tahoma2D.sln`.
- [ ] Change to `Debug`, `RelWithDebInfo`, or `Release` in the top bar.
- [ ] Build the solution.
- [ ] Find the output in the folder `$tahoma2d/toonz/build/`

## Build with extended stop motion support for webcams and Canon DSLR cameras.
You will need two additional libraries.
 - [ ] Get [libjpeg-turbo](https://www.libjpeg-turbo.org/)
 - [ ] Get the Canon SDK.  This requires applying for the Canon developer program and downloading the SDK.

Copy the following folders into the `$tahoma2d/thirdparty` folder.
 - [ ] Copy the Header and library folders from the Canon SDK to `$tahoma2d/thirdparty/canon`
 - Make sure that the library is the one from the EDSDK_64 folder.
 - [ ] Copy the lib and include folders from libjpeg-turbo64 into `$tahoma2d/thirdparty/libjpeg-turbo64`.
 - [ ] Check the checkbox in CMake to build with stop motion support.

To run the program with stop motion support, you will need to copy the .dll files from opencv2, libjpeg-turbo and the Canon SDK into the folder where your project is built.

## Quick Setup and Run in Debug Mode - suitable for most people
- Use this method if you are interested in analyzing Tahoma2D rather than just creating a running copy.
- [ ] Start with a local working Tahoma2D installation. The latest release version is available here: https://tahoma2d.org/
  - [ ] Copy all files and subfolders from the working Tahoma2D folder, except `tahomastuff` and `ffmpeg`.
  - Paste to the build folder where the compiler generates output. 
    - `$tahoma2d/toonz/build/Debug` or `$tahoma2d/toonz/build/RelWithDebug` are typical build folders and the name is based on the `Solution Configuration` choice you make in Visual Studio during build.
  - If the `Debug` or `RelWithDebug` subfolder does not already exist then create that folder manually or skip this step and come back to it after doing at least one build to create one of those folders. The Tahoma2d dlls and exe in the folder will be replaced when the next build is run.
  - [ ] Copy the `tahomastuff` folder to `$tahoma2d/toonz/build/toonz` to get this result: `$tahoma2d/toonz/build/toonz/tahomastuff`.
- [ ] Start Visual Studio and load the solution.
- [ ] In Visual Studio set the `Solution Configuration` to `Debug` or `RelWithDebInfo` using the drop-down at the top of the screen.
- [ ] In the Solution Explorer window, right click on the `Tahoma2D` project.
  - [ ] In the pop-up context menu that appears, choose `Set as StartUp Project`.
  - This is a one time step. The `Tahoma2D` project will now show in bold indicating it is the startup project for the solution.
- [ ] Build the solution. 
- [ ] Click the `Local Windows Debugger` to start Tahoma2D in debug mode.
- Set breakpoints, checkpoints, view the output window, do step-through debugging.
- To stop the debugging session exit Tahoma2D from its menu or stop the debugger in Visual Studio.

## Run the Program - More Steps, Individual File Copying - more awareness over which files are used
1. - [ ] Copy the entire contents of $tahoma2d/toonz/build/Release to an appropriate folder.
2. - [ ] Do **one** of:
      - (option 1) Open a Command Prompt and navigate to `QT_DIR/msvc2015_64/bin`. Run the Qt program `windeployqt.exe` with the path for `Tahoma2D.exe` as an argument. 

      - (option 2) Another way to do this is to open two windows in Windows Explorer. In the first window navigate to the folder containing `windeployqt.exe`. In a second window navigate to the Release folder contining the Tahoma2D.exe you built. Drag and drop Tahoma2D.exe onto `windeployqt.exe` in the other window.
    - This will automatically generate the QT files and folders you will need.

3. Confirm the result.
- These necessary Qt library files should be in the same folder as `Tahoma2D.exe`
    - [ ] `Qt5Core.dll`
    - [ ] `Qt5Gui.dll`
    - [ ] `Qt5Multimedia.dll`
    - [ ] `Qt5Network.dll`
    - [ ] `Qt5OpenGL.dll`
    - [ ] `Qt5PrintSupport.dll`
    - [ ] `Qt5Script.dll`
    - [ ] `Qt5SerialPort.dll`
    - [ ] `Qt5Svg.dll`
    - [ ] `Qt5Widgets.dll`
    - [ ] `Qt5Xml.dll`
    - and these files should be in corresponding sub-folders
    - [ ] `/bearer/qgenericbearer.dll`
    - [ ] `/iconengines/qsvgicon.dll`
    - [ ] `/imageformats/qgif.dll`
    - [ ] `/imageformats/qicns.dll`
    - [ ] `/imageformats/qico.dll`
    - [ ] `/imageformats/qjpeg.dll`
    - [ ] `/imageformats/qsvg.dll`
    - [ ] `/imageformats/qtga.dll`
    - [ ] `/imageformats/qtiff.dll`
    - [ ] `/imageformats/qwbmp.dll`
    - [ ] `/imageformats/qwebp.dll`
    - [ ] `/platforms/qwindows.dll`

4. Copy the following files to the same folder as `Tahoma2D.exe`
  - [ ] `$tahoma2d/thirdparty/freeglut/bin/x64/freeglut.dll`
  - [ ] `$tahoma2d/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`
  - [ ] `$tahoma2d/thirdparty/libjpeg-turbo64/dist/turbojpeg.dll`
  - [ ] `$tahoma2d/thirdparty/libmypaint/dist/64/libjson-c-2.dll`
  - [ ] `$tahoma2d/thirdparty/libmypaint/dist/64/libmypaint-1-4-0.dll`
  - [ ] `$OpenCV_DIR/build/x64/vc15/bin/opencv_world440.dll` Use the same OpenCV that was used to build the solution.
  
5. Copy the following files from a downloaded release copy of Tahoma2D to the same folder as `Tahoma2D.exe`. The latest release version is available here: https://tahoma2d.org/.
  - [ ] `concrt140.dll`
  - [ ] `EDSDK.dll`
  - [ ] `EdsImage.dll`
  - [ ] `ffmpeg.exe`
  - [ ] `ffprobe.exe`
  - [ ] `libiconv-2.dll`
  - [ ] `libintl-8.dll`
  - [ ] `msvcp140.dll`
  - [ ] `vcruntime140.dll`
  - [ ] `vcruntime140_1.dll`

### Create the stuff Folder
- [ ] Create an empty `tahomastuff` folder inside the folder where `Tahoma2D.exe` is located.
- [ ] Copy the files from `$tahoma2d/stuff` to the new folder.

### Run
`Tahoma2D.exe` can now be run.  Congratulations!

## Create Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
