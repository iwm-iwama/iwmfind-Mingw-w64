:: 設定 ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cd %~dp0
	%~d0
	cls

	:: ファイル名はソースと同じ
	set fn=%~n0
	set exec=%fn%.exe
	set cc=gcc.exe
	set op_link=-O2 -lgdi32 -luser32 -lshlwapi
	set lib=lib_iwmutil.a sqlite3.a

	set src=%fn%.c

:: make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	echo --- Compile -S --------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -S
		wc -l %%~ns.s
	)
	echo.

	echo --- Make -----------------------------------------
	for %%s in (%src%) do (
		%cc% %%s -c -Wall %op_link%
	)
	%cc% *.o %lib% -o %exec% %op_link%
	echo %exec%

	:: 後処理
	strip %exec%
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
