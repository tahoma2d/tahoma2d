# Setting up the development environment for Android

Building Tahoma2D for Android requires cross-compiling from a desktop host (Linux, macOS, or Windows).

## Required software

- **Android NDK**: (r21 or newer recommended)
- **Android SDK**: With platform tools and appropriate API levels (API 21+).
- **Qt for Android**: Qt 5.15.x or newer, installed via the Qt Online Installer or your package manager.
- **CMake**: 3.10 or newer.
- **Java Development Kit (JDK)**: JDK 8 or 11.

## Build instructions

### 1. Configure the environment

Ensure the following environment variables are set:
- `ANDROID_NDK_ROOT`: Path to your Android NDK.
- `ANDROID_SDK_ROOT`: Path to your Android SDK.
- `QT_PATH`: Path to your Qt for Android installation (e.g., `~/Qt/5.15.2/android`).

### 2. Building Dependencies

Many third-party dependencies (like LibPNG, Jpeg-Turbo, etc.) need to be cross-compiled for Android. It is recommended to use pre-built binaries for Android or include them in your CMake search path.

### 3. Configuring Tahoma2D with CMake

Create a build directory and run CMake using the Android toolchain:

```bash
$ mkdir build-android && cd build-android
$ cmake ../toonz/sources \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DANDROID=ON \
    -DCMAKE_PREFIX_PATH=$QT_PATH/lib/cmake \
    -DQt5_DIR=$QT_PATH/lib/cmake/Qt5
```

Adjust `ANDROID_ABI` to target other architectures (e.g., `armeabi-v7a`, `x86_64`).

### 4. Compiling

```bash
$ make -j$(nproc)
```

The output will be a shared library: `libTahoma2D.so`.

### 5. Packaging into an APK

The easiest way to package the application is to use **Qt Creator**:
1. Open `toonz/sources/CMakeLists.txt` in Qt Creator.
2. Configure the project using the **Android** kit.
3. Ensure `toonz/android/AndroidManifest.xml` is recognized.
4. Click **Run** or **Build APK**.

Alternatively, you can use the `androiddeployqt` tool provided with Qt:

```bash
$ $QT_PATH/bin/androiddeployqt \
    --input android-Tahoma2D-deployment-settings.json \
    --output android-build \
    --apk Tahoma2D.apk \
    --gradle
```

## Running on Android

Once the APK is installed on your device:
- The app will request storage permissions to access project files.
- Stylus support is enabled by default, providing pressure sensitivity and button handling.
- Professional pen features work best on tablets with active digitizers (like Samsung S-Pen or Wacom-powered devices).
