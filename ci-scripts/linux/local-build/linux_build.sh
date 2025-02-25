#!/bin/bash

# ======================================================================
# File:       linux_build.sh
# Author:     Charlie Martínez® <cmartinez@quirinux.org>
# License:    BSD 3-Clause "New" or "Revised" License
# Purpose:    Script para compilar Tahoma2D
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

# ======================================================================
# Variables
# ======================================================================

	source "./assets/var"

# ======================================================================
# Translations
# ======================================================================

	source "./assets/lang_build"

# ======================================================================
# Functions
# ======================================================================

function _copyright() {
	# Display the application header.

	# Generate the backtitle correctly aligned to the left and right.
	local width=$(tput cols)  # Get the width of the terminal.
	local left_text="TAHOMA2D LOCAL BUILD"
	local right_text="Copyright (c) 2025 Charlie Martínez"

	# Define a safety margin to prevent the text from being cut off.
	local safe_width=$((width - 2))  # We subtract 2 for margins.
	local max_left_length=$((safe_width / 2))  # Maximum for the left text.
	local max_right_length=$((safe_width / 2)) # Maximum for the right text.

	# Shorten the texts if they are longer than their assigned space.
		if (( ${#left_text} > max_left_length )); then
			left_text="${left_text:0:max_left_length-1}…"
		fi
		if (( ${#right_text} > max_right_length )); then
			right_text="${right_text:0:max_right_length-1}…"
		fi

	# Calculate the padding between the texts.
		local padding=$((safe_width - ${#left_text} - ${#right_text}))

	# If the space is too small, force at least one space.
		if (( padding < 1 )); then
			padding=1
		fi

	# Build the line with manual alignment.
	printf "%s%*s%s" "$left_text" "$padding" "" "$right_text"
}

function _checkRoot() {
		# This script should NOT be run as root, but... you can.
		if [ "$EUID" -eq 0 ]; then
			echo -e "$(_msg WARNING_YOU_ARE_ROOT)"
			
			while true; do
				echo -e "$(_msg OPTION_PROMPT)"
				read -r choice
				
				case "$choice" in
					1) 
						echo -e "\n$(_msg CONTINUING_ROOT)\n"
						break
						;;
					2) 
						echo -e "\n$(_msg CANCEL)\n"
						exit 1
						;;
					*) 
						echo -e "\n$(_msg INVALID_OPTION_RETRY)\n"
						;;
				esac
			done
		fi
}

function _checkDepends() {
	# The dependencies must be installed first.
		if [ ! -e "$CHECKFILE_PACKAGES" ]; then
			echo -e "\n$(_msg DEPENDS)\n"
			exit 1
		fi
}

function _checkInternet() {
	# We need the internet
		if ! ping -c 1 google.com &> /dev/null; then
			dialog --title "$(_msg MAIN_MENU)" \
			--backtitle "$(_copyright)" \
			--msgbox "$(_msg INTERNET)" 10 60
			exit 1
		fi
}

function _cloningTahoma() {
	# Cloning from the path set in the ./assets/var file.
		while true; do
				if [ ! -d "$TAHOMA_DIR" ]; then
					OPTION=$(dialog --title "$(_msg NAVIGATE) ↑ ↓ ← → | $(_msg CONFIRM) <Enter>" \
									--backtitle "$(_copyright)" \
									--menu "$(_msg REPOSITORY_NOT_FOUND)" 15 50 4 \
									1 "$(_msg SEARCH_AGAIN)" \
									2 "$(_msg AUTOMATIC_CLONING)" \
									3 "$(_msg SPECIFY_DIRECTORY)" \
									3>&1 1>&2 2>&3)

		# If Cancel is pressed in the main menu, we exit the script.
				if [ -z "$OPTION" ]; then
					exit 1 
				fi

				case $OPTION in
					1) 
						echo "$(_msg RETRYING_SEARCH)"
						continue  
						;;
					2)
						branches=$(git ls-remote --heads "$REPO_URL" | awk -F'/' '{print $NF}')
						OPTIONS=()
						i=1
						for branch in $branches; do
							OPTIONS+=("$i" "$branch")
							((i++))
						done

							BRANCH_NUM=$(dialog --title "$(_msg NAVIGATE) ↑ ↓ ← → | $(_msg CONFIRM) <Enter>" \
						--backtitle "$(_copyright)" \
						--menu "Branches disponibles:" 15 60 20 "${OPTIONS[@]}" 3>&1 1>&2 2>&3)

						if [ -z "$BRANCH_NUM" ]; then
							continue  # If cancelled in this option, it returns to the menu.
						fi

						BRANCH=$(echo "$branches" | sed -n "${BRANCH_NUM}p")

						if git clone -b "$BRANCH" "$REPO_URL" "$SCRIPT_DIR/tahoma2d"; then
							export TAHOMA_DIR="$SCRIPT_DIR/tahoma2d"
							break
						else
							dialog --msgbox "$(_msg ERROR_CLONING_REPOSITORY)" 10 50
						fi
						;;
					3)
						USER_DIR=$(dialog --title "$(_msg NAVIGATE) ← → | $(_msg CONFIRM) <Enter>" \
						--backtitle "$(_copyright)" \
						--inputbox "$(_msg INTRO_PATH)" 10 50 3>&1 1>&2 2>&3)

						if [ -z "$USER_DIR" ]; then
							continue  # If cancelled in this option, it returns to the menu.
						fi

						USER_DIR=$(eval echo "$USER_DIR")

						if [ -d "$USER_DIR" ]; then
							export TAHOMA_DIR="$USER_DIR"
							break
						else
							dialog --title "$(_msg MAIN_MENU)" \
							--backtitle "$(_copyright)" \
							--msgbox "$(_msg ERROR_INVALID_PATH)" 10 50
						fi
						;;
					*) continue;;  # If an invalid option is selected, it returns to the menu.
				esac

				else
					return  # Exit the function instead of staying in an infinite loop.
				fi
		done
}


function _stuff() {
	# Copy tahoma2d stuff dir
	mkdir -p "$USER_CONFIG_DIR"
	[ -d "$STUFF_DIR" ] && cp -rf "$STUFF_DIR" "$USER_CONFIG_DIR/"
}

function _systemVar() {
	# Create tahoma SystemVar.ini file
	mkdir -p "$USER_CONFIG_DIR"
	cat << EOF > "$USER_CONFIG_DIR/SystemVar.ini"
[General]
TAHOMA2DROOT="$USER_CONFIG_DIR/stuff"
TAHOMA2DPROFILES="$USER_CONFIG_DIR/stuff/profiles"
TAHOMA2DCACHEROOT="$USER_CONFIG_DIR/stuff/cache"
EOF
}

function _libTiff() {
	# Build libTiff
	cd "$LIBTIFF_DIR" || exit 1
	./configure --with-pic --disable-jbig --disable-webp
	make -j$(nproc)
	cd "$TAHOMA_DIR" || exit 1
}

function _build() {
	# Build Tahoma2D
	[ ! -d "$SOURCES_DIR" ] && echo "$(_msg SOURCES)" && exit 1
	mkdir -p "$BUILD_DIR"
	cd "$BUILD_DIR" || exit 1

		if command -v dnf >/dev/null 2>&1 || command -v yum >/dev/null 2>&1; then
			cmake "$SOURCES_DIR" -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DSUPERLU_INCLUDE_DIR=/usr/include/SuperLU
		else
			cmake "$SOURCES_DIR" -DCMAKE_INSTALL_PREFIX="$BUILD_DIR"
		fi

    [ $? -eq 0 ] && make -j$(nproc)
}

function _run() {
	# Run Tahoma2D widhout install
	[ ! -d "$BIN_DIR" ] && echo "Error: $BIN_DIR no existe." && exit 1
	LD_LIBRARY_PATH="$BUILD_DIR/lib/tahoma2d:$LD_LIBRARY_PATH"
	cd "$BIN_DIR" || exit 1
	./Tahoma2D
}

function _install() {
	# Install Tahoma2d in /opt
	su -c "make install"
}

function _bye() {
	# End message
	dialog --title "$(_msg NAVIGATE) ← →  | $(_msg CONFIRM) <Enter>" \
	--backtitle "$(_copyright)" \
	--msgbox "$(_msg BYE)" 10 60
	exit 0
}

function _menu() {
	# End options
		if [ -e "$BIN_DIR" ]; then
			opPrincipal=$(dialog --title "$(_msg NAVIGATE) ← →  | $(_msg CONFIRM) <Enter>" \
			--backtitle "$(_copyright)" \
			--menu 16 65 8 \
			1 "$(_msg EXECUTE)" \
			2 "$(_msg INSTALL)" \
			3 "$(_msg EXIT)" \
			3>&1 1>&2 2>&3)
			case "$opPrincipal" in
				1) _run ;;
				2) _install ;;
				3) _bye ;;
			esac
		fi
}

function _warning() {
	# Display a warning message before compilation.
		dialog --title "$(_msg CONFIRM) <Enter>" \
			   --backtitle "$(_copyright)" \
			   --yesno "$(_msg WARNING)" 10 60    
		if [ $? -ne 0 ]; then
			_bye 
		else
			_libTiff
			_build  
		fi
}

# ======================================================================
# Functions execution
# ======================================================================

function _main() {
	_checkRoot	# This script should NOT be run as root.
	_checkInternet	# We need the internet
	_checkDepends	# The dependencies must be installed first.
	_cloningTahoma	# Cloning from the path set in the ./assets/var file.
	_warning	# Display a warning message before compilation.  
}

_main
