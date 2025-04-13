#!/bin/bash
set -e
# ======================================================================
# File:       debcreator.sh --> QUIRINUX TWEAKS VERSION
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
# -c or --compressed: Provides the path to the compressed Tahoma2D archive (.tar.gz or .zip). Cannot be used with -x
# -x or --appimage-extract: Provides the path to the extracted appimage. Cannot be used with -c.
# -s or --stuff: Provides the path to the stuff folder. Used with -x
# -f or --ffmpag: Provides the path to the ffmpeg folder. Used with -x
# -r or --rhubarb: Provides the path to the rhubarb folder. Used with -x
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
            echo "Template path not found: " $DEFAULT_TEMPLATE_PATH
            exit 1
        else
            deb_template_path="$(pwd)/$DEFAULT_TEMPLATE_PATH"
        fi
    fi
    echo "[INFO] Template path set to: $deb_template_path"
}

function _updateVersion() {
    # Update package version and program version in DEBIAN/control
    control_file="$deb_template_path/DEBIAN/control"
    backup_file="/tmp/control.bak"

    if [ ! -f "$control_file" ]; then
        echo "[ERROR] File $control_file not found"
        exit 1
    fi

    cp "$control_file" "$backup_file"

    # Get current package version
    current_package_version=$(grep -oP '(?<=Version: )\d+(\.\d+)*' "$control_file")
    if [ -z "$package_version" ]; then
        # No -p provided, increment last number in package version
        IFS='.' read -r -a version_parts <<< "$current_package_version"
        ((version_parts[-1]++)) # Increment last digit
        package_version="${version_parts[*]}"
        package_version="${package_version// /"."}" # Convert array back to version format
    fi

    # Get current program version from "Description: Tahoma 2D Version ..."
    current_program_version=$(grep -oP '(?<=Description: Tahoma 2D Version )\d+(\.\d+)*' "$control_file")
    if [ -z "$program_version" ]; then
        # No -v provided, keep current program version
        program_version="$current_program_version"
    fi

    # Update Version: and Description: lines
    sed -i "s/Version: .*$/Version: $package_version/" "$control_file"
    sed -i "s/Description: .*$/Description: Tahoma 2D Version $program_version/" "$control_file"

    rm "$backup_file"
    echo "[INFO] Package version: $package_version"
    echo "[INFO] Program version: $program_version"
}

function _findCompressedFile() {
    # Skip finding compressed file if -x was given
    if [ ! -z "$appimage_extract_path" ]; then
        return
    fi

    # Find compressed file if not provided
    if [ -z "$compressed_file" ]; then
        compressed_file=$(find "$(pwd)" -maxdepth 1 -type f \( -name "Tahoma2D.AppImage" -o -name "Tahoma2D*.tar.gz" -o -name "Tahoma2D*.zip" \) | head -n 1)
        if [ ! -f "$compressed_file" ]; then
            echo "Compressed file not found: " compressed_file
            exit 1
        fi
    fi
    echo "[INFO] Compressed file: $compressed_file"
}

function _copyTemplate() {
    # Copy template to final folder in /tmp
    _sanitizePath
    final_folder="/tmp/tahoma2d_${version_number}_amd64"
    cp -rT "$deb_template_path" "$final_folder"
}

function _extract() {
    # Skip uncompressing file if -x was given
    if [ ! -z "$appimage_extract_path" ]; then
        return
    fi

    # Extract compressed file
    case "$compressed_file" in
        *.AppImage) temp_extract=$(dirname $(realpath "$compressed_file")) ;;
        *.tar.gz)  temp_extract=$(mktemp -d)
                   tar -xzf "$compressed_file" -C "$temp_extract" ;;
        *.zip)     temp_extract=$(mktemp -d)
                   unzip "$compressed_file" -d "$temp_extract" ;;
    esac
    find "$temp_extract" -type f \( -name "*.tar.gz" -o -name "*.zip" \) | while read file; do
        case "$file" in
            *.tar.gz) tar -xzf "$file" -C "$temp_extract" ;;
            *.zip) unzip "$file" -d "$temp_extract" ;;
        esac
        rm "$file"
    done
    echo "[INFO] Temp extract: $temp_extract"
}

