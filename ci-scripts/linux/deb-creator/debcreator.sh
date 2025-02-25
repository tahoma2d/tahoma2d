#!/bin/bash
set -e
# ======================================================================
# File:       debcreator.sh
# Author:     Charlie Martínez® <cmartinez@quirinux.org>
# License:    BSD 3-Clause "New" or "Revised" License
# Purpose:    Script create a functional .deb from .appimage
# Supported:  Debian and derivatives (originally for Quirinux 2.0)
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
# Parameters:
# -p or --package: Defines the package version (e.g., 1.5.1).
# -v or --version: Defines the program version (e.g., 1.5.1).
# -t or --template: Specifies the path to the deb-template directory.
# -c or --compressed: Provides the path to the compressed Tahoma2D archive (.tar.gz or .zip).
# -d or --deb: Specifies the output path for the .deb file (default is the script's directory).
# ======================================================================

# Default paths
DEFAULT_TEMPLATE_PATH="./deb-template"
DEFAULT_DEB_PATH="$(pwd)"

# ======================================================================
# Functions
# ======================================================================

function _sanitizePath() {
	# Ensure the template path does not end with '/'
	deb_template_path="${deb_template_path%/}"
}

function _setTemplatePath() {
	# Set template path
		if [ -z "$deb_template_path" ]; then
			if [ ! -d "$DEFAULT_TEMPLATE_PATH" ]; then
				read -p "Template path not found: " deb_template_path
			else
				deb_template_path="$(pwd)/$DEFAULT_TEMPLATE_PATH"
			fi
		fi
	echo "[INFO] Template path set to: $deb_template_path"
}

function _rename() {
	# Rename hidden folder
	if [ -e "$deb_template_path/etc/skel/local" ]; then
		mv "$deb_template_path/etc/skel/local" "$deb_template_path/etc/skel/.local"
	fi
}

function _updateVersion() {
# Update package version in DEBIAN/control
	control_file="$deb_template_path/DEBIAN/control"
	backup_file="$deb_template_path/DEBIAN/control.bak"

		if [ ! -f "$control_file" ]; then
			exit 1
		fi

	cp "$control_file" "$backup_file"

	current_version=$(grep -oP '(?<=Version: )\d+\.\d+\.\d+' "$control_file")
	IFS='.' read -r major minor patch <<< "$current_version"
	((patch++))
	version_number="$major.$minor.$patch"

	sed -i "s/Version: $current_version/Version: $version_number/" "$control_file"
	
	rm "$backup_file"
}

function _findCompressedFile() {
	# Find compressed file if not provided
		if [ -z "$compressed_file" ]; then
			compressed_file=$(find "$(pwd)" -maxdepth 1 -type f \( -name "*.tar.gz" -o -name "*.zip" \) | head -n 1)
			if [ -z "$compressed_file" ]; then
				read -p "Enter the path to the compressed file: " compressed_file
			fi
		fi
}

function _copyTemplate() {
	# Copy template to final folder in /tmp
	_sanitizePath
	final_folder="/tmp/tahoma2d_${version_number}_amd64"
	cp -rT "$deb_template_path" "$final_folder"
}

function _extract() {
# Extract compressed file
	temp_extract=$(mktemp -d)
		case "$compressed_file" in
			*.tar.gz) tar -xzf "$compressed_file" -C "$temp_extract" ;;
			*.zip) unzip "$compressed_file" -d "$temp_extract" ;;
		esac
	find "$temp_extract" -type f \( -name "*.tar.gz" -o -name "*.zip" \) | while read file; do
			case "$file" in
				*.tar.gz) tar -xzf "$file" -C "$temp_extract" ;;
				*.zip) unzip "$file" -d "$temp_extract" ;;
			esac
			rm "$file"
		done
}

function _extractAppImage() {
	appimage_file=$(find "$temp_extract" -type f -name "Tahoma2D.AppImage" | head -n 1)
		if [ -f "$appimage_file" ]; then
			chmod +x "$appimage_file"
			(cd /tmp && "$appimage_file" --appimage-extract)
			bin_folder="$final_folder/etc/skel/.local/share/Tahoma2D/"
			mkdir -p "$bin_folder"
			rsync -a --delete "/tmp/squashfs-root/usr/" "$bin_folder/usr/"
		fi
}

function _moveFiles() {
# Move extracted directories
		for folder in ffmpeg rhubarb tahomastuff; do
			src_folder=$(find "$temp_extract" -type d -name "$folder" | head -n 1)
			if [ -d "$src_folder" ]; then
				dest_folder="$final_folder/etc/skel/.local/share/Tahoma2D/$folder"
				rm -rf "$dest_folder"
				mv "$src_folder" "$dest_folder"
			fi
		done
}

function _generateMd5sums() {
# Generate md5sums
	pushd "$final_folder"
	find . -type f ! -path "./DEBIAN/*" -exec md5sum {} + > DEBIAN/md5sums
	popd
}

function _setPermissions() {
# Set permissions
	chmod 0775 "$final_folder/DEBIAN"
	chmod +x "$final_folder/DEBIAN/postinst"
	chmod +x "$final_folder/usr/local/bin/tahoma2d"
	chmod +x "$final_folder/usr/local/bin/tahoma-permissions"
	chmod +x "$final_folder/etc/skel/.local/share/Tahoma2D/AppRun"
}

function _createDeb() {
# Create Debian package
	output_deb="$deb_output_path/tahoma2d_${version_number}_amd64.deb"
	dpkg -b "$final_folder" "$output_deb" || exit 1
}

# ======================================================================
# Main Function
# ======================================================================
function _main() {
	deb_output_path="$DEFAULT_DEB_PATH"
		while [ "$#" -gt 0 ]; do
			case "$1" in
				-p|--package) package_version="$2"; shift 2 ;;
				-v|--version) program_version="$2"; shift 2 ;;
				-t|--template) deb_template_path="$2"; shift 2 ;;
				-c|--compressed) compressed_file="$2"; shift 2 ;;
				-d|--deb) deb_output_path="$2"; shift 2 ;;
				-h|--help)
					printf "Usage: ./debcreator.sh -p <package_version> -v <program_version> -t <template_path> -c <compressed_file> -d <output_deb_path>\n"
					exit 0
					;;
				*)
					printf "Invalid argument. Use -h for help.\n"
					exit 1
					;;
			esac
		done

	_setTemplatePath
	_rename
	_updateVersion
	_findCompressedFile
	_copyTemplate
	_extract
	_extractAppImage
	_moveFiles
	_generateMd5sums
	_setPermissions
	_createDeb
}

_main "$@"
