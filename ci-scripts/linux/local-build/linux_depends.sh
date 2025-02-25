#!/bin/bash

# ======================================================================
# File:       linux_depends.sh
# Author:     Charlie Martínez® <cmartinez@quirinux.org>
# License:    BSD 3-Clause "New" or "Revised" License
# Purpose:    Download dependences for compiling Tahoma2d
# Supported:  Multi-distro (originally for Quirinux 2.0)
# ======================================================================

#
# Copyright (c) 2019-2025 Charlie Martínez. All Rights Reserved.  
# License: BSD 3-Clause "New" or "Revised" License
# Authorized and unauthorized uses of the Quirinux trademark:  
# See https://www.quirinux.org/aviso-legal  
#
# Tahoma2D and OpenToonz are registered trademarks of their respective 
# owners, and any other third-party projects that may be mentioned or 
# affected by this code belong to their respective owners and follow 
# their respective licenses.
#

# Use: ./linux_depends.sh
#
# Arguments:
# -o | --opencv :	Compile OpenCV from sources
# -f | --ffmpeg :	Compile FFmpeg from sources
# -g | --gphoto :	Compile Gphoto2 from sources
# -m | --mypaint :	Compile LibMyPaint from sources
# -r | --rhubarb :	Add Rhubarb
#
# -c`| --clear : 
# 	Deletes check files and the local tahoma2d repository directory 
# 	(if it is at the same level as the script). |
#

# ======================================================================
# Variables
# ======================================================================

	source "./assets/var"
   
# ======================================================================
# Translations
# ======================================================================

	source "./assets/lang_depends"

# ======================================================================
# Functions
# ======================================================================

function _checkFolder() {
	# Create checkfiles dir if not exists
		if [ ! -e $CHECK_DIR ]; then
		mkdir -p $CHECK_DIR
		fi
}

function _ownership() {
		# If TAHOMA_DIR does not exist, exit the function
		if [ ! -d "$TAHOMA_DIR" ]; then
			return
		fi

	# Get the current owner of the folder
	OWNER=$(stat -c "%U" "$TAHOMA_DIR")

	# If the owner is not root, exit the function
		if [ "$OWNER" != "root" ]; then
			return
		fi

	# If the owner is root, show a warning and request user action
		while true; do
			echo -e "\n$(_msg TAHOMA2D_OWNED_BY_ROOT)"
			echo -e "$(_msg LINUX_BUILD_SH_NO_ROOT)"
			echo -e "$(_msg WOULD_YOU_LIKE_TO_ENTER_NORMAL_USERNAME)"
			echo -e "1) $(_msg ENTER_NORMAL_USER)\n2) $(_msg IGNORE_WARNING)"

			read -r OPTION

			# Validate that the input is a valid number
			if ! [[ "$OPTION" =~ ^[1-2]$ ]]; then
				echo -e "\n$(_msg INVALID_OPTION_TRY_AGAIN)"
				continue
			fi

			case "$OPTION" in
				1)
					while true; do
						echo -e "$(_msg ENTER_NORMAL_USER_NAME)"
						read -r USERNAME

						# Verify that the user entered something
						if [ -z "$USERNAME" ]; then
							echo -e "\n$(_msg USERNAME_CANNOT_BE_EMPTY)"
							continue
						fi

						# Change the folder ownership to the entered user
						chown -R "$USERNAME":"$USERNAME" "$TAHOMA_DIR"
						echo -e "\n$(_msg PROPERTY_CHANGED)"
						break
					done
					break
					;;
				2)
					echo -e "\n$(_msg IGNORED_WARNING)"
					break
					;;
			esac
		done
}

function _checkRoot() {	
	# This program need administrator privileges.
		if [ "$EUID" -ne 0  ]; then
			echo ''
			echo $(_msg ROOTCHECK)
			echo ''
			exit 1
		fi
}

function _checkInternet() {
	# We need the internet
		if ! ping -c 1 google.com &> /dev/null; then
			echo "$(_msg INTERNET)" 10 60
			exit 1
		fi
}

