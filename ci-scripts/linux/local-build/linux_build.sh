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
# Constants
# ======================================================================

    # Determine the script's execution directory
    SCRIPT_DIR="$(dirname "$(realpath "$0")")"
    TAHOMA_DIR="$SCRIPT_DIR/tahoma2d"
    USER_CONFIG_DIR="$HOME/.config/Tahoma2D" 
    STUFF_DIR="$TAHOMA_DIR/stuff"
    LIBTIFF_DIR="$TAHOMA_DIR/thirdparty/tiff-4.2.0"
    TOONZ_DIR="$TAHOMA_DIR/toonz"
    BUILD_DIR="$TOONZ_DIR/build"
    BIN_DIR="$BUILD_DIR/bin"
    SOURCES_DIR="$TOONZ_DIR/sources"

# ======================================================================
# Translations
# ======================================================================

function _msg() {
	# Return a translated message based on the key and system language   
    local clave=$1 # Store the key for the requested message
    declare -A msg=( # Translation messages for different languages
		
		[es_ROOTCHECK]="Este script no debe ejecutarse como root."
		[en_ROOTCHECK]="This script should not be run as root."
		[it_ROOTCHECK]="Questo script non dovrebbe essere eseguito come root."
		[de_ROOTCHECK]="Dieses Skript sollte nicht als Root ausgeführt werden."
		[fr_ROOTCHECK]="Ce script ne doit pas être exécuté en tant que root."
		[gl_ROOTCHECK]="Este script non debe executarse como root."
		[pt_ROOTCHECK]="Este script não deve ser executado como root."
		
		[es_DEPENDS]="Primero ejecute ./linux_depends.sh con permisos de root."
		[en_DEPENDS]="First run ./linux_depends.sh with root privileges."
		[it_DEPENDS]="Prima esegui ./linux_depends.sh con privilegi di root."
		[de_DEPENDS]="Führen Sie zuerst ./linux_depends.sh mit Root-Rechten aus."
		[fr_DEPENDS]="Exécutez d'abord ./linux_depends.sh avec les privilèges root."
		[gl_DEPENDS]="Primeiro execute ./linux_depends.sh con permisos de root."
		[pt_DEPENDS]="Primeiro execute ./linux_depends.sh com privilégios de root."
    
		[es_INTERNET]="Este programar equiere conexión a internet"
		[en_INTERNET]="This program requires an internet connection."
		[it_INTERNET]="Questo programma richiede una connessione a Internet."
		[de_INTERNET]="Dieses Programm erfordert eine Internetverbindung."
		[fr_INTERNET]="Ce programme nécessite une connexion Internet."
		[gl_INTERNET]="Este programa require unha conexión a internet."
		[pt_INTERNET]="Este programa requer uma conexão com a internet."
		
		[es_TITLE]="SCRIPT PARA COMPILAR TAHOMA2D"
		[en_TITLE]="SCRIPT TO COMPILE TAHOMA2D"
		[it_TITLE]="SCRIPT PER COMPILARE TAHOMA2D"
		[de_TITLE]="SCRIPT ZUR KOMPILIERUNG VON TAHOMA2D"
		[fr_TITLE]="SCRIPT POUR COMPILER TAHOMA2D"
		[gl_TITLE]="SCRIPT PARA COMPILAR TAHOMA2D"
		[pt_TITLE]="SCRIPT PARA COMPILAR TAHOMA2D"

		[es_NAVIGATE]="Desplazarse"
		[en_NAVIGATE]="Navigate"
		[it_NAVIGATE]="Spostarsi"
		[de_NAVIGATE]="Bewegen"
		[fr_NAVIGATE]="Se déplacer"
		[gl_NAVIGATE]="Desprazarse"
		[pt_NAVIGATE]="Deslocar-se"

		[es_CONFIRM]="Confirmar"
		[en_CONFIRM]="Confirm"
		[it_CONFIRM]="Confermare"
		[de_CONFIRM]="Bestätigen"
		[fr_CONFIRM]="Confirmer"
		[gl_CONFIRM]="Confirmar"
		[pt_CONFIRM]="Confirmar"

		[es_WARNING]="A continuación se compilará Tahoma2D directamente desde su código fuente.\nEste proceso puede tardar un largo tiempo."
		[en_WARNING]="Tahoma2D will now be compiled directly from its source code.\nThis process may take a long time."
		[it_WARNING]="Tahoma2D verrà ora compilato direttamente dal suo codice sorgente.\nQuesto processo potrebbe richiedere molto tempo."
		[de_WARNING]="Tahoma2D wird nun direkt aus dem Quellcode kompiliert.\nDieser Prozess könnte viel Zeit in Anspruch nehmen."
		[fr_WARNING]="Tahoma2D sera maintenant compilé directement à partir de son code source.\nCe processus pourrait prendre beaucoup de temps."
		[gl_WARNING]="A continuación, compilarase Tahoma2D directamente dende o seu código fonte.\nEste proceso pode levar moito tempo."
		[pt_WARNING]="Tahoma2D será agora compilado diretamente do seu código-fonte.\nEste processo pode levar muito tempo."

		[es_CANCEL]="Compilación cancelada."
		[en_CANCEL]="Compilation canceled."
		[it_CANCEL]="Compilazione annullata."
		[de_CANCEL]="Kompilierung abgebrochen."
		[fr_CANCEL]="Compilation annulée."
		[gl_CANCEL]="Compilación cancelada."
		[pt_CANCEL]="Compilação cancelada."

		[es_EXECUTE]="Ejecutar desde el directorio actual"
		[en_EXECUTE]="Run from the current directory"
		[it_EXECUTE]="Esegui dalla directory corrente"
		[de_EXECUTE]="Aus dem aktuellen Verzeichnis ausführen"
		[fr_EXECUTE]="Exécuter depuis le répertoire actuel"
		[gl_EXECUTE]="Executar dende o directorio actual"
		[pt_EXECUTE]="Executar a partir do diretório atual"

		[es_INSTALL]="Instalar en /opt"
		[en_INSTALL]="Install in /opt"
		[it_INSTALL]="Installare in /opt"
		[de_INSTALL]="In /opt installieren"
		[fr_INSTALL]="Installer dans /opt"
		[gl_INSTALL]="Instalar en /opt"
		[pt_INSTALL]="Instalar em /opt"

		[es_EXIT]="Salir"
		[en_EXIT]="Exit"
		[it_EXIT]="Esci"
		[de_EXIT]="Beenden"
		[fr_EXIT]="Quitter"
		[gl_EXIT]="Saír"
		[pt_EXIT]="Sair"
					
		[es_NOT_REWRITE]="Se utilizará el contenido actual del directorio $TAHOMA_DIR"
		[en_NOT_REWRITE]="The current content of the $TAHOMA_DIR directory will be used"
		[it_NOT_REWRITE]="Il contenuto attuale della directory $TAHOMA_DIR verrà utilizzato"
		[de_NOT_REWRITE]="Der aktuelle Inhalt des Verzeichnisses $TAHOMA_DIR wird verwendet"
		[fr_NOT_REWRITE]="Le contenu actuel du répertoire $TAHOMA_DIR sera utilisé"
		[gl_NOT_REWRITE]="Empregarase o contido actual do directorio $TAHOMA_DIR"
		[pt_NOT_REWRITE]="O conteúdo atual do diretório $TAHOMA_DIR será usado"
						
		[es_SOURCES]="Error: El directorio de fuentes $SOURCES_DIR no existe."
		[en_SOURCES]="Error: The source directory $SOURCES_DIR does not exist."
		[it_SOURCES]="Errore: La directory di origine $SOURCES_DIR non esiste."
		[de_SOURCES]="Fehler: Das Quellverzeichnis $SOURCES_DIR existiert nicht."
		[fr_SOURCES]="Erreur : Le répertoire source $SOURCES_DIR n'existe pas."
		[gl_SOURCES]="Erro: O directorio de fontes $SOURCES_DIR non existe."
		[pt_SOURCES]="Erro: O diretório de fontes $SOURCES_DIR não existe."
	
		[es_DIR_EXISTS]="Directorio $TAHOMA_DIR ya existe."
		[en_DIR_EXISTS]="Directory $TAHOMA_DIR already exists."
		[it_DIR_EXISTS]="La directory $TAHOMA_DIR esiste."
		[de_DIR_EXISTS]="Verzeichnis $TAHOMA_DIR existiert."
		[fr_DIR_EXISTS]="Le répertoire $TAHOMA_DIR existe déjà."
		[gl_DIR_EXISTS]="Directorio $TAHOMA_DIR xa existe."
		[pt_DIR_EXISTS]="O diretório $TAHOMA_DIR já existe."

		[es_OVERWRITE]="¿Quieres sobrescribirlo? (S/n)"
		[en_OVERWRITE]="Do you want to overwrite it? (Y/n)"
		[it_OVERWRITE]="Vuoi sovrascriverlo? (S/n)"
		[de_OVERWRITE]="Möchten Sie es überschreiben? (J/n)"
		[fr_OVERWRITE]="Voulez-vous le remplacer ? (O/n)"
		[gl_OVERWRITE]="Queres sobrescribilo? (S/n)"
		[pt_OVERWRITE]="Você deseja sobrescrevê-lo? (S/n)"

		[es_BYE]="Para ejecutar desde el directorio, utiliza: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nPara instalar en /opt: \nmake install"
		[en_BYE]="To run from the directory, use: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nTo install in /opt: \nmake install"
		[it_BYE]="Per eseguire dalla directory, usa: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nPer installare in /opt: \nmake install"
		[de_BYE]="Um aus dem Verzeichnis auszuführen, verwende: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nUm in /opt zu installieren: \nmake install"
		[fr_BYE]="Pour exécuter depuis le répertoire, utilisez : \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nPour installer dans /opt : \nmake install"
		[gl_BYE]="Para executar dende o directorio, usa: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nPara instalar en /opt: \nmake install"
		[pt_BYE]="Para executar a partir do diretório, use: \nLD_LIBRARY_PATH=./lib/opentoonz:$LD_LIBRARY_PATH \n./bin/Tahoma2D \n\nPara instalar em /opt: \nmake install"
  
  		[es_TAHOMA2D_FOLDER_FOUND]="Se encontró la carpeta Tahoma2D. Continuando..."
		[en_TAHOMA2D_FOLDER_FOUND]="Tahoma2D folder found. Continuing..."
		[it_TAHOMA2D_FOLDER_FOUND]="Cartella Tahoma2D trovata. Continuazione..."
		[de_TAHOMA2D_FOLDER_FOUND]="Tahoma2D-Ordner gefunden. Fortfahren..."
		[fr_TAHOMA2D_FOLDER_FOUND]="Dossier Tahoma2D trouvé. Continuation..."
		[gl_TAHOMA2D_FOLDER_FOUND]="Atopouse o cartafol Tahoma2D. Continuando..."
		[pt_TAHOMA2D_FOLDER_FOUND]="Pasta Tahoma2D encontrada. Continuando..."
		
		[es_REPOSITORY_NOT_FOUND]="No se encontró el repositorio. Coloque la carpeta Tahoma2D al mismo nivel que este script para continuar."
		[en_REPOSITORY_NOT_FOUND]="Repository not found. Place the Tahoma2D folder at the same level as this script to continue."
		[it_REPOSITORY_NOT_FOUND]="Repository non trovato. Posizionare la cartella Tahoma2D allo stesso livello di questo script per continuare."
		[de_REPOSITORY_NOT_FOUND]="Repository nicht gefunden. Platzieren Sie den Tahoma2D-Ordner auf derselben Ebene wie dieses Skript, um fortzufahren."
		[fr_REPOSITORY_NOT_FOUND]="Dépôt introuvable. Placez le dossier Tahoma2D au même niveau que ce script pour continuer."
		[gl_REPOSITORY_NOT_FOUND]="Non se atopou o repositorio. Coloque o cartafol Tahoma2D ao mesmo nivel que este script para continuar."
		[pt_REPOSITORY_NOT_FOUND]="Repositório não encontrado. Coloque a pasta Tahoma2D no mesmo nível deste script para continuar."
		
		[es_OPTIONS]="Opciones:"
		[en_OPTIONS]="Options:"
		[it_OPTIONS]="Opzioni:"
		[de_OPTIONS]="Optionen:"
		[fr_OPTIONS]="Options :"
		[gl_OPTIONS]="Opcións:"
		[pt_OPTIONS]="Opções:"
		
		[es_SEARCH_AGAIN]="Volver a buscar"
		[en_SEARCH_AGAIN]="Search again"
		[it_SEARCH_AGAIN]="Cerca di nuovo"
		[de_SEARCH_AGAIN]="Erneut suchen"
		[fr_SEARCH_AGAIN]="Rechercher à nouveau"
		[gl_SEARCH_AGAIN]="Volver a buscar"
		[pt_SEARCH_AGAIN]="Procurar novamente"
		
		[es_AUTOMATIC_CLONING]="Clonación automática"
		[en_AUTOMATIC_CLONING]="Automatic cloning"
		[it_AUTOMATIC_CLONING]="Clonazione automatica"
		[de_AUTOMATIC_CLONING]="Automatische Klonung"
		[fr_AUTOMATIC_CLONING]="Clonage automatique"
		[gl_AUTOMATIC_CLONING]="Clonación automática"
		[pt_AUTOMATIC_CLONING]="Clonagem automática"
		
		[es_SELECT_OPTION]="Seleccione una opción:"
		[en_SELECT_OPTION]="Select an option:"
		[it_SELECT_OPTION]="Seleziona un'opzione:"
		[de_SELECT_OPTION]="Wählen Sie eine Option:"
		[fr_SELECT_OPTION]="Sélectionnez une option :"
		[gl_SELECT_OPTION]="Seleccione unha opción:"
		[pt_SELECT_OPTION]="Selecione uma opção:"
		
		[es_CLONING_REPOSITORY]="Clonando el repositorio..."
		[en_CLONING_REPOSITORY]="Cloning the repository..."
		[it_CLONING_REPOSITORY]="Clonazione del repository..."
		[de_CLONING_REPOSITORY]="Repository wird geklont..."
		[fr_CLONING_REPOSITORY]="Clonage du dépôt..."
		[gl_CLONING_REPOSITORY]="Clonando o repositorio..."
		[pt_CLONING_REPOSITORY]="Clonando o repositório..."
		
		[es_CLONING_COMPLETED_CONTINUING]="Clonación completada. Continuando..."
		[en_CLONING_COMPLETED_CONTINUING]="Cloning completed. Continuing..."
		[it_CLONING_COMPLETED_CONTINUING]="Clonazione completata. Continuando..."
		[de_CLONING_COMPLETED_CONTINUING]="Klonvorgang abgeschlossen. Fortfahren..."
		[fr_CLONING_COMPLETED_CONTINUING]="Clonage terminé. Continuation..."
		[gl_CLONING_COMPLETED_CONTINUING]="Clonación completada. Continuando..."
		[pt_CLONING_COMPLETED_CONTINUING]="Clonagem concluída. Continuando..."
		
		[es_CLONING_ERROR_RETRY]="Error en la clonación. Intente nuevamente."
		[en_CLONING_ERROR_RETRY]="Cloning error. Please try again."
		[it_CLONING_ERROR_RETRY]="Errore nella clonazione. Riprova."
		[de_CLONING_ERROR_RETRY]="Fehler beim Klonen. Bitte versuche es erneut."
		[fr_CLONING_ERROR_RETRY]="Erreur de clonage. Veuillez réessayer."
		[gl_CLONING_ERROR_RETRY]="Erro na clonación. Inténtao de novo."
		[pt_CLONING_ERROR_RETRY]="Erro na clonagem. Tente novamente."
		
		[es_INVALID_OPTION_RETRY]="Opción no válida. Intente de nuevo."
		[en_INVALID_OPTION_RETRY]="Invalid option. Please try again."
		[it_INVALID_OPTION_RETRY]="Opzione non valida. Riprova."
		[de_INVALID_OPTION_RETRY]="Ungültige Option. Bitte versuche es erneut."
		[fr_INVALID_OPTION_RETRY]="Option invalide. Veuillez réessayer."
		[gl_INVALID_OPTION_RETRY]="Opción non válida. Inténtao de novo."
		[pt_INVALID_OPTION_RETRY]="Opção inválida. Tente novamente."
	)

    local idioma=$(echo $LANG | cut -d_ -f1)
    local mensaje=${msg[${idioma}_$clave]:-${msg[en_$clave]}}

    printf "%s" "$mensaje"
}

