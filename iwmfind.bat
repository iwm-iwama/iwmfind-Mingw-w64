:: Ini ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cls

	:: �t�@�C�����̓\�[�X�Ɠ���
	set fn=%~n0
	set src=%fn%.c
	set fn_exe=%fn%.exe
	set cc=gcc.exe
	set cc_op=-Os -Wall -lgdi32 -luser32 -lshlwapi
	set lib=lib_iwmutil2.a sqlite3_mini.a

:: Make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::	echo --- Compile -S ------------------------------------
::	for %%s in (%src%) do (
::		%cc% %%s -S %cc_op%
::		echo %%~ns.s
::	)
::	echo.

	echo --- Make ------------------------------------------
	for %%s in (%src%) do (
		echo %%s
		%cc% %%s -c -Wall %cc_op%
	)
	%cc% *.o %lib% -o %fn_exe% %cc_op%
	echo.

	:: �㏈��
	strip %fn_exe%
	rm *.o

	:: ���s
	if not exist "%fn_exe%" goto end

	:: ����
	echo.
	pause

:: Test ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	cls

	%fn_exe% . -r

:: Quit ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:end
	echo.
	pause
	exit