function _dependsDebian() {
	# Install dependencies for Debian-based systems (e.g., Ubuntu)
	# Based on: https://github.com/tahoma2d/tahoma2d/blob/master/doc/how_to_build_linux.md
	echo -e "\n\n"
		for paquetes in dialog libtiff-dev ffmpeg build-essential git cmake freeglut3-dev libboost-all-dev libegl1-mesa-dev \
			libfreetype6-dev libgles2-mesa-dev libglew-dev libglib2.0-dev libjpeg-dev libjpeg-turbo8-dev \
			libjson-c-dev liblz4-dev liblzma-dev liblzo2-dev libpng-dev libsuperlu-dev pkg-config \
			qt5-default qtbase5-dev libqt5svg5-dev qtscript5-dev qttools5-dev qttools5-dev-tools \
			libqt5opengl5-dev qtmultimedia5-dev qtwayland5 libqt5multimedia5-plugins libturbojpg0-dev \
			libqt5serialport5-dev libsuperlu-dev liblz4-dev liblzo-dev libmypaint-dev libglew-dev \
			freeglut3-dev qt59multimedia qt59script qt59serialport qt59svg qt59tools libopencv-dev \
			libgsl2 libopenblas-dev libmypaint-1.5-1 libmypaint libturbojpeg0-dev; do apt-get install -y $paquetes 
		done
	touch $CHECKFILE_PACKAGES

}

function _dependsFedora() {
	# Install dependencies for Fedora-based systems
	# Based on: https://github.com/tahoma2d/tahoma2d/blob/master/doc/how_to_build_linux.md
	echo -e "\n\n"
		for paquetes in dialog ffmpeg gcc gcc-c++ automake git cmake boost boost-devel SuperLU SuperLU-devel \
			lz4-devel lzma lzo-devel libjpeg-turbo-devel libGLEW glew-devel freeglut-devel freeglut \
			freetype-devel libpng-devel qt5-qtbase-devel qt5-qtsvg qt5-qtsvg-devel qt5-qtscript \
			qt5-qtscript-devel qt5-qttools qt5-qttools-devel qt5-qtmultimedia-devel blas blas-devel \
			json-c-deve libtool intltool make qt5-qtmultimedia libmypaint-devel libmypaint; do dnf install -y $paquetes
		done
	touch $CHECKFILE_PACKAGES
}

function _dependsArch() {
	# Install dependencies for Arch Linux-based systems
	# Based on: https://github.com/tahoma2d/tahoma2d/blob/master/doc/how_to_build_linux.md
	echo -e "\n\n"
	pacman -S base-devel git cmake libtiff boost boost-libs qt5-base qt5-svg qt5-script qt5-tools qt5-multimedia lz4 lzo libjpeg-turbo glew freeglut freetype2
	pacman -S blas cblas
	yaourt -S superlu libmypaint 
	touch $CHECKFILE_PACKAGES
}

function _dependsOpenSUSE() {
	# Install dependencies for openSUSE systems
	# Based on: https://github.com/tahoma2d/tahoma2d/blob/master/doc/how_to_build_linux.md
	echo -e "\n\n"
		for paquetes in dialog ffmpeg boost-devel cmake freeglut-devel freetype2-devel gcc-c++ glew-devel \
			libQt5OpenGL-devel libjpeg-devel liblz4-devel libpng16-compat-devel libqt5-linguist-devel \
			libqt5-qtbase-devel libqt5-qtmultimedia-devel libqt5-qtscript-devel libqt5-qtsvg-devel \
			libtiff-devel lzo-devel openblas-devel pkgconfig sed superlu-devel zlib-devel json-c-devel \
			libqt5-qtmultimedia libmypaint-devel libmypaint libjpeg62-turbo-devel; do zypper install -y
		done
	touch $CHECKFILE_PACKAGES
}

function _warning() {
	# Display warning before install packages
		while true; do
			# Show the message and prompt the user for input
			echo -e "\n$(_msg WARNING)\n"
			echo -e "1) $(_msg ACEPT)"
			echo -e "2) $(_msg CANCEL)\n"
			read -p "$(_msg OPTION):" option
			# Check the selected option
			case $option in
				1)
				return 0  # Exit the function with success
				;;
				2)
				echo -e "\n\n$(_msg EXIT_INSTALL)\n\n"
				exit 1  # Exit the script
				;;
			esac
		done
}

