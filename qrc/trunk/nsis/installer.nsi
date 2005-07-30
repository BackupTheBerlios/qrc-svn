; NSIS Script for the Gaim-QRC Plugins
; Uses NSIS v2.0

Name "Gaim-QRC ${QRC_VERSION}"

; Registry keys:
!define QRC_REG_KEY        "SOFTWARE\gaim-qrc"
!define QRC_UNINSTALL_KEY  "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\gaim-qrc"
!define QRC_UNINST_EXE     "gaim-qrc-uninst.exe"
!define GAYM_DLL           "libgaym.dll"
!define BOT_CHALLENGER_DLL "libbot-challenger.dll"
!define GAYM_EXTRAS_DLL	   "libgaym-extras.dll"
!define GAYM_PNG           "gaym.png"
!define QRC_UNINSTALL_LNK  "Gaim-QRC Uninstall.lnk"
!define	ALPHA_PNG	   "alpha.png"
!define	ENTRY_PNG	   "entry.png"
!define	PIC_PNG		    "pic.png"
!include "MUI.nsh"

;Do A CRC Check
CRCCheck On

;Output File Name
OutFile "..\gaim-${GAIM_VERSION}-qrc-${QRC_VERSION}.exe"

ShowInstDetails show
ShowUnInstDetails show
SetCompressor lzma

; Translations
!include "locale\english.nsh"

; Gaim Plugin installer helper stuff

!addincludedir "${GAIM_TOP}\src\win32\nsis"
!include "${GAIM_TOP}\src\win32\nsis\gaim-plugin.nsh"

; Modern UI Configuration
!define MUI_HEADERIMAGE

; Pages
!define MUI_WELCOMEPAGE_TITLE $(WELCOME_TITLE)
!define MUI_WELCOMEPAGE_TEXT $(WELCOME_TEXT)
!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE  "..\COPYING"

!define MUI_DIRECTORYPAGE_TEXT_TOP $(DIR_SUBTITLE)
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION $(DIR_INNERTEXT)
!insertmacro MUI_PAGE_DIRECTORY

!define MUI_FINISHPAGE_NOAUTOCLOSE
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE $(FINISH_TITLE)
!define MUI_FINISHPAGE_TEXT $(FINISH_TEXT)
!insertmacro MUI_PAGE_FINISH

; MUI Config

!define MUI_CUSTOMFUNCTION_GUIINIT qrc_checkGaimVersion
!define MUI_ABORTWARNING
!define MUI_UNINSTALLER
!define MUI_PROGRESSBAR smooth
!define MUI_INSTALLCOLORS /windows

!insertmacro MUI_LANGUAGE "English"

!define MUI_LICENSEPAGE_RADIOBUTTONS


;The Default Installation Directory
InstallDir "$PROGRAMFILES\gaim"
InstallDirRegKey HKLM SOFTWARE\gaim ""

Section -SecUninstallOldPlugin
  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  StrCmp $R0 "HKLM" rights_hklm
  StrCmp $R0 "HKCU" rights_hkcu done

  rights_hkcu:
      ReadRegStr $R1 HKCU "${QRC_REG_KEY}" ""
      ReadRegStr $R2 HKCU "${QRC_REG_KEY}" "Version"
      ReadRegStr $R3 HKCU "${QRC_UNINSTALL_KEY}" "UninstallString"
      Goto try_uninstall

  rights_hklm:
      ReadRegStr $R1 HKLM "${QRC_REG_KEY}" ""
      ReadRegStr $R2 HKLM "${QRC_REG_KEY}" "Version"
      ReadRegStr $R3 HKLM "${QRC_UNINSTALL_KEY}" "UninstallString"

  ; If previous version exists .. remove
  try_uninstall:
    StrCmp $R1 "" done
      StrCmp $R2 "" uninstall_problem
        IfFileExists $R3 0 uninstall_problem
          ; Have uninstall string.. go ahead and uninstall.
          SetOverwrite on
          ; Need to copy uninstaller outside of the install dir
          ClearErrors
          CopyFiles /SILENT $R3 "$TEMP\${QRC_UNINST_EXE}"
          SetOverwrite off
          IfErrors uninstall_problem
            ; Ready to uninstall..
            ClearErrors
            ExecWait '"$TEMP\${QRC_UNINST_EXE}" /S _?=$R1'
            IfErrors exec_error
              Delete "$TEMP\${QRC_UNINST_EXE}"
              Goto done

            exec_error:
              Delete "$TEMP\${QRC_UNINST_EXE}"
              Goto uninstall_problem

        uninstall_problem:
            ; Just delete the plugin and uninstaller, and remove Registry key
             MessageBox MB_YESNO $(QRC_PROMPT_WIPEOUT) IDYES do_wipeout IDNO cancel_install
          cancel_install:
            Quit

          do_wipeout:
            StrCmp $R0 "HKLM" del_lm_reg del_cu_reg
            del_cu_reg:
              DeleteRegKey HKCU ${QRC_REG_KEY}
              Goto uninstall_prob_cont
            del_lm_reg:
              DeleteRegKey HKLM ${QRC_REG_KEY}

            uninstall_prob_cont:
              ; plugin DLL
              Delete "$R1\plugins\${GAYM_DLL}"
              Delete "$R1\plugins\${BOT_CHALLENGER_DLL}"
              Delete "$R1\plugins\${GAYM_EXTRAS_DLL}"
              ; pixmaps
	      Delete "$R1\pixmaps\${ALPHA_PNG}"
	      Delete "$R1\pixmaps\${ENTRY_PNG}"
	      Delete "$R1\pixmaps\${PIC_PNG}"
              Delete "$R1\pixmaps\gaim\status\default\${GAYM_PNG}"
              Delete "$R3"

  done:

