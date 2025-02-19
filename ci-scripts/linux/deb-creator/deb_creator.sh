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
# Translations
# ======================================================================

source "./lang_debcreator.sh"

# ======================================================================
# Parameter Handling
# ======================================================================

while [ "$#" -gt 0 ]; do
    case "$1" in
        -p|--package)
            package_version="$2"
            shift 2
            ;;
        -v|--version)
            program_version="$2"
            shift 2
            ;;
        -t|--template)
            deb_template_path="$2"
            shift 2
            ;;
        -c|--compressed)
            compressed_file="$2"
            shift 2
            ;;
        -h|--help)
            printf "%b\n" "$(_msg HELP)\n\n"
            exit 0
            ;;
        *)
            echo "ERROR: Opción desconocida: $1"
            exit 1
            ;;
    esac
done


# ======================================================================
# Functions
# ======================================================================

function _template() {
    if [ -z "$template_template" ]; then
        if [ ! -d "deb-template" ]; then
            read -p "\n\n$(_msg DEB_TEMPLATE_NOT_FOUND)" deb_template_template
        else
            deb_template_template="$(pwd)/deb-template"
        fi
    else
        deb_template_template="$template_template"
    fi
}

function _templateparam() {
    # Asegurar que la ruta de la plantilla no termine con "/"
    deb_template_path="${deb_template_path%/}"

	if [ -z "$deb_template_path" ]; then
		if [ -d "deb-template" ]; then
			deb_template_path="$(pwd)/deb-template"
		else
			echo "ERROR: No se encontró 'deb-template' en el directorio actual y no se especificó con -t." >&2
			exit 1
		fi
	fi

}

function _rename() {
    if [ -e "$deb_template_template/etc/skel/local" ]; then
        mv "$deb_template_template/etc/skel/local" "$deb_template_template/etc/skel/.local"
    fi
}

function _version() {
    control_file="deb-template/DEBIAN/control"
    backup_file="deb-template/DEBIAN/control.bak"

    # Ensure the control file exists
    if [ ! -f "$control_file" ]; then
        echo -e "\n\n$(_msg CONTROL_FILE_NOT_FOUND)"
        exit 1
    fi

    # Create a backup of the control file
    cp "$control_file" "$backup_file"

    # Extract the current version
    current_version=$(grep -oP '(?<=Version: )\d+\.\d+\.\d+' "$control_file")
    if [ -z "$current_version" ]; then
        echo -e "\n\n$(_msg NO_VERSION_FOUND_IN_CONTROL)"
        exit 1
    fi

    # Increment the version
    IFS='.' read -r major minor patch <<< "$current_version"
    if [ "$patch" -ge 9 ]; then
        ((minor++))
        patch=0
    else
        ((patch++))
    fi
    version_number="$major.$minor.$patch"

    # Update the control file with the new version
    sed -i "s/Version: $current_version/Version: $version_number/" "$control_file"

    # Display the new package version
    echo -e "\n\n$(_msg PACKAGE_VERSION)"
    echo "$(_msg AUTOMATIC_CORRELATIVE)"
    
    # Extract current program version
    program_version=$(grep -oP '(?<=Description: Tahoma 2D Version )\d+\.\d+\.\d+' "$control_file")
    
    echo "$(_msg CURRENT_PROGRAM_VERSION)"
    read -p "$(_msg KEPT_OR_CHANGE)" change_program_version
    if [[ "$change_program_version" =~ ^[Nn]$ ]]; then
        read -p "$(_msg NEW_VERSION)" new_program_version
        sed -i "s/Description: Tahoma 2D Version $program_version/Description: Tahoma 2D Version $new_program_version/" "$control_file"
        program_version="$new_program_version"
    fi

    # Confirm final versions
    echo -e "\n\n$(_msg FINAL_PACKAGE_VERSION)"
    echo "$(_msg FINAL_PROGRAM_VERSION)"
    read -p "$(_msg CONTINUE)" confirm
    if [[ "$confirm" =~ ^[Nn]$ ]]; then
        echo "$(_msg RESTORING_CONTROL)"
        mv "$backup_file" "$control_file"
        echo -e "\n\n$(_msg ABORTED)"
        exit 1
    fi

    # Remove the backup after confirmation
    rm "$backup_file"
}

function _versionparam() {
    # Asegurar que la ruta de la plantilla no termine con "/"
    deb_template_path="${deb_template_path%/}"

    control_file="$deb_template_path/DEBIAN/control"

    # Verificar que el archivo existe antes de modificarlo
    if [ ! -f "$control_file" ]; then
        echo "$(_msg ERROR_TEMPLATE)" >&2
        exit 1
    fi

    if [ -n "$package_version" ]; then
        sed -i "s/^Version: .*/Version: $package_version/" "$control_file"
    fi
    if [ -n "$program_version" ]; then
        sed -i "s/^Description: Tahoma 2D Version .*/Description: Tahoma 2D Version $program_version/" "$control_file"
    fi

    version_number="$package_version"
}


