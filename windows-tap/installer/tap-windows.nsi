; ****************************************************************************
; * Copyright (C) 2002-2010 OpenVPN Technologies, Inc.                       *
; * Copyright (C)      2012 Alon Bar-Lev <alon.barlev@gmail.com>             *
; *  This program is free software; you can redistribute it and/or modify    *
; *  it under the terms of the GNU General Public License version 2          *
; *  as published by the Free Software Foundation.                           *
; ****************************************************************************

; TAP-Windows install script for Windows, using NSIS

SetCompressor lzma

!include "MUI.nsh"
!include "StrFunc.nsh"
!include "x64.nsh"
!define MULTIUSER_EXECUTIONLEVEL Admin
!include "MultiUser.nsh"
!include FileFunc.nsh
!insertmacro GetParameters
!insertmacro GetOptions

${StrLoc}

;--------------------------------
;Configuration

;General

OutFile "${OUTPUT}"

ShowInstDetails show
ShowUninstDetails show

;Remember install folder
InstallDirRegKey HKLM "SOFTWARE\${PRODUCT_NAME}" ""

;--------------------------------
;Modern UI Configuration

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"

!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of ${PRODUCT_NAME}, a kernel driver to provide virtual tap device functionality on Windows originally written by James Yonan.\r\n\r\nNote that the Windows version of ${PRODUCT_NAME} will only run on Windows XP or later.\r\n\r\n\r\n"

!define MUI_COMPONENTSPAGE_TEXT_TOP "Select the components to install/upgrade.  Stop any ${PRODUCT_NAME} processes or the ${PRODUCT_NAME} service if it is running.  All DLLs are installed locally."

!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_ABORTWARNING
!define MUI_ICON "icon.ico"
!define MUI_UNICON "icon.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "install-whirl.bmp"
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${IMAGE}\license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES  
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Language Strings

LangString DESC_SecTAP ${LANG_ENGLISH} "Install/Upgrade the TAP virtual device driver.  Will not interfere with CIPE."
LangString DESC_SecTAPUtilities ${LANG_ENGLISH} "Install the TAP Utilities."
LangString DESC_SecTAPSDK ${LANG_ENGLISH} "Install the TAP SDK."

;--------------------------------
;Reserve Files

;Things that need to be extracted on first (keep these lines before any File command!)
;Only useful for BZIP2 compression

ReserveFile "install-whirl.bmp"

;--------------------------------
;Macros

!macro SelectByParameter SECT PARAMETER DEFAULT
	${GetOptions} $R0 "/${PARAMETER}=" $0
	${If} ${DEFAULT} == 0
		${If} $0 == 1
			!insertmacro SelectSection ${SECT}
		${EndIf}
	${Else}
		${If} $0 != 0
			!insertmacro SelectSection ${SECT}
		${EndIf}
	${EndIf}
!macroend

;--------------------------------
;Installer Sections

Section /o "TAP Virtual Ethernet Adapter" SecTAP

	SetOverwrite on

	${If} ${RunningX64}
		DetailPrint "We are running on a 64-bit system."

		SetOutPath "$INSTDIR\bin"
		File "${DEVCON64}"

		SetOutPath "$INSTDIR\driver"
		File "${IMAGE}\amd64\OemWin2k.inf"
		File "${IMAGE}\amd64\${PRODUCT_TAP_WIN_COMPONENT_ID}.cat"
		File "${IMAGE}\amd64\${PRODUCT_TAP_WIN_COMPONENT_ID}.sys"
	${Else}
		DetailPrint "We are running on a 32-bit system."

		SetOutPath "$INSTDIR\bin"
		File "${DEVCON32}"

		SetOutPath "$INSTDIR\driver"
		File "${IMAGE}\i386\OemWin2k.inf"
		File "${IMAGE}\i386\${PRODUCT_TAP_WIN_COMPONENT_ID}.cat"
		File "${IMAGE}\i386\${PRODUCT_TAP_WIN_COMPONENT_ID}.sys"
	${EndIf}
SectionEnd

Section /o "TAP Utilities" SecTAPUtilities
	SetOverwrite on

	# Delete previous start menu
	RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

	FileOpen $R0 "$INSTDIR\bin\addtap.bat" w
	FileWrite $R0 "rem Add a new TAP virtual ethernet adapter$\r$\n"
	FileWrite $R0 '"$INSTDIR\bin\${DEVCON_BASENAME}" install "$INSTDIR\driver\OemWin2k.inf" ${PRODUCT_TAP_WIN_COMPONENT_ID}$\r$\n'
	FileWrite $R0 "pause$\r$\n"
	FileClose $R0

	FileOpen $R0 "$INSTDIR\bin\deltapall.bat" w
	FileWrite $R0 "echo WARNING: this script will delete ALL TAP virtual adapters (use the device manager to delete adapters one at a time)$\r$\n"
	FileWrite $R0 "pause$\r$\n"
	FileWrite $R0 '"$INSTDIR\bin\${DEVCON_BASENAME}" remove ${PRODUCT_TAP_WIN_COMPONENT_ID}$\r$\n'
	FileWrite $R0 "pause$\r$\n"
	FileClose $R0

	; Create shortcuts
	CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\Utilities"
	CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Utilities\Add a new TAP virtual ethernet adapter.lnk" "$INSTDIR\bin\addtap.bat" ""
	CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Utilities\Delete ALL TAP virtual ethernet adapters.lnk" "$INSTDIR\bin\deltapall.bat" ""
SectionEnd

Section /o "TAP SDK" SecTAPSDK
	SetOverwrite on
	SetOutPath "$INSTDIR\include"
	File "${IMAGE}\include\tap-windows.h"
SectionEnd

