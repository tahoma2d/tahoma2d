#!/bin/bash

# ======================================================================
# File:       linux_linux_depends.sh
# Author:     Charlie Martínez® <cmartinez@quirinux.org>
# License:    https://www.gnu.org/licenses/gpl-2.0.txt
# Purpose:    Download dependences for compiling Tahoma2d
# Supported:  Multi-distro (originally for Quirinux 2.0)
# ======================================================================

#
# Copyright (c) 2019-2025 Charlie Martínez. All Rights Reserved.  
# License: https://www.gnu.org/licenses/gpl-2.0.txt  
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

function _msg() {
	# Return a translated message based on the key and system language   
    local clave=$1 # Store the key for the requested message
    declare -A msg=( # Translation messages for different languages
    
    	[es_INTERNET]="Este programar equiere conexión a internet"
		[en_INTERNET]="This program requires an internet connection."
		[it_INTERNET]="Questo programma richiede una connessione a Internet."
		[de_INTERNET]="Dieses Programm erfordert eine Internetverbindung."
		[fr_INTERNET]="Ce programme nécessite une connexion Internet."
		[gl_INTERNET]="Este programa require unha conexión a internet."
		[pt_INTERNET]="Este programa requer uma conexão com a internet."
		
		[es_PACKAGE_GESTOR_NOT_FOUND]="No se encontró un gestor de paquetes compatible."
		[en_PACKAGE_GESTOR_NOT_FOUND]="No compatible package manager found."
		[it_PACKAGE_GESTOR_NOT_FOUND]="Nessun gestore di pacchetti compatibile trovato."
		[de_PACKAGE_GESTOR_NOT_FOUND]="Kein kompatibler Paketmanager gefunden."
		[fr_PACKAGE_GESTOR_NOT_FOUND]="Aucun gestionnaire de paquets compatible trouvé."
		[gl_PACKAGE_GESTOR_NOT_FOUND]="Non se atopou un xestor de paquetes compatible."
		[pt_PACKAGE_GESTOR_NOT_FOUND]="Nenhum gerenciador de pacotes compatível encontrado."
		
		[es_OK_DEPENDS]="Dependencias instaladas. Ejecute linux_build.sh SIN PERMISOS DE ROOT."
		[en_OK_DEPENDS]="Dependencies installed. Run linux_build.sh WITHOUT ROOT PRIVILEGES."
		[it_OK_DEPENDS]="Dipendenze installate. Esegui linux_build.sh SENZA privilegi di root."
		[de_OK_DEPENDS]="Abhängigkeiten installiert. Führen Sie linux_build.sh OHNE ROOT-BERECHTIGUNGEN aus."
		[fr_OK_DEPENDS]="Dépendances installées. Exécutez linux_build.sh SANS PREROGATIVES ROOT."
		[gl_OK_DEPENDS]="Dependencias instaladas. Execute linux_build.sh SEN WITHOUT PERMISOS DE ROOT."
		[pt_OK_DEPENDS]="Dependências instaladas. Execute linux_build.sh SEM PRivilégios DE ROOT."
		
		[es_ROOTCHECK]="Este script debe ejecutarse con permisos de root."
		[en_ROOTCHECK]="This script must be run as root."
		[it_ROOTCHECK]="Questo script deve essere eseguito come root."
		[de_ROOTCHECK]="Dieses Skript muss als Root ausgeführt werden."
		[fr_ROOTCHECK]="Ce script doit être exécuté en tant que root."
		[gl_ROOTCHECK]="Este script debe executarse con permisos de root."
		[pt_ROOTCHECK]="Este script deve ser executado como root."
		
		[es_ACEPT]="Aceptar"
		[en_ACEPT]="Accept"
		[it_ACEPT]="Accetta"
		[de_ACEPT]="Akzeptieren"
		[fr_ACEPT]="Accepter"
		[gl_ACEPT]="Aceptar"
		[pt_ACEPT]="Aceitar"

		[es_CANCEL]="Cancelar"
		[en_CANCEL]="Cancel"
		[it_CANCEL]="Annulla"
		[de_CANCEL]="Abbrechen"
		[fr_CANCEL]="Annuler"
		[gl_CANCEL]="Cancelar"
		[pt_CANCEL]="Cancelar"

	    [es_WARNING]="A continuación se instalarán las dependencias necesarias para ejecutar ./linux_build.sh."
		[en_WARNING]="The following dependencies will be installed to run ./linux_build.sh."
		[it_WARNING]="Le seguenti dipendenze verranno installate per eseguire ./linux_build.sh."
		[de_WARNING]="Die folgenden Abhängigkeiten werden installiert, um ./linux_build.sh auszuführen."
		[fr_WARNING]="Les dépendances suivantes seront installées pour exécuter ./linux_build.sh."
		[gl_WARNING]="A continuación instalaranse as dependencias necesarias para executar ./linux_build.sh."
		[pt_WARNING]="As seguintes dependências serão instaladas para executar ./linux_build.sh."
		
		[es_OPTION]="Opción"
		[en_OPTION]="Option"
		[it_OPTION]="Opzione"
		[de_OPTION]="Option"
		[fr_OPTION]="Option"
		[gl_OPTION]="Opción"
		[pt_OPTION]="Opção"
		
		[es_CANCEL]="Cancelar"
		[en_CANCEL]="Cancel"
		[it_CANCEL]="Annulla"
		[de_CANCEL]="Abbrechen"
		[fr_CANCEL]="Annuler"
		[gl_CANCEL]="Cancelar"
		[pt_CANCEL]="Cancelar"
		
		[es_EXIT_INSTALL]="Instalación cancelada."
		[en_EXIT_INSTALL]="Installation canceled."
		[it_EXIT_INSTALL]="Installazione annullata."
		[de_EXIT_INSTALL]="Installation abgebrochen."
		[fr_EXIT_INSTALL]="Installation annulée."
		[gl_EXIT_INSTALL]="Instalación cancelada."
		[pt_EXIT_INSTALL]="Instalação cancelada."

	)

    local idioma=$(echo $LANG | cut -d_ -f1)
    local mensaje=${msg[${idioma}_$clave]:-${msg[en_$clave]}}

    printf "%s" "$mensaje"
}