function _extractAppImage() {
    bin_folder="$final_folder/etc/skel/.local/share/Tahoma2D/"
    mkdir -p "$bin_folder"

    # Skip appimage extract if -x was given
    if [ ! -z "$appimage_extract_path" ]; then
        if [ ! -d "$appimage_extract_path" ]; then
            echo "[ERROR] AppImage-Extract Path not found: $appimage_extract_path"
            exit 1
        fi
    else
        appimage_file=$(find "$temp_extract" -type f -name "Tahoma2D.AppImage" | head -n 1)
        echo "[INFO] AppImage file: $appimage_file"
        if [ -f "$appimage_file" ]; then
            chmod +x "$appimage_file"
            (cd $temp_extract && "$appimage_file" --appimage-extract)
            appimage_extract_path="$temp_extract/squashfs-root"
        fi    
    fi

    echo "[INFO] AppImage-Extract Path: $appimage_extract_path"
    rsync -a --delete "$appimage_extract_path/usr/" "$bin_folder/usr/"
}

function _moveFiles() {
    dest_folder="$final_folder/etc/skel/.local/share/Tahoma2D"
    if [ -z "$compressed_file" ]; then
        if [ -d "$stuff_path" ]; then
            echo "[INFO] Copying $stuff_path"
            cp -r "$stuff_path" "$dest_folder/tahomastuff"
            find "$dest_folder/tahomastuff" -name .gitkeep -exec rm -f {} \;
        else
            echo "[ERROR] Could not find stuff folder: $stuff_path"
            exit 1
        fi
        if [ -d "$ffmpeg_path" ]; then
            echo "[INFO] Copying $ffmpeg_path"
            cp -r "$ffmpeg_path" "$dest_folder"
        else
            echo "[WARNING] Could not find ffmepg folder: $ffmpeg_path"
        fi
        if [ -d "$rhubarb_path" ]; then
            echo "[INFO] Copying $rhubarb_path"
            cp -r "$rhubarb_path" "$dest_folder"
        else
            echo "[WARNING] Could not find rhubarb folder: $rhubarb_path"
        fi
    else
        # Move extracted directories
        for folder in ffmpeg rhubarb tahomastuff; do 
            src_folder=$(find "$appimage_extract_path/.." -type d -name "$folder" | head -n 1)
            if [ -d "$src_folder" ]; then
                echo "Copying $src_folder"
                cp -r "$src_folder" "$dest_folder"
            elif [ "$folder" = "tahomastuff" ]; then
               echo "[ERROR] Could not find $appimage_extract_paths/$folder"
               exit 1
            else
               echo "[WARNING] Could not find $appimage_extract_paths/$folder"
            fi
        done
    fi
    
    echo "[INFO] Copying $appimage_extract_path/usr/share/applications/org.tahoma2d.Tahoma2D.desktop"
    mkdir -p "$final_folder/usr/local/share/applications"
    cp "$appimage_extract_path/usr/share/applications/org.tahoma2d.Tahoma2D.desktop" "$final_folder/usr/local/share/applications/org.tahoma2d.Tahoma2D.desktop"

    echo "[INFO] Copying $appimage_extract_path/usr/share/icons/hicolor/128x128/apps/org.tahoma2d.Tahoma2D.png"
    mkdir -p "$final_folder/usr/share/icons/hicolor/128x128/apps/"
    cp "$appimage_extract_path/usr/share/icons/hicolor/128x128/apps/org.tahoma2d.Tahoma2D.png" "$final_folder/usr/share/icons/hicolor/128x128/apps/org.tahoma2d.Tahoma2D.png"

    echo "[INFO] Copying $appimage_extract_path/usr/share/icons/hicolor/256x256/apps/org.tahoma2d.Tahoma2D.png"
    mkdir -p "$final_folder/usr/share/icons/hicolor/256x256/apps/"
    cp "$appimage_extract_path/usr/share/icons/hicolor/256x256/apps/org.tahoma2d.Tahoma2D.png" "$final_folder/usr/share/icons/hicolor/256x256/apps/org.tahoma2d.Tahoma2D.png"

    echo "[INFO] Copying $appimage_extract_path/usr/share/metainfo/org.tahoma2d.Tahoma2D.metainfo.xml"
    mkdir -p "$final_folder/usr/share/metainfo/"
    cp "$appimage_extract_path/usr/share/metainfo/org.tahoma2d.Tahoma2D.metainfo.xml" "$final_folder/usr/share/metainfo/org.tahoma2d.Tahoma2D.metainfo.xml"

    echo "[INFO] Copying $appimage_extract_path/AppRun"
    mkdir -p "$final_folder/etc/skel/.local/share/Tahoma2D/"
    cp "$appimage_extract_path/AppRun" "$final_folder/etc/skel/.local/share/Tahoma2D/AppRun"
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
    # Asegurar que package_version tiene un valor válido
    if [ -z "$package_version" ]; then
        echo "[ERROR] package_version is empty!"
        exit 1
    fi

    output_deb="$deb_output_path/tahoma2d_${package_version}_amd64.deb"

    echo "[INFO] Creating Debian package: $output_deb"

    dpkg -b "$final_folder" "$output_deb" || exit 1
}

