:: Ini ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cls

	:: �t�@�C�����̓\�[�X�Ɠ���
	set fn=%~n0
	set src=%fn%.c
	set exec=%fn%.exe
	set cc=gcc.exe
	set lib=lib_iwmutil.a sqlite3.a
	:: 2021-09-23����
	::   -Os �̕��� -O2 �����肵�đ����X������
	set option=-Os -Wall -lgdi32 -luser32 -lshlwapi

:: Make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	echo --- Compile -S ------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -S %option%
		echo %%~ns.s
	)
	echo.

	echo --- Make ------------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -g -c %option%
		objdump -S -d %%~ns.o > %%~ns.objdump.txt
	)
	%cc% *.o %lib% -o %exec% %option%
	echo %exec%
	echo.

	:: �㏈��
	strip %exec%
	rm *.o

	:: ���s
	if not exist "%exec%" goto end

	:: ����
	echo.
	pause

:: Test ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	cls
	set t=%time%
	echo [%t%]

	%exec% . -r

	echo [%t%]
	echo [%time%]

:: Quit ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:end
	echo.
	pause
	exit
