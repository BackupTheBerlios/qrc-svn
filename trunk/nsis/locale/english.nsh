;;
;;  english.nsh
;;
;;  Default language strings for the Windows gaim-qrc NSIS installer.
;;  Windows Code page: 1252
;;

; Startup Gaim check
LangString GAIM_NEEDED ${LANG_ENGLISH} "Gaim-GayM requires that Gaim be installed. You must install Gaim (http://gaim.sourceforge.net/win32/index.php) before installing Gaim-GayM."

LangString GAYM_TITLE ${LANG_ENGLISH} "Gaim-GayM plugin for Gaim"

LangString NO_GAIM_VERSION ${LANG_ENGLISH} "Unable to determine installed Gaim version."

LangString BAD_GAIM_VERSION_1 ${LANG_ENGLISH} "Incompatible version.$\r$\n$\r$\nThis version of the Gaim-GayM plugin was built for Gaim version ${GAIM_VERSION}.  It appears that you have Gaim version"

LangString BAD_GAIM_VERSION_2 ${LANG_ENGLISH} "installed.$\r$\n$\r$\nSee http://developer.berlios.de/projects/qrc/ for more information."

; Overrides for default text in windows:

LangString WELCOME_TITLE ${LANG_ENGLISH} "Gaim-GayM v${GAYM_VERSION} Installer"
LangString WELCOME_TEXT  ${LANG_ENGLISH} "Note: This version of the plugin is designed for Gaim ${GAIM_VERSION}, and will not install or function with other versions of Gaim.\r\n\r\nWhen you upgrade your version of Gaim, you must uninstall or upgrade this plugin as well.\r\n\r\n"

LangString DIR_SUBTITLE ${LANG_ENGLISH} "Please locate the directory where Gaim is installed"
LangString DIR_INNERTEXT ${LANG_ENGLISH} "Install in this Gaim folder:"

LangString FINISH_TITLE ${LANG_ENGLISH} "Gaim-GayM v${GAYM_VERSION} Install Complete"
LangString FINISH_TEXT ${LANG_ENGLISH} "You will need to restart Gaim for the plugin to be loaded."

; during install uninstaller
LangString GAYM_PROMPT_WIPEOUT ${LANG_ENGLISH} "The libgaym.dll plugin is about to be deleted from your Gaim/plugins directory.  Continue?"

; for windows uninstall
LangString GAYM_UNINSTALL_DESC ${LANG_ENGLISH} "Gaim-GayM Plugin (remove only)"
LangString un.GAYM_UNINSTALL_ERROR_1 ${LANG_ENGLISH} "The uninstaller could not find registry entries for Gaim-GayM.$\rIt is likely that another user installed the plugin."
LangString un.GAYM_UNINSTALL_ERROR_2 ${LANG_ENGLISH} "You do not have the permissions necessary to uninstall the plugin."



