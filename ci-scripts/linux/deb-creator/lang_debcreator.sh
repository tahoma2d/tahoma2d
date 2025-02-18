#!/bin/bash

# ======================================================================
# File:       lang_debcreator.sh
# Author:     Charlie Martínez® <cmartinez@quirinux.org>
# License:    BSD 3-Clause "New" or "Revised" License
# Purpose:    Translations for tahoma_debcreator.sh
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

function _msg() {
	# Return a translated message based on the key and system language   
    local key=$1 # Store the key for the requested message
    declare -A msg=( # Translation messages for different languages
    
		[es_HELP]="\n\nUso: $0 [opciones]\n\nOpciones:\n  -p, --package <versión>      Versión del paquete (Ej: 1.0.3).\n  -v, --version <versión>      Versión del programa (Ej: 2.3.1).\n  -t, --template <ruta>        Ruta al directorio de la plantilla DEB.\n  -c, --compressed <archivo>   Ruta del archivo comprimido de Tahoma2D.\n  -h, --help                   Muestra esta ayuda y sale."
		[en_HELP]="\n\nUsage: $0 [options]\n\nOptions:\n  -p, --package <version>      Package version (e.g., 1.0.3).\n  -v, --version <version>      Program version (e.g., 2.3.1).\n  -t, --template <path>        Path to the DEB template directory.\n  -c, --compressed <file>     Path to the Tahoma2D compressed file.\n  -h, --help                   Shows this help and exits."
		[it_HELP]="\n\nUso: $0 [opzioni]\n\nOpzioni:\n  -p, --package <versione>     Versione del pacchetto (es. 1.0.3).\n  -v, --version <versione>     Versione del programma (es. 2.3.1).\n  -t, --template <percorso>    Percorso alla cartella del template DEB.\n  -c, --compressed <file>      Percorso al file compresso di Tahoma2D.\n  -h, --help                   Mostra questa guida e esce."
		[de_HELP]="\n\nVerwendung: $0 [Optionen]\n\nOptionen:\n  -p, --package <version>      Paketversion (z.B. 1.0.3).\n  -v, --version <version>      Programmversion (z.B. 2.3.1).\n  -t, --template <pfad>        Pfad zum DEB-Vorlagenordner.\n  -c, --compressed <datei>    Pfad zur komprimierten Tahoma2D-Datei.\n  -h, --help                   Zeigt diese Hilfe an und beendet."
		[fr_HELP]="\n\nUsage : $0 [options]\n\nOptions :\n  -p, --package <version>      Version du paquet (ex. 1.0.3).\n  -v, --version <version>      Version du programme (ex. 2.3.1).\n  -t, --template <chemin>      Chemin vers le répertoire du modèle DEB.\n  -c, --compressed <fichier>   Chemin du fichier compressé de Tahoma2D.\n  -h, --help                   Affiche cette aide et quitte."
		[gl_HELP]="\n\nUsar: $0 [opcións]\n\nOpcións:\n  -p, --package <versión>     Versión do paquete (ex. 1.0.3).\n  -v, --version <versión>     Versión do programa (ex. 2.3.1).\n  -t, --template <camiño>     Camiño á carpeta da plantilla DEB.\n  -c, --compressed <arquivo>  Camiño do ficheiro comprimido de Tahoma2D.\n  -h, --help                  Mostra esta axuda e sae."
		[pt_HELP]="\n\nUso: $0 [opções]\n\nOpções:\n  -p, --package <versão>      Versão do pacote (ex. 1.0.3).\n  -v, --version <versão>      Versão do programa (ex. 2.3.1).\n  -t, --template <caminho>    Caminho para o diretório do modelo DEB.\n  -c, --compressed <arquivo>  Caminho do arquivo comprimido do Tahoma2D.\n  -h, --help                  Exibe esta ajuda e sai."
    
		[es_INVALID_OPTION]="Parámetro inválido."
		[en_INVALID_OPTION]="Invalid parameter."
		[it_INVALID_OPTION]="Parametro invalido."
		[de_INVALID_OPTION]="Ungültige Option."
		[fr_INVALID_OPTION]="Paramètre invalide."
		[gl_INVALID_OPTION]="Parámetro inválido."
		[pt_INVALID_OPTION]="Opção inválida."
		
		[es_ERROR_COMPRESSED_NOT_EXISTS]="ERROR: El archivo comprimido '$compressed_file' no existe."
		[en_COMPLETE_WITH_PREVIOUS_TAG]=""
		[it_COMPLETE_WITH_PREVIOUS_TAG]=""
		[de_COMPLETE_WITH_PREVIOUS_TAG]=""
		[fr_COMPLETE_WITH_PREVIOUS_TAG]=""
		[gl_COMPLETE_WITH_PREVIOUS_TAG]=""
		[pt_COMPLETE_WITH_PREVIOUS_TAG]=""
		
		[es_ERROR_COMPRESSED_NOT_EXISTS]="ERROR: El archivo comprimido '$compressed_file' no existe."
		[en_ERROR_COMPRESSED_NOT_EXISTS]="ERROR: The compressed file '$compressed_file' does not exist."
		[it_ERROR_COMPRESSED_NOT_EXISTS]="ERRORE: Il file compresso '$compressed_file' non esiste."
		[de_ERROR_COMPRESSED_NOT_EXISTS]="FEHLER: Die komprimierte Datei '$compressed_file' existiert nicht."
		[fr_ERROR_COMPRESSED_NOT_EXISTS]="ERREUR : Le fichier compressé '$compressed_file' n'existe pas."
		[gl_ERROR_COMPRESSED_NOT_EXISTS]="ERRO: O arquivo comprimido '$compressed_file' non existe."
		[pt_ERROR_COMPRESSED_NOT_EXISTS]="ERRO: O arquivo compactado '$compressed_file' não existe."
		
		[es_ERROR_COMPRESSED_FILE]="ERROR: No se especificó la ruta del archivo comprimido con -c."
		[en_ERROR_COMPRESSED_FILE]="ERROR: The compressed file path was not specified with -c."
		[it_ERROR_COMPRESSED_FILE]="ERRORE: Il percorso del file compresso non è stato specificato con -c."
		[de_ERROR_COMPRESSED_FILE]="FEHLER: Der Pfad der komprimierten Datei wurde nicht mit -c angegeben."
		[fr_ERROR_COMPRESSED_FILE]="ERREUR : Le chemin du fichier compressé n'a pas été spécifié avec -c."
		[gl_ERROR_COMPRESSED_FILE]="ERRO: Non se especificou a ruta do arquivo comprimido con -c."
		[pt_ERROR_COMPRESSED_FILE]="ERRO: O caminho do arquivo compactado não foi especificado com -c."
		
		[es_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copiando desde '$deb_template_path' a '$final_folder'..."
		[en_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copying from '$deb_template_path' to '$final_folder'..."
		[it_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copia da '$deb_template_path' a '$final_folder'..."
		[de_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Kopiere von '$deb_template_path' nach '$final_folder'..."
		[fr_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copie de '$deb_template_path' vers '$final_folder'..."
		[gl_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copiando desde '$deb_template_path' a '$final_folder'..."
		[pt_COPY_DEB_TEMPLATE_TO_FINAL_FOLDER]="Copiando de '$deb_template_path' para '$final_folder'..."
		
		[es_ERROR_VAR_TEMPLATE]="ERROR: La plantilla '$deb_template_path' no existe o no es un directorio válido."
		[en_ERROR_VAR_TEMPLATE]="ERROR: The template '$deb_template_path' does not exist or is not a valid directory."
		[it_ERROR_VAR_TEMPLATE]="ERRORE: Il modello '$deb_template_path' non esiste o non è una directory valida."
		[de_ERROR_VAR_TEMPLATE]="FEHLER: Die Vorlage '$deb_template_path' existiert nicht oder ist kein gültiges Verzeichnis."
		[fr_ERROR_VAR_TEMPLATE]="ERREUR : Le modèle '$deb_template_path' n'existe pas ou n'est pas un répertoire valide."
		[gl_ERROR_VAR_TEMPLATE]="ERRO: A plantilla '$deb_template_path' non existe ou non é un directorio válido."
		[pt_ERROR_VAR_TEMPLATE]="ERRO: O modelo '$deb_template_path' não existe ou não é um diretório válido."
		
		[es_ERROR_DEB_TEMPLATE_VAR]="ERROR: La variable deb_template_path no está definida."
		[en_ERROR_DEB_TEMPLATE_VAR]="ERROR: The variable deb_template_path is not defined."
		[it_ERROR_DEB_TEMPLATE_VAR]="ERRORE: La variabile deb_template_path non è definita."
		[de_ERROR_DEB_TEMPLATE_VAR]="FEHLER: Die Variable deb_template_path ist nicht definiert."
		[fr_ERROR_DEB_TEMPLATE_VAR]="ERREUR : La variable deb_template_path n'est pas définie."
		[gl_ERROR_DEB_TEMPLATE_VAR]="ERRO: A variable deb_template_path non está definida."
		[pt_ERROR_DEB_TEMPLATE_VAR]="ERRO: A variável deb_template_path não está definida."
		
		[es_ERROR_VERSION_NUMBER_VAR]="ERROR: La variable version_number no está definida."
		[en_ERROR_VERSION_NUMBER_VAR]="ERROR: The variable version_number is not defined."
		[it_ERROR_VERSION_NUMBER_VAR]="ERRORE: La variabile version_number non è definita."
		[de_ERROR_VERSION_NUMBER_VAR]="FEHLER: Die Variable version_number ist nicht definiert."
		[fr_ERROR_VERSION_NUMBER_VAR]="ERREUR : La variable version_number n'est pas définie."
		[gl_ERROR_VERSION_NUMBER_VAR]="ERRO: A variable version_number non está definida."
		[pt_ERROR_VERSION_NUMBER_VAR]="ERRO: A variável version_number não está definida."
		
		[es_ERROR_TEMPLATE]="ERROR: No se encontró 'deb-template' en el directorio actual y no se especificó con -t."
		[en_ERROR_TEMPLATE]="ERROR: 'deb-template' was not found in the current directory and was not specified with -t."
		[it_ERROR_TEMPLATE]="ERRORE: 'deb-template' non è stato trovato nella directory corrente e non è stato specificato con -t."
		[de_ERROR_TEMPLATE]="FEHLER: 'deb-template' wurde im aktuellen Verzeichnis nicht gefunden und wurde nicht mit -t angegeben."
		[fr_ERROR_TEMPLATE]="ERREUR : 'deb-template' n'a pas été trouvé dans le répertoire actuel et n'a pas été spécifié avec -t."
		[gl_ERROR_TEMPLATE]="ERRO: Non se atopou 'deb-template' no directorio actual e non se especificou con -t."
		[pt_ERROR_TEMPLATE]="ERRO: 'deb-template' não foi encontrado no diretório atual e não foi especificado com -t."
      
		[es_DEB_TEMPLATE_NOT_FOUND]="Carpeta deb-template no encontrada. Proporcione su ruta completa: "
		[en_DEB_TEMPLATE_NOT_FOUND]="deb-template folder not found. Please provide its full path: "
		[it_DEB_TEMPLATE_NOT_FOUND]="Cartella deb-template non trovata. Si prega di fornire il percorso completo: "
		[de_DEB_TEMPLATE_NOT_FOUND]="Deb-Template-Ordner nicht gefunden. Bitte geben Sie den vollständigen Pfad an: "
		[fr_DEB_TEMPLATE_NOT_FOUND]="Dossier deb-template introuvable. Veuillez fournir son chemin complet : "
		[gl_DEB_TEMPLATE_NOT_FOUND]="Cartafol deb-template non atopado. Proporcione a ruta completa: "
		[pt_DEB_TEMPLATE_NOT_FOUND]="Pasta deb-template não encontrada. Por favor, forneça o caminho completo: "
		
		[es_CONTROL_FILE_NOT_FOUND]="Error: Archivo de control no encontrado!"
		[en_CONTROL_FILE_NOT_FOUND]="Error: Control file not found!"
		[it_CONTROL_FILE_NOT_FOUND]="Errore: File di controllo non trovato!"
		[de_CONTROL_FILE_NOT_FOUND]="Fehler: Steuerdatei nicht gefunden!"
		[fr_CONTROL_FILE_NOT_FOUND]="Erreur : Fichier de contrôle introuvable !"
		[gl_CONTROL_FILE_NOT_FOUND]="Erro: Arquivo de control non atopado!"
		[pt_CONTROL_FILE_NOT_FOUND]="Erro: Arquivo de controle não encontrado!"
		
		[es_NO_VERSION_FOUND_IN_CONTROL]="Error: No se encontró una versión en el archivo de control!"
		[en_NO_VERSION_FOUND_IN_CONTROL]="Error: No version found in control file!"
		[it_NO_VERSION_FOUND_IN_CONTROL]="Errore: Nessuna versione trovata nel file di controllo!"
		[de_NO_VERSION_FOUND_IN_CONTROL]="Fehler: Keine Version in der Steuerdatei gefunden!"
		[fr_NO_VERSION_FOUND_IN_CONTROL]="Erreur : Aucune version trouvée dans le fichier de contrôle !"
		[gl_NO_VERSION_FOUND_IN_CONTROL]="Erro: Non se atopou unha versión no arquivo de control!"
		[pt_NO_VERSION_FOUND_IN_CONTROL]="Erro: Nenhuma versão encontrada no arquivo de controle!"

		[es_PACKAGE_VERSION]="El paquete .deb se creará con la versión: $version_number."
		[en_PACKAGE_VERSION]="The .deb package will be created as version: $version_number."
		[it_PACKAGE_VERSION]="Il pacchetto .deb sarà creato con la versione: $version_number."
		[de_PACKAGE_VERSION]="Das .deb-Paket wird mit der Version $version_number erstellt."
		[fr_PACKAGE_VERSION]="Le paquet .deb sera créé avec la version : $version_number."
		[gl_PACKAGE_VERSION]="O paquete .deb crearase coa versión: $version_number."
		[pt_PACKAGE_VERSION]="O pacote .deb será criado com a versão: $version_number."
		
		[es_AUTOMATIC_CORRELATIVE]="(La versión del paquete es correlativa automática y no es la versión del programa)."
		[en_AUTOMATIC_CORRELATIVE]="(The package version is automatic correlative and is not the program version)."
		[it_AUTOMATIC_CORRELATIVE]="(La versione del pacchetto è correlativa automatica e non è la versione del programma)."
		[de_AUTOMATIC_CORRELATIVE]="(Die Paketversion ist automatisch fortlaufend und nicht die Programmversion)."
		[fr_AUTOMATIC_CORRELATIVE]="(La version du paquet est corrélative automatique et n'est pas la version du programme)."
		[gl_AUTOMATIC_CORRELATIVE]="(A versión do paquete é correlativa automática e non é a versión do programa)."
		[pt_AUTOMATIC_CORRELATIVE]="(A versão do pacote é correlativa automática e não é a versão do programa)."
		
		[es_CURRENT_PROGRAM_VERSION]="Versión actual del programa: $program_version"
		[en_CURRENT_PROGRAM_VERSION]="Current program version: $program_version"
		[it_CURRENT_PROGRAM_VERSION]="Versione attuale del programma: $program_version"
		[de_CURRENT_PROGRAM_VERSION]="Aktuelle Programmversion: $program_version"
		[fr_CURRENT_PROGRAM_VERSION]="Version actuelle du programme : $program_version"
		[gl_CURRENT_PROGRAM_VERSION]="Versión actual do programa: $program_version"
		[pt_CURRENT_PROGRAM_VERSION]="Versão atual do programa: $program_version"
		
		[es_KEPT_OR_CHANGE]="¿Mantener? (Presione Enter para mantener, escriba 'N' para cambiar): "
		[en_KEPT_OR_CHANGE]="Keep it? (Press Enter to keep, type 'N' to change): "
		[it_KEPT_OR_CHANGE]="Mantenere? (Premi Invio per mantenere, digita 'N' per cambiare): "
		[de_KEPT_OR_CHANGE]="Behalten? (Drücken Sie Enter, um zu behalten, geben Sie 'N' ein, um zu ändern): "
		[fr_KEPT_OR_CHANGE]="Le garder ? (Appuyez sur Entrée pour garder, tapez 'N' pour changer) : "
		[gl_KEPT_OR_CHANGE]="Mantelo? (Preme Enter para mantelo, escribe 'N' para cambiar): "
		[pt_KEPT_OR_CHANGE]="Manter? (Pressione Enter para manter, digite 'N' para alterar): "
		
		[es_NEW_VERSION]="¿Nueva versión? "
		[en_NEW_VERSION]="New version? "
		[it_NEW_VERSION]="Nuova versione? "
		[de_NEW_VERSION]="Neue Version? "
		[fr_NEW_VERSION]="Nouvelle version ? "
		[gl_NEW_VERSION]="Nova versión? "
		[pt_NEW_VERSION]="Nova versão? "
		
		[es_FINAL_PACKAGE_VERSION]="Versión final del paquete: $version_number"
		[en_FINAL_PACKAGE_VERSION]="Final package version: $version_number"
		[it_FINAL_PACKAGE_VERSION]="Versione finale del pacchetto: $version_number"
		[de_FINAL_PACKAGE_VERSION]="Endgültige Paketversion: $version_number"
		[fr_FINAL_PACKAGE_VERSION]="Version finale du paquet : $version_number"
		[gl_FINAL_PACKAGE_VERSION]="Versión final do paquete: $version_number"
		[pt_FINAL_PACKAGE_VERSION]="Versão final do pacote: $version_number"
		
		[es_FINAL_PROGRAM_VERSION]="Versión final del programa: $program_version"
		[en_FINAL_PROGRAM_VERSION]="Final program version: $program_version"
		[it_FINAL_PROGRAM_VERSION]="Versione finale del programma: $program_version"
		[de_FINAL_PROGRAM_VERSION]="Endgültige Programmversion: $program_version"
		[fr_FINAL_PROGRAM_VERSION]="Version finale du programme : $program_version"
		[gl_FINAL_PROGRAM_VERSION]="Versión final do programa: $program_version"
		[pt_FINAL_PROGRAM_VERSION]="Versão final do programa: $program_version"
		
		[es_CONTINUE]="¿Continuar? (Presiona Enter para confirmar, escribe 'N' para cancelar): "
		[en_CONTINUE]="Continue? (Press Enter to confirm, type 'N' to cancel): "
		[it_CONTINUE]="Continuare? (Premi Invio per confermare, digita 'N' per annullare): "
		[de_CONTINUE]="Fortfahren? (Drücken Sie Enter zur Bestätigung, geben Sie 'N' ein, um abzubrechen): "
		[fr_CONTINUE]="Continuer ? (Appuyez sur Entrée pour confirmer, tapez 'N' pour annuler) : "
		[gl_CONTINUE]="Continuar? (Preme Enter para confirmar, escribe 'N' para cancelar): "
		[pt_CONTINUE]="Continuar? (Pressione Enter para confirmar, digite 'N' para cancelar): "
		
		[es_RESTORING_CONTROL]="Restaurando el archivo original /DEBIAN/control..."
		[en_RESTORING_CONTROL]="Restoring original /DEBIAN/control file..."
		[it_RESTORING_CONTROL]="Ripristino del file /DEBIAN/control originale..."
		[de_RESTORING_CONTROL]="Stelle die originale /DEBIAN/control-Datei wieder her..."
		[fr_RESTORING_CONTROL]="Restauration du fichier original /DEBIAN/control..."
		[gl_RESTORING_CONTROL]="Restaurando o arquivo orixinal /DEBIAN/control..."
		[pt_RESTORING_CONTROL]="Restaurando o arquivo original /DEBIAN/control..."
		
		[es_ABORTED]="Abortado."
		[en_ABORTED]="Aborted."
		[it_ABORTED]="Annullato."
		[de_ABORTED]="Abgebrochen."
		[fr_ABORTED]="Abandonné."
		[gl_ABORTED]="Abandonado."
		[pt_ABORTED]="Abortado."
		
		[es_PATH_COMPRESSED]="Introduce la ruta del archivo comprimido de Tahoma2D: "
		[en_PATH_COMPRESSED]="Enter the path of the Tahoma2D compressed file: "
		[it_PATH_COMPRESSED]="Inserisci il percorso del file compresso di Tahoma2D: "
		[de_PATH_COMPRESSED]="Geben Sie den Pfad der komprimierten Tahoma2D-Datei ein: "
		[fr_PATH_COMPRESSED]="Entrez le chemin du fichier compressé de Tahoma2D : "
		[gl_PATH_COMPRESSED]="Introduce a ruta do arquivo comprimido de Tahoma2D: "
		[pt_PATH_COMPRESSED]="Digite o caminho do arquivo compactado do Tahoma2D: "
		
		[es_FOUND_COMPRESSED]="Archivo comprimido encontrado: $tahoma_compressed"
		[en_FOUND_COMPRESSED]="Found compressed file: $tahoma_compressed"
		[it_FOUND_COMPRESSED]="File compresso trovato: $tahoma_compressed"
		[de_FOUND_COMPRESSED]="Komprimierte Datei gefunden: $tahoma_compressed"
		[fr_FOUND_COMPRESSED]="Fichier compressé trouvé : $tahoma_compressed"
		[gl_FOUND_COMPRESSED]="Arquivo comprimido atopado: $tahoma_compressed"
		[pt_FOUND_COMPRESSED]="Arquivo compactado encontrado: $tahoma_compressed"
		
		[es_UNSUPPORTED_FILE_FORMAT]="Formato de archivo no compatible. Saliendo."
		[en_UNSUPPORTED_FILE_FORMAT]="Unsupported file format. Exiting."
		[it_UNSUPPORTED_FILE_FORMAT]="Formato di file non supportato. Uscendo."
		[de_UNSUPPORTED_FILE_FORMAT]="Nicht unterstütztes Dateiformat. Beende."
		[fr_UNSUPPORTED_FILE_FORMAT]="Format de fichier non supporté. Sortie."
		[gl_UNSUPPORTED_FILE_FORMAT]="Formato de arquivo non compatible. Saíndo."
		[pt_UNSUPPORTED_FILE_FORMAT]="Formato de arquivo não suportado. Saindo."
		
		[es_FAILED]="Error: No se pudo crear el paquete Debian."
		[en_FAILED]="Error: Failed to create Debian package."
		[it_FAILED]="Errore: Impossibile creare il pacchetto Debian."
		[de_FAILED]="Fehler: Debian-Paket konnte nicht erstellt werden."
		[fr_FAILED]="Erreur : Échec de la création du paquet Debian."
		[gl_FAILED]="Erro: Non se puido crear o paquete Debian."
		[pt_FAILED]="Erro: Não foi possível criar o pacote Debian."
		
		[es_COMPLETED]="Proceso completado con éxito."
		[en_COMPLETED]="Process completed successfully."
		[it_COMPLETED]="Processo completato con successo."
		[de_COMPLETED]="Prozess erfolgreich abgeschlossen."
		[fr_COMPLETED]="Processus terminé avec succès."
		[gl_COMPLETED]="Proceso completado con éxito."
		[pt_COMPLETED]="Processo concluído com sucesso."
	)

    local lang=$(echo $LANG | cut -d_ -f1)
    local message=${msg[${lang}_$key]:-${msg[en_$key]}}

    printf "%s" "$message"
}