function _updateInstalledSize() {
    control_file="$final_folder/DEBIAN/control"

    # Verificar si el archivo de control existe
    if [ ! -f "$control_file" ]; then
        echo "[ERROR] Control file not found: $control_file"
        exit 1
    fi

    # Verificar si /etc existe
    if [ -d "$final_folder/etc" ]; then
        echo "[DEBUG] /etc directory exists. Calculating size..."
        installed_size=$(du -s "$final_folder/etc" | awk '{print $1}')
        if [ -z "$installed_size" ]; then
            echo "[ERROR] du command returned an empty value!"
            exit 1
        fi
    else
        echo "[WARNING] Directory $final_folder/etc not found. Setting Installed-Size to 0."
        installed_size=0
    fi

    echo "[DEBUG] Installed Size: $installed_size KB"

    # Asegurar que sed está escribiendo el valor correcto
    if grep -q "^Installed-Size:" "$control_file"; then
        echo "[DEBUG] Replacing existing Installed-Size entry"
        sed -i "s/^Installed-Size:.*/Installed-Size: $installed_size/" "$control_file"
    else
        echo "[DEBUG] Adding new Installed-Size entry"
        echo "Installed-Size: $installed_size" >> "$control_file"
    fi

    echo "[INFO] Installed size updated to $installed_size KB"
}

function _postBuildCleanup() {
    if [ ! -z "$temp_extract" ]; then
       echo "[INFO] Post-build cleanup: Removing $temp_extract"
       rm -rf "$temp_extract"
    fi
    echo "[INFO] Post-build cleanup: Removing $final_folder"
    rm -rf "$final_folder"
}

# ======================================================================
# Main 
# ======================================================================
function _main() {
    deb_output_path="$DEFAULT_DEB_PATH"
    while [ "$#" -gt 0 ]; do
        case "$1" in
            -p|--package) package_version="$2"; shift 2 ;;
            -v|--version) program_version="$2"; shift 2 ;;
            -t|--template) deb_template_path="$2"; shift 2 ;;
            -c|--compressed) compressed_file="$2"; shift 2 ;;
            -x|--appimage-extract) appimage_extract_path="$2"; shift 2 ;;
            -s|--stuff) stuff_path="$2"; shift 2;;
            -f|--ffmpeg) ffmpeg_path="$2"; shift 2;;
            -r|--rhubarb) rhubarb_path="$2"; shift 2;;
            -d|--deb) deb_output_path="$2"; shift 2 ;;
            -h|--help)
                printf "Usage: ./debcreator.sh -p <package_version> -v <program_version> -t <template_path> [-c <compressed_file> | -x <appimage-extract path> -s <stuff path> -f <ffmpeg -path> -r <rhubarb path>] -d <output_deb_path>\n"
                exit 0
                ;;
            *)
                printf "Invalid argument. Use -h for help.\n"
                exit 1
                ;;
        esac
    done

    _setTemplatePath
    _updateVersion
    _findCompressedFile
    _copyTemplate
    _extract
    _extractAppImage
    _moveFiles
    _generateMd5sums
    _updateInstalledSize
    _setPermissions
    _createDeb
    _postBuildCleanup
}

_main "$@"
