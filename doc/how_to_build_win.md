
# Building on Windows

This software can be built using Visual Studio 2015 or above and Qt 5.9 (later Qt versions still not working correctly.)

## Required Software

### Visual Studio Express 2015 or higher for Windows Desktop
- https://www.visualstudio.com/vs/older-downloads/
- Make sure the target platform is "for Windows Desktop", not "for Windows".
- Community and Professional versions of Visual Studio for Windows Desktop also work.
- During the installation, make sure to select all the Visual C++ packages.

### CMake
- https://cmake.org/download/
- This will be used to create the `MSVC 2015` project file.

### Qt
- https://www.qt.io/download-open-source/
- Qt is a cross-platform GUI framework.
- Install Qt 5.9 (64-bit version, tested up to 5.9.9) with the [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

### boost
- Boost 1.55.0 or later is required (tested up to 1.61.0).
- http://www.boost.org/users/history/version_1_61_0.html
- Download boost_1_73_0.zip from the above link. Extract all contents to the - '$tahoma2d/thirdparty/boost' directory.
- Install the following patch (Make the changes listed in the patch file), if you use Boost 1.55.0 with Visual Studio 2013.
- https://svn.boost.org/trac/boost/attachment/ticket/9369/vc12_fix_has_member_function_callable_with.patch

## Acquiring the Source Code
- Note: You can also perform these next commands with Github for Desktop client.
- Clone the base repository.
- Throughout the explanation `$tahoma2d` will represent the root for the base repository.

### lib and dll

- `lib` and `dll` files are tracked by [Git Large File Storage](https://git-lfs.github.com/).
- Note: git-lfs is also installed with Github Desktop.
- Visual Studio cannot recognize UTF-8 without BOM source code properly. Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to be treated as a comment by `MSVS` as well.
- In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
- `git config core.safecrlf true`
- Execute `git lfs pull` after `git clone` by using the lfs client.

### Using CMake to Create a Visual Studio Project
- Launch CMake
- In `Where is the source code`, navigate to `$tahoma2d/toonz/sources`
- In `Where to build the binaries`, navigate to `$tahoma2d/toonz/build`
- Or to wherever you usually build to.
- If the build directory is in the git repository, be sure to add the directory to .gitignore
- If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
-Click on Configure and select the version of Visual Studio you are using.
-If Qt was installed to a directory other than the default, and the error Specify QT_PATH properly appears, navigate to the `QT_DIR` install folder and specify the path to `msvc2015_64`. Rerun Configure.
-If red lines appear in the bottom box, you can safely ignore them.
-Click Generate
-Should the CMakeLists.txt file change, such as during automatic build cleanup, there is no need to rerun CMake.

## Setting Up Libraries
Rename the following files:
- `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.vc` to `$tahoma2d/thirdparty/LibJPEG/jpeg-9/jconfig.h`
- `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.vc.h` to `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tif_config.h`
- `$tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.vc.h to $tahoma2d/thirdparty/tiff-4.0.3/libtiff/tiffconf.h`
- `$tahoma2d/thirdparty/libpng-1.6.21/scripts/pnglibconf.h.prebuilt to $tahoma2d/thirdparty/libpng-1.6.21/pnglibconf.h`
- Note that the destination is a different folder for the last file.

## Building
- Open `$tahoma2d/toonz/build/Tahoma2D.sln` and change to `Debug` or `Release` in the top bar.
- Compile the build.
- The output will be in the corresponding folder in `$tahoma2d/toonz/build/`

## Building with extended stop motion support for webcams and Canon DSLR cameras.
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

## Running the Program
### Setting Up the Program's Path
1. Copy the entire contents of $tahoma2d/toonz/build/Release to an appropriate folder.

2. Open a Command Prompt and navigate to `QT_DIR/msvc2015_64/bin`. Run the Qt program `windeployqt.exe` with the path for `Tahoma2D.exe` as an argument. (Another way to do this is navigate to the exe that was created in your Release folder and drag and drop the Tahoma2D.exe on top of the windeployqt.exe This will automatically generate the QT files and folders you will need.)
 - The necessary Qt library files should be in the same folder as `Tahoma2D.exe`
  - These include:
    - `Qt5Core.dll`
    - `Qt5Gui.dll`
    - `Qt5Network.dll`
    - `Qt5OpenGL.dll`
    - `Qt5PrintSupport.dll`
    - `Qt5Script.dll`
    - `Qt5Svg.dll`
    - `Qt5Widgets.dll`
    - `Qt5Multimedia.dll`
  - These files should be in the corresponding folders in the same folder `Tahoma2D.exe`
    - `/bearer/qgenericbearer.dll`
    - `/bearer/qnativewifibearer.dll`
    - `/iconengines/qsvgicon.dll`
    - `/imageformats/qdds.dll`
    - `/imageformats/qgif.dll`
    - `/imageformats/qicns.dll`
    - `/imageformats/qico.dll`
    - `/imageformats/qjpeg.dll`
    - `/imageformats/qsvg.dll`
    - `/imageformats/qtga.dll`
    - `/imageformats/qtiff.dll`
    - `/imageformats/qwbmp.dll`
    - `/imageformats/qwebp.dll`
    - `/platforms/qwindows.dll`

3. Copy the following files to the same folder as `Tahoma2D.exe`
  - `$tahoma2d/thirdparty/freeglut/bin/x64/freeglut.dll`
  - `$tahoma2d/thirdparty/glew/glew-1.9.0/bin/64bit/glew32.dll`

### Creating the stuff Folder
1. Create a `tahomastuff` folder inside the folder where `Tahoma2D.exe` is located.
1. Copy the files from `$tahoma2d/stuff` to the new folder.

### Running
`Tahoma2D.exe` can now be run.  Congratulations!

## Debugging
1. In the Solution Explorer, select the Tahoma2D project within the Tahoma2D solution.
2. In the Project menu, choose Set as StartUp Project.

## Creating Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド」) is used.  These files are not created in the default `Build Project Solution`.
