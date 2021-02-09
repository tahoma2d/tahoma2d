
# Build Tahoma2D on Windows

This software can be built using Visual Studio 2015 or above and Qt 5.9 (later Qt versions still not working correctly.)

## Required Software

### Visual Studio Express 2015 or higher for Windows Desktop
- [ ] Install Visual Studio Express 2015 or higher for Windows Desktop, https://www.visualstudio.com/vs/older-downloads/
- - [ ] Make sure the target platform is "for Windows Desktop", not "for Windows".
- - [ ] Community and Professional versions of Visual Studio for Windows Desktop also work.
- - [ ] During the installation, make sure to select all the Visual C++ packages.

### Qt - cross-platform GUI framework.
- [ ] Install via **one** of the following methods.
- - [ ] Install Qt from here: https://www.qt.io/download-open-source/
- **or**
- - [ ] Install Qt 5.9 (64-bit version, tested up to 5.9.9) with the [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

### CMake
- This will be used to create the `MSVC` project file.
- [ ] If CMake was installed with Qt, no additional installation is required. 
- - Typical location of the CMake installed with Qt: `C:\Qt\Tools\CMake_64\bin\cmake-gui.exe`
- [ ] If necessary, install CMake, https://cmake.org/download/

### boost
- Boost 1.55.0 or later is required (tested up to 1.75.0).
- [ ] Download boost_1_75_0.zip from http://www.boost.org/users/history/version_1_75_0.html
- [ ] Extract all contents to the - '$tahoma2d/thirdparty/boost' directory.
- [ ] If you use Boost 1.55.0 with Visual Studio 2013 then install the following patch (Make the changes listed in the patch file).
- - https://svn.boost.org/trac/boost/attachment/ticket/9369/vc12_fix_has_member_function_callable_with.patch

## Acquire the Source Code
- Note: You can also perform these next commands with GitHub Desktop https://desktop.github.com/.
- [ ] Fork the base repository to your own GitHub account.
- [ ] Clone the base repository to your local environment.
- :pushpin: Throughout these instructions `$tahoma2d` will represent the root for the base repository.
- Visual Studio cannot recognize UTF-8 without BOM (Byte Order Mark) source code properly. 
- - Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to also be treated as a comment by `Microsoft Visual Studio`.
- - In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
- - [ ] From a command line navigate to your local Git repository for Tahoma2D and execute: `git config core.safecrlf true`
- **or**
- Acquire a copy only, without expecting to upload any changes to GitHub. No GitHub Fork or Clone required.
- [ ] Click the Code button in GitHub and select `Download ZIP` to download a zip file.
- [ ] Expand to a folder in your local environment.

### lib and dll
- Perform these steps if you are using the Git command line to acquire the code.
- - If you used the GitHub Desktop UI or downloaded the zip to acquire the Source Code, the lib and dll files will already be downloaded and you can skip these steps.
- `lib` and `dll` files are tracked by [Git Large File Storage](https://git-lfs.github.com/).
- git-lfs is installed with Github Desktop.
- Download the 'lib' and 'dll' files
- - From Git command line execute: 
- - - [ ] `git clone`
- - - [ ] `git lfs pull`

### Use CMake to Create a Visual Studio Project
- [ ] Launch CMake GUI. You can find it in the Qt subfolder. C:\\Qt\\Tools\\CMake_64\\bin\\cmake-gui.exe
- [ ] In `Where is the source code`, navigate to `$tahoma2d/toonz/sources`
- [ ] In `Where to build the binaries`, navigate to `$tahoma2d/toonz/build` or to wherever you usually build to.
- [ ] If the build directory is in the git repository, be sure to add the directory to .gitignore
- [ ] If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
- [ ] Click on Configure.
- [ ] In the pop-up that appears:
- - :warning: No pop-up? Then a cached version from a prior run of CMake was found at the locations you selected above. 
- - - [ ] Clear the cache using: File -> Delete Cache.
- - - [ ] Click on Configure, the pop-up should now appear.
- - [ ] Select the version of Visual Studio you are using. 
- - [ ] Select the x64 Target Environment.
- If Qt was installed to a directory other than the default, and the error Specify QT_PATH properly appears:
- - [ ] navigate to the `QT_DIR` install folder and down to the subfolder that most closely matches your version of Visual Studio, for example: `C:\Qt\5.9.9\msvc2017_64` for Visual Studio 2017. 
- - [ ] Rerun Configure.
- If OpenCV was installed to a directory other than the default, and the error Specify OpenCV_DIR properly appears:
- - [ ] navigate to the OpenCV install folder and down to the level of the `build` folder. example: `C:\opencv_4.4.0\build`
- - [ ] Rerun Configure.
- If red warning lines appear in the bottom box, you can safely ignore them.
- [ ] Click Generate
- :bulb: Should the CMakeLists.txt file change, such as during automatic build cleanup in Visual Studio, there is no need to rerun CMake.

## Set Up Libraries
Rename the following files:
- [ ] `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.vc` to
- `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.h`
- [ ] `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` to
- `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
- [ ] `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h` to
- `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
- [ ] `$tahoma2d/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt` to
- `$tahoma2d/thirdparty/libpng-1.6.21/pnglibconf.h` :warning: Note that the destination is a different folder for this file.

## Build
- [ ] Start Visual Studio and open the solution `$tahoma2d/toonz/build/Tahoma2D.sln`.
- [ ] Change to `Debug` or `Release` in the top bar.
- [ ] Compile the build.
- [ ] Find the output in the folder `$tahoma2d/toonz/build/`

## Build with extended stop motion support for webcams and Canon DSLR cameras.
 You will need three additional libraries.
- [OpenCV](https://opencv.org/) (v4.1.0 and later)
  - [libjpeg-turbo](https://www.libjpeg-turbo.org/)
  - The Canon SDK.  This requires applying for the Canon developer program and downloading the SDK.

Copy the following folders into the `$tahoma2d/thirdparty` folder.
  - Copy the Header and library folders from the Canon SDK to `$tahoma2d/thirdparty/canon`
    - Make sure that the library is the one from the EDSDK_64 folder.
  - Copy the lib and include folders from libjpeg-turbo64 into `$tahoma2d/thirdparty/libjpeg-turbo64`.

Check the checkbox in CMake to build with stop motion support.
On configuring with CMake or in the environmental variables, specify `OpenCV_DIR` to the `build` folder in the install folder of OpenCV (like `C:/opencv/build`).

To run the program with stop motion support, you will need to copy the .dll files from opencv2, libjpeg-turbo and the Canon SDK into the folder where your project is built.

## Run the Program
### Set Up the Program's Path
1. - [ ] Copy the entire contents of $tahoma2d/toonz/build/Release to an appropriate folder.
2. - [ ] Open a Command Prompt and navigate to `QT_DIR/msvc2015_64/bin`. 
- - [ ] Run the Qt program `windeployqt.exe` with the path for `Tahoma2D.exe` as an argument. 
- OR
2. - [ ] Another way to do this is to use Windows Explorer to navigate to the exe that was created in your Release folder and drag and drop the Tahoma2D.exe on top of  `windeployqt.exe`
- - This will automatically generate the QT files and folders you will need.
3. - [ ] Confirm the result.
- These necessary Qt library files should be in the same folder as `Tahoma2D.exe`
    - [ ] `Qt5Core.dll`
    - [ ] `Qt5Gui.dll`
    - [ ] `Qt5Network.dll`
    - [ ] `Qt5OpenGL.dll`
    - [ ] `Qt5PrintSupport.dll`
    - [ ] `Qt5Script.dll`
    - [ ] `Qt5Svg.dll`
    - [ ] `Qt5Widgets.dll`
    - [ ] `Qt5Multimedia.dll`
  - and these files should be in corresponding sub-folders
    - [ ] `/bearer/qgenericbearer.dll`
    - [ ] `/bearer/qnativewifibearer.dll`
    - [ ] `/iconengines/qsvgicon.dll`
    - [ ] `/imageformats/qdds.dll`
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

### Create the stuff Folder
- [ ] Create a `tahomastuff` folder inside the folder where `Tahoma2D.exe` is located.
- [ ] Copy the files from `$tahoma2d/stuff` to the new folder.

### Run
`Tahoma2D.exe` can now be run.  Congratulations!

## Setup and Run in Debug Mode
- [ ] Start with a local working Tahoma2D installation.
- - [ ] Copy all files and subfolders from the working Tahoma2D folder, except `tahomastuff` and `ffmpeg`.
- - - Paste to the build folder where the compiler generates output.
- - - `$tahoma2d/toonz/build/Debug` or `$tahoma2d/toonz/build/RelWithDebug` are typical build folders and are based on the `Solution Configuration` choice you will make in the build option below.
- - - If the Debug or RelWithDebug subfolder does not already exist then create that folder manually or skip this step and come back to it after doing at least one build to create one of those folders.
- - - The Tahoma2d dlls and exe in those folders will be replaced when the next build is run.
- - [ ] Copy the `tahomastuff` folder to `$tahoma2d/toonz/build/toonz` to have this result: `$tahoma2d/toonz/build/toonz/tahomastuff`.
- [ ] Start Visual Studio and load the solution.
- [ ] In Visual Studio set the `Solution Configuration` to `Debug` or `RelWithDebInfo` using the drop-down at the top of the screen.
- [ ] In the Solution Explorer window, right click on the `Tahoma2D` project.
- - [ ] In the pop-up context menu that appears, choose `Set as StartUp Project`.
- - - This is a one time step. The `Tahoma2D` project will now show in bold indicating it is the startup project for the solution.
- [ ] Build the solution. 
- [ ] Click the `Local Windows Debugger` to start Tahoma2D in debug mode.
- Set breakpoints, checkpoints, view the output window, do typical debug stuff.
- To stop the debugging session exit Tahoma2D from its menu or stop the debugger in Visual Studio.

## Create Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
