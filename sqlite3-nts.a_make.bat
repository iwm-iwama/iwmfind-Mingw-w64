:: Ver.2026-07-03

@echo off
cls

set cc=gcc.exe -std=c23

set lib_src=sqlite3.c
set o_out=sqlite3.o
set lib_out=sqlite3-nts.a

:R20
	:: 1.ゼロクリア（データ完全抹消）            : -DSQLITE_SECURE_DELETE=1
	:: 2.シングルスレッド専用（Non-ThreadSafe化）: -DSQLITE_THREADSAFE=0
	:: 3.一時ファイルもメモリ上だけで完結させる  : -DSQLITE_TEMP_STORE=3
	:: 4.ログファイル（WAL）へのデータ残留を防ぐ : -DSQLITE_OMIT_WAL=1
	%cc% %lib_src% -o %o_out% -c -Os -DSQLITE_API= -DSQLITE_THREADSAFE=0 -DSQLITE_SECURE_DELETE=1 -DSQLITE_TEMP_STORE=3 -DSQLITE_OMIT_WAL=1
	ar rcs %lib_out% %o_out%
	rm %o_out%
	ls -la %lib_out%

:R99
	echo.
	pause