function _update() {
	# Detect the package manager and update the system
		if command -v apt-get &>/dev/null; then
			apt-get update
		elif command -v dnf &>/dev/null; then
			dnf update -y
		elif command -v yum &>/dev/null; then
			pacman -Syu
		else
			echo -e "\n\n$(_msg PACKAGE_GESTOR_NOT_FOUND)\n\n"
			exit 1
		fi   
}

function _depends() {

	# Detect and execute system's package manager
		if [ ! -e "$CHECKFILE_PACKAGES" ]; then
			_warning
			_update
			if command -v apt-get >/dev/null 2>&1; then
				_dependsDebian
			elif command -v dnf >/dev/null 2>&1; then
				_dependsFedora
			elif command -v pacman >/dev/null 2>&1; then
				_dependsArch
			elif command -v zypper >/dev/null 2>&1; then
				_dependsOpenSUSE
			else
				echo -e "\n\n$(_msg PACKAGE_GESTOR_NOT_FOUND)\n\n"
				exit 1
			fi
		fi
	touch "$CHECKFILE_PACKAGES" 
}

function _libOpencv() {
	# With -o | --opencv parameter
	_cloneIf

	cd "$THIRDPARTY_DIR"

	echo ">>> $(_msg CLONING_libOpencv)"
	git clone https://github.com/tahoma2d/opencv || true

	cd opencv
	echo "*" >| .gitignore

	[ ! -d build ] && mkdir build
	cd build

	echo ">>> $(_msg CMAKING_libOpencv)"
echo ">>> Cmaking openv"
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_JASPER=OFF \
      -DBUILD_JPEG=OFF \
      -DBUILD_OPENEXR=OFF \
      -DBUILD_PERF_TESTS=OFF \
      -DBUILD_PNG=OFF \
      -DBUILD_PROTOBUF=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_TIFF=OFF \
      -DBUILD_WEBP=OFF \
      -DBUILD_ZLIB=OFF \
      -DBUILD_opencv_hdf=OFF \
      -DBUILD_opencv_java=OFF \
      -DBUILD_opencv_text=ON \
      -DOPENCV_ENABLE_NONFREE=ON \
      -DOPENCV_GENERATE_PKGCONFIG=ON \
      -DPROTOBUF_UPDATE_FILES=ON \
      -DWITH_1394=OFF \
      -DWITH_CUDA=OFF \
      -DWITH_EIGEN=ON \
      -DWITH_FFMPEG=OFF \
      -DWITH_GPHOTO2=OFF \
      -DWITH_GSTREAMER=ON \
      -DWITH_JASPER=OFF \
      -DWITH_OPENEXR=ON \
      -DWITH_OPENGL=OFF \
      -DWITH_QT=OFF \
      -DWITH_TBB=OFF \
      -DWITH_VTK=ON \
      -DBUILD_opencv_python2=OFF \
      -DBUILD_opencv_python3=ON \
      -DCMAKE_INSTALL_NAME_DIR=/usr/local/lib \
		..

	echo ">>> $(_msg BUILDING_libOpencv)"
	make -j$(nproc) || { echo "$(_msg OPENCV_BUILD_ERROR)"; exit 1; }

	echo ">>> $(_msg INSTALLING_libOpencv)"
	make install || { echo "$(_msg OPENCV_BUILD_ERROR)"; exit 1; }

	cd "$SCRIPT_DIR"
	touch "$CHECKFILE_OPENCV"
}