# ======================================================================
# Functions
# ======================================================================

function _rootcheck() {	
	# Check if the user has administrator privileges
	if [ "$EUID" -ne 0  ]; then
		echo ''
		echo $(_msg ROOTCHECK)
		echo ''
		exit 1
	fi	
}

function _debianDepends() {
	# Install dependencies for Debian-based systems (e.g., Ubuntu)
    for paquetes in dialog build-essential git cmake freeglut3-dev libboost-all-dev libegl1-mesa-dev \
        libfreetype6-dev libgles2-mesa-dev libglew-dev libglib2.0-dev libjpeg-dev libjpeg-turbo8-dev \
        libjson-c-dev liblz4-dev liblzma-dev liblzo2-dev libpng-dev libsuperlu-dev pkg-config \
        qt5-default qtbase5-dev libqt5svg5-dev qtscript5-dev qttools5-dev qttools5-dev-tools \
        libqt5opengl5-dev qtmultimedia5-dev qtwayland5 libqt5multimedia5-plugins libturbojpg0-dev \
        libqt5serialport5-dev libsuperlu-dev liblz4-dev liblzo-dev libmypaint-dev libglew-dev \
        freeglut3-dev qt59multimedia qt59script qt59serialport qt59svg qt59tools libopencv-dev \
        libgsl2 libopenblas-dev libmypaint-1.5-1 libmypaint; do apt-get install -y $paquetes 
    done
    touch /tmp/ok-packages
    echo -e "\n\n$(_msg OK_DEPENDS)\n\n"
}

function _fedoraDepends() {
	# Install dependencies for Fedora-based systems
    for paquetes in dialog gcc gcc-c++ automake git cmake boost boost-devel SuperLU SuperLU-devel \
        lz4-devel lzma lzo-devel libjpeg-turbo-devel libGLEW glew-devel freeglut-devel freeglut \
        freetype-devel libpng-devel qt5-qtbase-devel qt5-qtsvg qt5-qtsvg-devel qt5-qtscript \
        qt5-qtscript-devel qt5-qttools qt5-qttools-devel qt5-qtmultimedia-devel blas blas-devel \
        json-c-deve libtool intltool make qt5-qtmultimedia libmypaint-devel libmypaint; do dnf install -y $paquetes
    done
    touch /tmp/ok-packages
	echo -e "\n\n$(_msg OK_DEPENDS)\n\n"
}

function _archDepends() {
	# Install dependencies for Arch Linux-based systems
	echo -e "\n\n"
    for paquetes in dialog base-devel git cmake boost boost-libs qt5-base qt5-svg qt5-script \
        qt5-tools qt5-multimedia lz4 lzo libjpeg-turbo glew freeglut freetype2 blas cblas \
        superlu libmypaint; do pacman -S --needed
    done
    touch /tmp/ok-packages
	echo -e "\n\n$(_msg OK_DEPENDS)\n\n"
}

function _openSuseDepends() {
	# Install dependencies for openSUSE systems
    echo -e "\n\n"
    for paquetes in dialog boost-devel cmake freeglut-devel freetype2-devel gcc-c++ glew-devel \
        libQt5OpenGL-devel libjpeg-devel liblz4-devel libpng16-compat-devel libqt5-linguist-devel \
        libqt5-qtbase-devel libqt5-qtmultimedia-devel libqt5-qtscript-devel libqt5-qtsvg-devel \
        libtiff-devel lzo-devel openblas-devel pkgconfig sed superlu-devel zlib-devel json-c-devel \
        libqt5-qtmultimedia libmypaint-devel libmypaint; do zypper install -y
    done
    touch /tmp/ok-packages
    echo -e "\n\n$(_msg OK_DEPENDS})\n\n"
}

function _verifpackages() {
	if [ -e "/tmp/ok-packages" ]; then
	echo -e "\n\n$(_msg OK_DEPENDS)\n\n"
	exit 1
fi 
}

function _warning() {
    while true; do
		clear
        # Show the message and prompt the user for input
        echo "$(_msg WARNING)"
        echo "1) $(_msg ACEPT)"
        echo "2) $(_msg CANCEL)"
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
			apt-get update && apt-get upgrade -y
		elif command -v dnf &>/dev/null; then
			dnf update -y
		elif command -v yum &>/dev/null; then
			yum update -y
		else
			echo -e "\n\n$(_msg PACKAGE_GESTOR_NOT_FOUND)\n\n"
			exit 1
		fi   
}

function _depends() {
	# Detect and install dependencies based on the system's package manager
	clear	
	
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

# ======================================================================
# Functions execution
# ======================================================================

_rootcheck      # Check for root privileges
_verifpackages  # Verify the existence of the test file
_warning		# Show a warning about process
_update         # Update the system
_depends        # Install dependencies

