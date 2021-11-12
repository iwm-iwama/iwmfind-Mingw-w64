#!ruby.exe
#coding:utf-8

#--------------------------------------------------------------------------------------------------
# 2021-08-25
# ＜問題＞
#   iwmfind.exe による出力(Shift_JIS)だと、ファイル名に含まれるUnicode文字が化けてしまう。
# ＜対応＞
#   iwmfind.exe で作成された Sqlite3 DBファイルから、パス名などを UTF-8 で抽出する。
#   ファイルの一括削除などを行う場合、カスタマイズして利用されたい。
#--------------------------------------------------------------------------------------------------

# TmpDB
$db = "#{Time.now.strftime("%Y%m%d_%H%M%S_%L")}.db"
$iCnt = 0

# (例)
#   カレントフォルダ以下、24時間以内に更新されたファイル名を抽出
print %x(./iwmfind.exe "." -r -nh -nf -o="#{$db}" -w="type like 'f' and mtime>=[-24h]")

# sqlite3.exe から直接DBファイルを操作
%x(./sqlite3.exe -separator "\t" "#{$db}" "select mtime,path from V_INDEX order by mtime;").split("\n") do
	|ln|
	ln.strip!

	$iCnt += 1
	puts "#{$iCnt}\t#{ln}"

	#
	# 本処理をここに書く
	# (例) puts ln.split("\t")[1]
	#
end

# TmpDB 削除
File.delete $db