function _libFFmpeg(){
	# With -f | --ffmpeg parameter 
	_cloneIf

	cd "$THIRDPARTY_DIR"

	echo ">>> $(_msg CLONING_OPENH264)"
	git clone https://github.com/cisco/openh264.git openh264 || true

	cd openh264
	echo "*" >| .gitignore

	echo ">>> $(_msg MAKING_OPENH264)"
	make || { echo "$(_msg FFMPEG_BUILD_ERROR)"; exit 1; }

	echo ">>> $(_msg INSTALLING_OPENH264)"
	make install || { echo "$(_msg FFMPEG_BUILD_ERROR)"; exit 1; }

	cd ..

	echo ">>> $(_msg CLONING_libFFmpeg)"
	git clone -b v4.3.1 https://github.com/tahoma2d/FFmpeg ffmpeg

	cd ffmpeg
	echo "*" >| .gitignore

	echo ">>> $(_msg CONFIGURING_TO_BUILD_FFMPEG_SHARED)"
	
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

./configure  --prefix=/usr/local \
	  --cc="$CC" \
	  --cxx="$CXX" \
	  --toolchain=hardened \
	  --pkg-config-flags="--static" \
	  --extra-cflags="-I/usr/local/include" \
	  --extra-ldflags="-L/usr/local/lib" \
	  --enable-pthreads \
	  --enable-version3 \
	  --enable-avresample \
	  --enable-gnutls \
	  --enable-libbluray \
	  --enable-libmp3lame \
	  --enable-libopus \
	  --enable-libsnappy \
	  --enable-libtheora \
	  --enable-libvorbis \
	  --enable-libvpx \
	  --enable-libwebp \
	  --enable-libxml2 \
	  --enable-lzma \
	  --enable-libfreetype \
	  --enable-libass \
	  --enable-libopencore-amrnb \
	  --enable-libopencore-amrwb \
	  --enable-libopenjpeg \
	  --enable-libspeex \
	  --enable-libsoxr \
	  --enable-libopenh264 \
	  --enable-shared \
	  --disable-static \
	  --disable-libjack \
	  --disable-indev=jack

	echo ">>> $(_msg BUILDING_libFFmpeg_SHARED)"
	make -j$(nproc) || { echo "$(_msg FFMPEG_BUILD_ERROR)"; exit 1; }

	echo ">>> $(_msg INSTALLING_libFFmpeg_SHARED)"
	make install || { echo "$(_msg FFMPEG_BUILD_ERROR)"; exit 1; }

	ldconfig

	cd "$SCRIPT_DIR"
	touch "$CHECKFILE_FFMPEG"
}

function _libGphoto2() {
	# With -g | --gphoto parameter 
	_cloneIf

	cd "$THIRDPARTY_DIR"
	echo ">>> $(_msg CLONING_libGphoto2)"
	git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src

	cd libgphoto2_src

	git checkout tahoma2d-version

	echo ">>> $(_msg CONFIGURING_libGphoto2)"
	autoreconf --install --symlink

	./configure --prefix=/usr/local

	echo ">>> $(_msg MAKING_libGphoto2)"
	make || { echo "$(_msg GPHOTO_BUILD_ERROR)"; exit 1; }

	echo ">>> $(_msg INSTALLING_libGphoto2)"
	  make install || { echo "$(_msg GPHOTO_BUILD_ERROR)"; exit 1; }

	cd "$SCRIPT_DIR"
	touch "$CHECKFILE_GPHOTO"
}

function _libMyPaint() {
	# With -m | --mpaint parameter 
	_cloneIf

	cd "$THIRDPARTY_DIR"

	cd libmypaint

	echo ">>> $(_msg CLONING_LIBMYPAINT)"
	git clone https://github.com/tahoma2d/libmypaint src

	cd src
	echo "*" >| .gitignore

	export CFLAGS='-Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations'

	echo ">>> $(_msg GENERATING_LIBMYPAINT_ENVIRONMENT)"
	./autogen.sh

	echo ">>> $(_msg CONFIGURING_LIBMYPAINT_BUILD)"
	./configure

	echo ">>> $(_msg BUILDING_LIBMYPAINT)"
	make || { echo "$(_msg MPAINT_BUILD_ERROR)"; exit 1; }

	echo ">>> $(_msg INSTALLING_LIBMYPAINT)"
	make install || { echo "$(_msg MPAINT_BUILD_ERROR)"; exit 1; }

	ldconfig

}