# ======================================================================
# Functions
# ======================================================================

function _checkroot() {
    # Check if the script is running WITH root privileges (and exit if true)
    if [ "$EUID" -eq 0 ]; then
        echo ''
        echo "$(_msg ROOTCHECK)"
        echo ''
        exit 1
    fi
}

function _checkdepends() {
	# Check if dependencies have been installed
	if [ ! -e "$SCRIPT_DIR/ok-packages" ]; then
	echo""
	echo "$(_msg DEPENDS)"
	echo""
	exit 1
	fi
}

function _copyright() {
    # Generate the backtitle with left and right alignment
    local width=$(tput cols)  # Get the terminal width
    local left_text="$(_msg TITLE)"
    local right_text="(c) 2019-2025 Charlie Martínez GPLv2"

    # Define a safe width to avoid text truncation
    local safe_width=$((width - 2))  # Subtract 2 for margins
    local max_left_length=$((safe_width / 2))  # Max length for the left text
    local max_right_length=$((safe_width / 2)) # Max length for the right text

    # Truncate the text if it exceeds the assigned space
    if (( ${#left_text} > max_left_length )); then
        left_text="${left_text:0:max_left_length-1}…"
    fi
    if (( ${#right_text} > max_right_length )); then
        right_text="${right_text:0:max_right_length-1}…"
    fi

    # Calculate padding between the texts
    local total_length=$(( ${#left_text} + ${#right_text} ))  # Total length of both texts
    local padding=$((safe_width - total_length))

    # If space is very small, force at least one space
    if (( padding < 1 )); then
        padding=1
    fi

    # Build the line with manual alignment
    printf "%s%*s%s" "$left_text" "$padding" "" "$right_text"
}
   
function _checkinternet() {
     # Check if there's an internet connection
    if ! ping -c 1 google.com &> /dev/null; then
        dialog --msgbox "$(_msg INTERNET)" 10 60
        exit 1
    fi
}


function _warning() {
    # Show warning message before compilation
    clear
    dialog --title "$(_msg NAVIGATE) ← →  | $(_msg CONFIRM) <Enter>" --backtitle "$(_copyright)" \
        --yesno "$(_msg WARNING)" 10 60 >/dev/tty 2>/dev/tty
    response=$?
    if [ $response -ne 0 ]; then
       	# If the user selects 'No', exit the program
       	clear
       	exit 1
    fi
}

function _cloningTahoma() {

    local TAHOMA_DIR="./tahoma2d"
    
    # Check if the repository local folder exists
    if [ -d "$TAHOMA_DIR" ]; then
        echo "$(_msg TAHOMA2D_FOLDER_FOUND)"
        return
    fi
    
    # If not found, show options to the use
    while true; do
        echo "$(_msg REPOSITORY_NOT_FOUND)"
        echo "$(_msg OPTIONS)"
        echo "  1) $(_msg SEARCH_AGAIN)"
        echo "  2) $(_msg AUTOMATIC_CLONING)"
        echo "  3) $(_msg EXIT)"
        read -p "$(_msg SELECT_OPTION) " option

        case $option in
            1)
                continue # Search again
                ;;
            2)
                echo "$(_msg CLONING_REPOSITORY)"
                git clone https://github.com/tahoma2d/tahoma2d "$TAHOMA_DIR"
                if [ -d "$TAHOMA_DIR" ]; then
                    echo "$(_msg CLONING_COMPLETED_CONTINUING)"
                    return
                else
                    echo "$(_msg CLONING_ERROR_RETRY)"
                fi
                ;;
            3)
                exit 1
                ;;
            *)
                echo "$(_msg INVALID_OPTION_RETRY)"
                ;;
        esac
    done
}

function _stuff() {

    mkdir -p "$USER_CONFIG_DIR"
    # Check if the 'stuff' directory exists before copying
    if [ -d "$STUFF_DIR" ]; then
        cp -rf "$STUFF_DIR" "$USER_CONFIG_DIR/"
    fi
}


function _systemVar() {

    mkdir -p "$USER_CONFIG_DIR"

    cat << EOF > "$USER_CONFIG_DIR/SystemVar.ini"
[General]
TAHOMA2DROOT="$USER_CONFIG_DIR/stuff"
TAHOMA2DPROFILES="$USER_CONFIG_DIR/stuff/profiles"
TAHOMA2DCACHEROOT="$USER_CONFIG_DIR/stuff/cache"
TAHOMA2DCONFIG="$USER_CONFIG_DIR/stuff/config"
TAHOMA2DFXPRESETS="$USER_CONFIG_DIR/stuff/fxs"
TAHOMA2DLIBRARY="$USER_CONFIG_DIR/stuff/library"
TAHOMA2DPROJECTS="$USER_CONFIG_DIR/stuff/projects"
TAHOMA2DSTUDIOPALETTE="$USER_CONFIG_DIR/stuff/studiopalette"
EOF
}

function _libTiff() {
    cd "$LIBTIFF_DIR"
    ./configure --with-pic --disable-jbig --disable-webp
    make -j$(nproc)
    cd "$TAHOMA_DIR"  # Return to the base directory.
}

function _build() {
    if [ ! -d "$SOURCES_DIR" ]; then
        clear
        echo -e "\n\n$(_msg SOURCES)\n\n"
        exit 1
    fi
    
    mkdir -p "$BUILD_DIR"   

    cd "$BUILD_DIR"  # Make sure the compilation is done in the correct directory.

    if command -v dnf >/dev/null 2>&1 || command -v yum >/dev/null 2>&1; then
        cmake "$SOURCES_DIR" -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DSUPERLU_INCLUDE_DIR=/usr/include/SuperLU
    else
        cmake "$SOURCES_DIR" -DCMAKE_INSTALL_PREFIX="$BUILD_DIR"
    fi

    make -j$(nproc)
}


function _run() {
    # Running Tahoma2D using absolute path
    clear
    LD_LIBRARY_PATH="$BUILD_DIR/lib/tahoma2d:$LD_LIBRARY_PATH"
    cd $BIN_DIR
    ./Tahoma2D
}

function _install() {
    # System installation using absolute paths
    clear
    su -c "make install"
}

function _menu() {
    # Display the final menu
    clear
    opPrincipal=$(dialog --title "$(_msg MAIN_MENU)" --backtitle "$(_copyright)" \
        --nocancel \
        --stdout \
        --menu "$(_msg NAVIGATE): ↑↓ | $(_msg CONFIRM): <Enter>" 16 65 8 \
        1 "$(_msg EXECUTE)" \
        2 "$(_msg INSTALL)" \
        3 "$(_msg EXIT)"
    )
    echo $opPrincipal

    if [[ "$opPrincipal" == "1" ]]; then # Run from the current directory
        _run
    fi

    if [[ "$opPrincipal" == "2" ]]; then # Install to /opt
        _install
    fi

    if [[ "$opPrincipal" == "3" ]]; then # Install specific components of the derivative.
        dialog --msgbox "$(_msg BYE)" 10 60
        clear
        exit 0
    fi

}

# ======================================================================
# Functions execution
# ======================================================================

_checkroot     # Check if the script is running WITHOUT root privileges
_checkdepends  # Ensure dependencies are installed
_checkinternet      # Check for internet connection


# Compile:
clear
_cloningTahoma # Cloning the GIT Tree
_warning       # Show a warning about compilation time
clear
_stuff         # Copying the 'stuff' Directory
_systemVar     # Creating SystemVar.ini
_libTiff       # Building LibTIFF
_build         # Building Tahoma2D
_menu          # Ask if you want run /build or install in /opt
