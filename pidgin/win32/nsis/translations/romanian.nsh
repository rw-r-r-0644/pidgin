;;
;;  romanian.nsh
;;
;;  Romanian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Mi�u Moldovan <dumol@gnome.ro>, (c) 2004 - 2005.
;;

; Startup Checks
!define INSTALLER_IS_RUNNING                     "Instalarea este deja pornit�."
!define PIDGIN_IS_RUNNING                  "O instan�� a programului Gaim este deja pornit�. �nchide�i-o �i �ncerca�i din nou."
!define GTK_INSTALLER_NEEDED			"Mediul GTK+ nu e prezent sau ave�i o versiune prea veche.$\rInstala�i cel pu�in versiunea v${GTK_MIN_VERSION} a mediului GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON                      "�nainte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT         "$(^Name) are licen�� GPL (GNU Public License). Licen�a este inclus� aici doar pentru scopuri informative. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client de mesagerie instant (obligatoriu)"
!define GTK_SECTION_TITLE			"Mediu GTK+ (obligatoriu)"
!define GTK_THEMES_SECTION_TITLE		"Teme GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"F�r� teme"
!define GTK_WIMP_SECTION_TITLE		"Tem� Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tem� Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tem� Light House Blue"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Scurt�turi"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Meniu Start"
!define PIDGIN_SECTION_DESCRIPTION		"Fi�iere Gaim �i dll-uri"
!define GTK_SECTION_DESCRIPTION		"Un mediu de dezvoltare multiplatform� utilizat de Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Temele GTK+ schimb� aparen�a aplica�iilor GTK+."
!define GTK_NO_THEME_DESC			"F�r� teme GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp este o tem� GTK+ ce imit� mediul grafic Windows."
!define GTK_BLUECURVE_THEME_DESC		"Tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Tema Lighthouseblue."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Scurt�turi pentru pornirea Gaim"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Creeaz� iconi�e Gaim pe Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Creeaz� o intrare Gaim �n meniul Start"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Ave�i o versiune veche a mediului GTK+. Dori�i s� o actualiza�i?$\rNot�: E posibil ca Gaim s� nu func�ioneze cu versiunea veche."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE               "Vizita�i pagina de web Windows Gaim"

; Gaim Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Gaim (doar dezinstalare)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Eroare la instalarea mediului GTK+."
!define GTK_BAD_INSTALL_PATH			"Directorul specificat nu poate fi accesat sau creat."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nu ave�i drepturile de acces necesare instal�rii unei teme GTK+."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Programul de dezinstalare nu a g�sit intr�ri Gaim �n regi�tri.$\rProbabil un alt utilizator a instalat aceast� aplica�ie."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Nu ave�i drepturile de acces necesare dezinstal�rii acestei aplica�ii."