function _libRhubarb() {
	# With -r | --rhubarb parameter 
	_cloneIf

	cd "$THIRDPARTY_DIR"

		if [ ! -d apps ]
		then
		   mkdir apps
		fi
		cd apps
		echo "*" >| .gitignore

	echo ">>> Getting FFmpeg"
		if [ -d ffmpeg ]
		then
		   rm -rf ffmpeg
		fi
	wget https://github.com/tahoma2d/FFmpeg/releases/download/v5.0.0/ffmpeg-5.0.0-linux64-static-lgpl.zip
	unzip ffmpeg-5.0.0-linux64-static-lgpl.zip 
	mv ffmpeg-5.0.0-linux64-static-lgpl ffmpeg


	echo ">>> Getting Rhubarb Lip Sync"
		if [ -d rhubarb ]
		then
		   rm -rf rhubarb ]
		fi
	wget https://github.com/tahoma2d/rhubarb-lip-sync/releases/download/v1.13.0/rhubarb-lip-sync-tahoma2d-linux.zip
	unzip rhubarb-lip-sync-tahoma2d-linux.zip -d rhubarb
	
}

function _cloneIf() {
	# Clone the repository when you need it to compile libraries.
		if [ ! -d "$THIRDPARTY_DIR" ]; then
			# Show message and instructions
			echo -e "\n$(_msg REPOSITORY_NEEDS_CLONING)"

			# Ask the user if they want to continue
			echo -e "\n$(_msg CONTINUE_REPOSITORY_CLONING)"
			select option in "$(_msg ACEPT)" "$(_msg CANCEL)"; do
				case $option in
					"$(_msg ACEPT)")
						echo "$(_msg CLONING_REPOSITORY)"
						git clone $REPO_URL
						cd "$THIRDPARTY_DIR"
						
						# List the available branches in the repository
						echo -e "\n$(_msg LISTING_AVAILABLE_BRANCHE)"
						git fetch --all
						branches=$(git branch -r | grep -v '\->' | sed 's/origin\///' )
						
						# Display the branches to the user with option to exit
						echo -e "$(_msg SELECT_BRANCH_OR_EXIT)"
						select branch in $branches "$(_msg EXIT)"; do
							if [[ "$branch" == "$(_msg EXIT)" ]]; then
								echo "$(_msg EXITING_CLONING_PROCESS)"
								exit 1
							elif [[ -n "$branch" ]]; then
								echo "$(_msg SELECTED_BRANCH)"
								git checkout "$branch"
								break
							else
								echo "$(_msg INVALID_SELECTION_TRY_AGAIN)"
							fi
						done
						break
						;;
					"$(_msg CANCEL)")
						echo "$(_msg EXIT_INSTALL)"
						exit 1
						;;
					*)
						echo "$(_msg INVALID_OPTION_TRY_AGAIN)"
						;;
				esac
			done
		fi
}