function _copytemplate() {
    if [ -z "$version_number" ]; then
        echo "$(_msg ERROR_VERSION_NUMBER_VAR)" >&2
        exit 1
    fi

    # Asegurar que la ruta de la plantilla no termine con "/"
    deb_template_path="${deb_template_path%/}"

    final_folder="tahoma2d_${version_number}_amd64"

    if [ -z "$deb_template_path" ]; then
        echo "$(_msg ERROR_DEB_TEMPLATE_VAR)" >&2
        exit 1
    fi

    if [ ! -d "$deb_template_path" ]; then
        echo "$(_msg ERROR_VAR_TEMPLATE)" >&2
        exit 1
    fi

    echo "$(_msg COPY_DEB_TEMPLATE_TO_FINAL_FOLDER)"
    
    cp -rT "$deb_template_path" "$final_folder"
}


function _compressed() {
	# Prompt for the Tahoma2D compressed file path
    tahoma_compressed=$(find "$(pwd)" -maxdepth 1 -type f \( -name "*.tar.gz" -o -name "*.zip" \) | head -n 1)
	if [ -z "$tahoma_compressed" ]; then
		read -p "$(_msg PATH_COMPRESSED)" tahoma_compressed
	else
		echo "$(_msg FOUND_COMPRESSED): $tahoma_compressed"
	fi
}



function _compressedparam() {
    echo "package_version='$package_version'"
    echo "program_version='$program_version'"
    echo "deb_template_path='$deb_template_path'"
    echo "compressed_file='$compressed_file'"

    if [ -z "$compressed_file" ]; then
        echo "$(_msg ERROR_COMPRESSED_FILE)" >&2
        exit 1
    fi

    if [ ! -f "$compressed_file" ]; then
        echo "$(_msg ERROR_COMPRESSED_NOT_EXISTS)" >&2
        exit 1
    fi

    case "$compressed_file" in
        *.tar.gz|*.zip) ;;
        *)
            echo "$(_msg UNSUPPORTED_FILE_FORMAT)" >&2
            exit 1
            ;;
    esac

    tahoma_compressed="$compressed_file"
}

function _extract() {
	# Extract files
    temp_extract=$(mktemp -d)
    if [[ $tahoma_compressed == *.tar.gz ]]; then
        tar -xzf "$tahoma_compressed" -C "$temp_extract"
    elif [[ $tahoma_compressed == *.zip ]]; then
        unzip "$tahoma_compressed" -d "$temp_extract"
    else
        echo "$(_msg UNSUPPORTED_FILE_FORMAT)"
        exit 1
    fi
    
    find "$temp_extract" -type f \( -name "*.tar.gz" -o -name "*.zip" \) | while read file; do
        if [[ $file == *.tar.gz ]]; then
            tar -xzf "$file" -C "$temp_extract"
        elif [[ $file == *.zip ]]; then
            unzip "$file" -d "$temp_extract"
        fi
        rm "$file"
    done
}

function _appimage() {
    # Buscar el AppImage extraído
    appimage_file=$(find "$temp_extract" -type f -name "Tahoma2D.AppImage" | head -n 1)
    
    if [ -f "$appimage_file" ]; then
        chmod +x "$appimage_file"
        pushd "$temp_extract" > /dev/null
        "$appimage_file" --appimage-extract
        popd > /dev/null
        
        bin_folder="$final_folder/etc/skel/.local/share/Tahoma2D/"
        mkdir -p "$bin_folder"

        # Copiar archivos y sobreescribir si existen
        rsync -a --delete "$temp_extract/squashfs-root/usr/" "$bin_folder/usr/"
    fi
}


function _move() {
	# Move folders
    for folder in ffmpeg rhubarb tahomastuff; do
        src_folder=$(find "$temp_extract" -type d -name "$folder" | head -n 1)
        if [ -d "$src_folder" ]; then
            dest_folder="$final_folder/etc/skel/.local/share/Tahoma2D/$folder"
            rm -rf "$dest_folder"
            mv "$src_folder" "$dest_folder"
        fi
    done
  
    
}

function _md5sums() {
	# Generate md5sums
    pushd "$final_folder"
    find . -type f ! -path "./DEBIAN/*" -exec md5sum {} + > DEBIAN/md5sums
    popd
}

function _permissions() {
	# Set permissions
	chmod 0775 "$final_folder/DEBIAN"
    chmod +x "$final_folder/DEBIAN/postinst"
    chmod +x "$final_folder/usr/local/bin/tahoma2d"
    chmod +x "$final_folder/usr/local/bin/tahoma-permissions"
    chmod +x "$final_folder/etc/skel/.local/share/Tahoma2D/AppRun"
}

function _makedeb() {
	# Create Debian package
    if ! dpkg -b "$final_folder"; then
        echo "$(_msg FAILED)"
        exit 1
    fi
}

# ======================================================================
# Main
# ======================================================================

if [[ -n "$package_version" && -n "$program_version" && -n "$deb_template_path" && -n "$compressed_file" ]]; then
    _versionparam
    _compressedparam
else
	_templateparam
    _template
    _rename
    _version
    _compressed
fi

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    _help
    exit 0
fi

_copytemplate
_extract
_appimage
_move
_md5sums
_permissions
_makedeb

echo -e "\n\n$(_msg COMPLETED)"
