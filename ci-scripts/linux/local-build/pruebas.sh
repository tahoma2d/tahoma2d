#!/bin/bash

# ======================================================================
# Variables
# ======================================================================

	REPO_URL="https://github.com/tahoma2d/tahoma2d.git"
	#REPO_URL="https://github.com/charliemartinez/tahoma2d.git" # author script fork
	OPTS=""
	SCRIPT_DIR="$(dirname "$(realpath "$0")")"
	TAHOMA_DIR="$SCRIPT_DIR/tahoma2d"
	USER_CONFIG_DIR="$HOME/.config/Tahoma2D" 
	STUFF_DIR="$TAHOMA_DIR/stuff"
	THIRDPARTY_DIR="$TAHOMA_DIR/thirdparty"
	LIBTIFF_DIR="$THIRD_DIR/tiff-4.2.0"
	TOONZ_DIR="$TAHOMA_DIR/toonz"
	BUILD_DIR="$TOONZ_DIR/build"
	BIN_DIR="$BUILD_DIR/bin"
	SOURCES_DIR="$TOONZ_DIR/sources"
	
	# Checkfiles:
	CHECK_DIR="$SCRIPT_DIR/checkfiles"
	CHECKFILE_PACKAGES="$CHECK_DIR/ok-packages"
	CHECKFILE_OPENCV="$CHECK_DIR/ok-opencv"
	CHECKFILE_FFMPEG="$CHECK_DIR/ok-ffmpeg"
	CHECKFILE_GPHOTO="$CHECK_DIR/ok-gphoto"
	CHECKFILE_MPAINT="$CHECK_DIR/ok-mpaint"

function _depends() {
	# Detect and execute system's package manager
		if command -v apt-get >/dev/null 2>&1; then
			_debianDepends
		elif command -v dnf >/dev/null 2>&1; then
			_fedoraDepends
		elif command -v pacman >/dev/null 2>&1; then
			_archDepends
		elif command -v zypper >/dev/null 2>&1; then
			_openSuseDepends
		else
			echo -e "\n\n$(_msg PACKAGE_GESTOR_NOT_FOUND)\n\n"
			exit 1
		fi
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
                    echo "$(_msg SELECT_BRANCH_OR_EXIT)"
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

function _arguments() {
	# Parse arguments
	selected_libraries=()
	install_dependencies_only=true
	reset_requested=false

	# Primera pasada: solo mostrar avisos y confirmar
	for arg in "$@"; do
		if [[ "$arg" == "-r" || "$arg" == "--reset" ]]; then
			reset_requested=true
			echo -e "\n$(_msg RESET_DETECTED) "
			echo -e "$(_msg RESET_WARNING)"
			echo -e "\n$(_msg PROCEED_WITH_RESET)"
			echo "1) $(_msg ACEPT)"
			echo "2) $(_msg CANCEL)"
			read -p "$(_msg OPTION): " choice

			if [[ "$choice" -ne 1 ]]; then
				echo "$(_msg EXIT_INSTALL)"
				exit 1
			fi
		fi
	done

	# Ejecutar _depends antes de continuar con la ejecución
	_depends 

	# Segunda pasada: Procesar argumentos y ejecutar acciones
	if [ $# -ne 0 ]; then
		install_dependencies_only=false
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
					if [ -e "$CHECKFILE_FFMPEG" ]; then
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
				-r|--reset)
					# Ya procesado en la primera pasada, ignorar aquí
					;;
				*)
					echo -e "\n$(_msg USAGE)\n"
					exit 1
					;;
			esac
		done
	fi

	# Confirmar compilación si hay librerías seleccionadas
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

	# Clonar repositorio si es necesario
	if ! $install_dependencies_only; then
		_cloneIf
	fi

	# Compilar librerías seleccionadas
	for param in "${selected_libraries[@]}"; do
		case $param in
			"OpenCV")
				echo -e "\n$(_msg PARAMETER_O_OPENCV_SELECTED)\n"
				_opencv
				;;
			"FFmpeg")
				echo -e "\n$(_msg PARAMETER_F_FFMPEG_SELECTED)\n"
				_ffmpeg
				;;
			"GPhoto")
				echo -e "\n$(_msg PARAMETER_GPHOTO_SELECTED)\n"
				_gphoto
				;;
			"MyPaint")
				echo -e "\n$(_msg PARAMETER_MPAINT_SELECTED)\n"
				_libMyPaint
				;;
		esac
	done
}


function _main() {
	_checkRoot # This program need administrator privileges.
	_checkInternet # We need the internet
	_checkPackages # Check if this script has already been executed.
	_arguments "$@" # # Parse arguments
	_warning # Display warning before install packages

		if [ -z "$SKIP_UPDATE_DEPENDS" ]; then
			_update # Detect the package manager and update the system
			_depends # Detect and execute system's package manager
		fi

	_ownership
}

_main "$@"