Function .onInit
	${GetParameters} $R0
	ClearErrors

	!insertmacro SelectByParameter ${SecTAP} SELECT_TAP 1
	!insertmacro SelectByParameter ${SecTAPUtilities} SELECT_UTILITIES 0 
	!insertmacro SelectByParameter ${SecTAPSDK} SELECT_SDK 0

	!insertmacro MULTIUSER_INIT
	SetShellVarContext all

	${If} ${RunningX64}
		SetRegView 64
		StrCpy $INSTDIR "$PROGRAMFILES64\${PRODUCT_NAME}"
	${Else}
		StrCpy $INSTDIR "$PROGRAMFILES\${PRODUCT_NAME}"
	${EndIf}
FunctionEnd

;--------------------------------
;Dependencies

Function .onSelChange
	${If} ${SectionIsSelected} ${SecTAPUtilities}
		!insertmacro SelectSection ${SecTAP}
	${EndIf}
FunctionEnd

;--------------------
;Post-install section

Section -post

	SetOverwrite on

	; Store README, license, icon
	SetOverwrite on
	SetOutPath $INSTDIR
	File "${IMAGE}\license.txt"
	File "icon.ico"

	${If} ${SectionIsSelected} ${SecTAP}
		;
		; install/upgrade TAP driver if selected, using devcon
		;
		; TAP install/update was selected.
		; Should we install or update?
		; If tapinstall error occurred, $R5 will
		; be nonzero.
		IntOp $R5 0 & 0
		nsExec::ExecToStack '"$INSTDIR\bin\${DEVCON_BASENAME}" hwids ${PRODUCT_TAP_WIN_COMPONENT_ID}'
		Pop $R0 # return value/error/timeout
		IntOp $R5 $R5 | $R0
		DetailPrint "${DEVCON_BASENAME} hwids returned: $R0"

		; If tapinstall output string contains "${PRODUCT_TAP_WIN_COMPONENT_ID}" we assume
		; that TAP device has been previously installed,
		; therefore we will update, not install.
		Push "${PRODUCT_TAP_WIN_COMPONENT_ID}"
		Push ">"
		Call StrLoc
		Pop $R0

		${If} $R5 == 0
			${If} $R0 == ""
				StrCpy $R1 "install"
			${Else}
				StrCpy $R1 "update"
			${EndIf}
			DetailPrint "TAP $R1 (${PRODUCT_TAP_WIN_COMPONENT_ID}) (May require confirmation)"
			nsExec::ExecToLog '"$INSTDIR\bin\${DEVCON_BASENAME}" $R1 "$INSTDIR\driver\OemWin2k.inf" ${PRODUCT_TAP_WIN_COMPONENT_ID}'
			Pop $R0 # return value/error/timeout
			${If} $R0 == ""
				IntOp $R0 0 & 0
				SetRebootFlag true
				DetailPrint "REBOOT flag set"
			${EndIf}
			IntOp $R5 $R5 | $R0
			DetailPrint "${DEVCON_BASENAME} returned: $R0"
		${EndIf}

		DetailPrint "${DEVCON_BASENAME} cumulative status: $R5"
		${If} $R5 != 0
			MessageBox MB_OK "An error occurred installing the TAP device driver."
		${EndIf}

		; Store install folder in registry
		WriteRegStr HKLM SOFTWARE\${PRODUCT_NAME} "" $INSTDIR
	${EndIf}

	; Create uninstaller
	WriteUninstaller "$INSTDIR\Uninstall.exe"

	; Show up in Add/Remove programs
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME} ${PRODUCT_VERSION}"
	WriteRegExpandStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\icon.ico"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"

	; Advise a reboot
	;Messagebox MB_OK "IMPORTANT: Rebooting the system is advised in order to finalize TAP driver installation/upgrade (this is an informational message only, pressing OK will not reboot)."

SectionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecTAP} $(DESC_SecTAP)
!insertmacro MUI_DESCRIPTION_TEXT ${SecTAPUtilities} $(DESC_SecTAPUtilities)
!insertmacro MUI_DESCRIPTION_TEXT ${SecTAPSDK} $(DESC_SecTAPSDK)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Function un.onInit
	ClearErrors
	!insertmacro MULTIUSER_UNINIT
	SetShellVarContext all
	${If} ${RunningX64}
		SetRegView 64
	${EndIf}
FunctionEnd

Section "Uninstall"
	DetailPrint "TAP REMOVE"
	nsExec::ExecToLog '"$INSTDIR\bin\${DEVCON_BASENAME}" remove ${PRODUCT_TAP_WIN_COMPONENT_ID}'
	Pop $R0 # return value/error/timeout
	DetailPrint "${DEVCON_BASENAME} remove returned: $R0"

	Delete "$INSTDIR\bin\${DEVCON_BASENAME}"
	Delete "$INSTDIR\bin\addtap.bat"
	Delete "$INSTDIR\bin\deltapall.bat"

	Delete "$INSTDIR\driver\OemWin2k.inf"
	Delete "$INSTDIR\driver\${PRODUCT_TAP_WIN_COMPONENT_ID}.cat"
	Delete "$INSTDIR\driver\${PRODUCT_TAP_WIN_COMPONENT_ID}.sys"

	Delete "$INSTDIR\include\tap-windows.h"

	Delete "$INSTDIR\icon.ico"
	Delete "$INSTDIR\license.txt"
	Delete "$INSTDIR\Uninstall.exe"

	RMDir "$INSTDIR\bin"
	RMDir "$INSTDIR\driver"
	RMDir "$INSTDIR\include"
	RMDir "$INSTDIR"
	RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

	DeleteRegKey HKLM "SOFTWARE\${PRODUCT_NAME}"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

SectionEnd