function _clear() {
	# Delete the checkfiles and the tahoma2d folder 
	# (if it is at the same level as the script).
		if [[ "$TAHOMA_DIR" == "$SCRIPT_DIR/tahoma2d" ]]; then
			echo -e "\n$(_msg CLEAR_DETECTED) "
			echo -e "$(_msg CLEAR_WARNING)"
			echo -e "\n$(_msg PROCEED_WITH_CLEAR)"
			echo "1) $(_msg ACEPT)"
			echo "2) $(_msg CANCEL)"
			read -p "$(_msg OPTION): " choice

		if [[ "$choice" -ne 1 ]]; then
			echo "$(_msg EXIT_INSTALL)"
			exit 1
		fi

			echo -e "\n$(_msg DELETING_TAHOMA_DIR)"
			rm -rf "$TAHOMA_DIR" # Only if it is at the same level as the script.
			rm -rf  "$CHECK_DIR"/*
			echo -e "$(_msg CLEAR_COMPLETED)"
		else
			echo -e "\n$(_msg CLEAR_NOT_ALLOWED_HERE)"
			exit 1
		fi
}

function _arguments() {
	# Parse arguments
	selected_libraries=()
	install_dependencies_only=true
	reset_requested=false

	# First, check if the --reset parameter is present and execute it.
		for arg in "$@"; do
			if [[ "$arg" == "-c" || "$arg" == "--clear" ]]; then
				_clear
			fi
		done

		if [ $# -ne 0 ]; then
			# If there are parameters, run _depends before processing them.
			_depends
			install_dependencies_only=false

			# Process the parameters.
			for arg in "$@"; do
				case $arg in
					-o|--opencv)
						if [ -e "$CHECKFILE_OPENCV" ]; then
							echo -e "\nOpenCV $(_msg ALREADY_COMPILED)"
						else
							selected_libraries+=("OpenCV")
						fi
						;;
					-f|--ffmpeg)
						if [ -e "$HECKFILE_LIBFFMPEG" ]; then
							echo -e "\nFFmpeg $(_msg ALREADY_COMPILED)"
						else
							selected_libraries+=("FFmpeg")
						fi
						;;
					-g|--gphoto)
						if [ -e "$CHECKFILE_GPHOTO" ]; then
							echo -e "\nGPhoto $(_msg ALREADY_COMPILED)"
						else
							selected_libraries+=("GPhoto")
						fi
						;;
					-m|--mypaint)
						if [ -e "$CHECKFILE_MYPAINT" ]; then
							echo -e "\nMyPaint $(_msg ALREADY_COMPILED)"
						else
							selected_libraries+=("MyPaint")
						fi
						;;
					-r|--rhubarb)
						if [ -e "$CHECKFILE_RHUBARB" ]; then
							echo -e "\nRhubarb $(_msg ALREADY_COMPILED)"
						else
							selected_libraries+=("Rhubarb")
						fi
						;;
					-c|--clear)
						# It was already processed in the first pass.
						;;
					*)
						echo -e "\n$(_msg USAGE)\n"
						exit 1
						;;
				esac
			done
		fi

		# Confirm compilation if there are selected libraries.
		if ! $install_dependencies_only; then
			echo -e "\n$(_msg LIBRARIES_TO_COMPILE)"
			for param in "${selected_libraries[@]}"; do
				echo "- $param"
			done

			echo -e "\n$(_msg CONTINUE_COMPILATION)"
			echo "1) $(_msg ACEPT)"
			echo "2) $(_msg CANCEL)"
			read -p "$(_msg OPTION): " choice
			
			if [[ "$choice" -ne 1 ]]; then
				echo -e "\n$(_msg EXIT_INSTALL)\n"
				exit 1
			fi
		fi

		# Clone the repository if necessary.
		if ! $install_dependencies_only; then
			_cloneIf
		fi

		# Compile the selected libraries.
		for param in "${selected_libraries[@]}"; do
			case $param in
				"OpenCV")
					echo -e "\n$(_msg PARAMETER_O_libOpencv_SELECTED)\n"
					_libOpencv
					;;
				"FFmpeg")
					echo -e "\n$(_msg PARAMETER_F_libFFmpeg_SELECTED)\n"
					_libFFmpeg
					;;
				"GPhoto")
					echo -e "\n$(_msg PARAMETER_libGphoto2_SELECTED)\n"
					_libGphoto2
					;;
				"MyPaint")
					echo -e "\n$(_msg PARAMETER_MPAINT_SELECTED)\n"
					_libMyPaint
					;;
				"Rhubarb")
					echo -e "\n$(_msg PARAMETER_RHUBARB_SELECTED)\n"
					_libMyPaint
					;;
			esac
		done
}

function _end() {
	echo -e "\n\n$(_msg OK_DEPENDS)\n\n"
}

function _main() {
	_checkRoot # Check for administrator privileges.
	_checkInternet # Check the internet connection.
	_checkFolder # Check folder for filechecks :-D
	_arguments "$@" # Process arguments.

		# If there were no parameters, run _depends.
		if [ $# -eq 0 ] && [ -z "$SKIP_UPDATE_DEPENDS" ]; then
			_depends # Install dependencies (packages from the distribution repository).
		fi

	_ownership # Option to modify the owner of the tahoma2d folder if it belongs to root.
	_end # Farewell message.
}

_main "$@"
