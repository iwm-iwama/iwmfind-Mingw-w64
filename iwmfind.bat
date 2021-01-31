:: 設定 ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cd %~dp0
	%~d0
	cls

	:: ファイル名はソースと同じ
	set fn=%~n0
	set src=%fn%.c
	set exec=%fn%.exe
	set cc=gcc.exe
	set lib=lib_iwmutil.a sqlite3.a
	set option=-O2 -Wall -lgdi32 -luser32 -lshlwapi

:: make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	echo --- Compile -S ------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -S %option%
		wc -l %%~ns.s
	)
	echo.

	echo --- Make ------------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -g -c %option%
		objdump -S -d %%~ns.o > %%~ns.objdump.txt
	)
	%cc% *.o %lib% -o %exec% %option%
	echo %exec%

	:: 後処理
	strip -s %exec%
	rm *.o

	:: 失敗
	if not exist "%exec%" goto end

	:: 成功
	echo.
	pause

:: テスト ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	cls
	set tm1=%time%
	echo [%tm1%]

	%exec% . -r

	echo.
	echo [%tm1%]
	echo [%time%]

:: 終了 ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:end
	echo.
	pause
	exit