SectionEnd


Section "Install"
  Call CheckUserInstallRights
  Pop $R0

  StrCmp $R0 "NONE" instrights_none
  StrCmp $R0 "HKLM" instrights_hklm instrights_hkcu

  instrights_hklm:
    ; Write the version registry keys:
    WriteRegStr HKLM ${QRC_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKLM ${QRC_REG_KEY} "Version" "${QRC_VERSION}"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM ${QRC_UNINSTALL_KEY} "DisplayName" "$(QRC_UNINSTALL_DESC)"
    WriteRegStr HKLM ${QRC_UNINSTALL_KEY} "UninstallString" "$INSTDIR\${QRC_UNINST_EXE}"
    SetShellVarContext "all"
    Goto install_files

  instrights_hkcu:
    ; Write the version registry keys:
    WriteRegStr HKCU ${QRC_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKCU ${QRC_REG_KEY} "Version" "${QRC_VERSION}"

    ; Write the uninstall keys for Windows
    WriteRegStr HKCU ${QRC_UNINSTALL_KEY} "DisplayName" "$(QRC_UNINSTALL_DESC)"
    WriteRegStr HKCU ${QRC_UNINSTALL_KEY} "UninstallString" "$INSTDIR\${QRC_UNINST_EXE}"
    Goto install_files
  
  instrights_none:
    ; No registry keys for us...
    
  install_files:
    SetOutPath "$INSTDIR\plugins"
    SetCompress Auto
    SetOverwrite on
    File "..\gaym\src\.libs\${GAYM_DLL}"
    File "..\gaym-extras\src\.libs\${GAYM_EXTRAS_DLL}"
    File "..\bot-challenger\.libs\${BOT_CHALLENGER_DLL}"
    
    SetOutPath "$INSTDIR\pixmaps\gaim\status\default"
    File "..\gaym\pixmaps\${GAYM_PNG}"
    	
    SetOutPath "$INSTDIR\pixmaps"
    File "..\gaym-extras\pixmaps\${ALPHA_PNG}"
    File "..\gaym-extras\pixmaps\${ENTRY_PNG}"
    File "..\gaym-extras\pixmaps\${PIC_PNG}"
    StrCmp $R0 "NONE" done
    CreateShortCut "$SMPROGRAMS\Gaim\${QRC_UNINSTALL_LNK}" "$INSTDIR\${QRC_UNINST_EXE}"
    WriteUninstaller "$INSTDIR\${QRC_UNINST_EXE}"
    SetOverWrite off

  done:
SectionEnd

Section Uninstall
  Call un.CheckUserInstallRights
  Pop $R0
  StrCmp $R0 "NONE" no_rights
  StrCmp $R0 "HKCU" try_hkcu try_hklm

  try_hkcu:
    ReadRegStr $R0 HKCU "${QRC_REG_KEY}" ""
    StrCmp $R0 $INSTDIR 0 cant_uninstall
      ; HKCU install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKCU "${QRC_REG_KEY}"
      DeleteRegKey HKCU "${QRC_UNINSTALL_KEY}"
      Goto cont_uninstall

  try_hklm:
    ReadRegStr $R0 HKLM "${QRC_REG_KEY}" ""
    StrCmp $R0 $INSTDIR 0 try_hkcu
      ; HKLM install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKLM "${QRC_REG_KEY}"
      DeleteRegKey HKLM "${QRC_UNINSTALL_KEY}"
      ; Sets start menu and desktop scope to all users..
      SetShellVarContext "all"

  cont_uninstall:
    ; plugin 
    Delete "$INSTDIR\plugins\${GAYM_DLL}"
    Delete "$INSTDIR\plugins\${BOT_CHALLENGER_DLL}"
    ; pixmaps
    Delete "$INSTDIR\pixmaps\gaim\status\default\${GAYM_PNG}"
    Delete "$INSTDIR\pixmaps\${ALPHA_PNG}"
    Delete "$INSTDIR\pixmaps\${ENTRY_PNG}"
    Delete "$INSTDIR\pixmaps\${PIC_PNG}"
    ; uninstaller
    Delete "$INSTDIR\${QRC_UNINST_EXE}"
    ; uninstaller shortcut
    Delete "$SMPROGRAMS\Gaim\${QRC_UNINSTALL_LNK}"
    
    ; try to delete the Gaim directories, in case it has already uninstalled
    RMDir "$INSTDIR\plugins"
    RMDir "$INSTDIR"
    RMDir "$SMPROGRAMS\Gaim"

    Goto done

  cant_uninstall:
    MessageBox MB_OK $(un.QRC_UNINSTALL_ERROR_1) IDOK
    Quit

  no_rights:
    MessageBox MB_OK $(un.QRC_UNINSTALL_ERROR_2) IDOK
    Quit

  done:
SectionEnd

Function .onVerifyInstDir
  IfFileExists $INSTDIR\gaim.exe Good1
    Abort
  Good1:
FunctionEnd

Function qrc_checkGaimVersion
  Push $R0

  Push ${GAIM_VERSION}
  Call CheckGaimVersion
  Pop $R0

  StrCmp $R0 ${GAIM_VERSION_OK} qrc_checkGaimVersion_OK
  StrCmp $R0 ${GAIM_VERSION_INCOMPATIBLE} +1 +6
    Call GetGaimVersion
    IfErrors +3
    Pop $R0
    MessageBox MB_OK|MB_ICONSTOP "$(BAD_GAIM_VERSION_1) $R0 $(BAD_GAIM_VERSION_2)"
    goto +2
    MessageBox MB_OK|MB_ICONSTOP "$(NO_GAIM_VERSION)"
    Quit

  qrc_checkGaimVersion_OK:
  Pop $R0
FunctionEnd

Function CheckUserInstallRights
        ClearErrors
        UserInfo::GetName
        IfErrors Win9x
        Pop $0
        UserInfo::GetAccountType
        Pop $1

        StrCmp $1 "Admin" 0 +3
                StrCpy $1 "HKLM"
                Goto done
        StrCmp $1 "Power" 0 +3
                StrCpy $1 "HKLM"
                Goto done
        StrCmp $1 "User" 0 +3
                StrCpy $1 "HKCU"
                Goto done
        StrCmp $1 "Guest" 0 +3
                StrCpy $1 "NONE"
                Goto done
        ; Unknown error
        StrCpy $1 "NONE"
        Goto done

        Win9x:
                StrCpy $1 "HKLM"

        done:
        Push $1
FunctionEnd

Function un.CheckUserInstallRights
        ClearErrors
        UserInfo::GetName
        IfErrors Win9x
        Pop $0
        UserInfo::GetAccountType
        Pop $1
        StrCmp $1 "Admin" 0 +3
                StrCpy $1 "HKLM"
                Goto done
        StrCmp $1 "Power" 0 +3
                StrCpy $1 "HKLM"
                Goto done
        StrCmp $1 "User" 0 +3
                StrCpy $1 "HKCU"
                Goto done
        StrCmp $1 "Guest" 0 +3
                StrCpy $1 "NONE"
                Goto done
        ; Unknown error
        StrCpy $1 "NONE"
        Goto done

        Win9x:
                StrCpy $1 "HKLM"

        done:
        Push $1
FunctionEnd


