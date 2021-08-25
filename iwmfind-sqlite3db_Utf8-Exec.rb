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

$db = "#{Time.now.strftime("%Y%m%d_%H%M%S_%L")}.db"
$iCnt = 0

# (例)
#   D:フォルダ以下、24時間以内に更新されたファイル名を抽出
%x(iwmfind.exe "D:" -r -nh -nf -o="#{$db}" -w="type like 'f' and mtime>=[-24h]")

print "                    \r"

%x(sqlite3.exe "#{$db}" "select mtime||'\t'||path from V_INDEX order by mtime;").split("\n") do
	|ln|
	ln.strip!

	$iCnt += 1
	puts "#{$iCnt}\t#{ln}"
	#
	# 本処理をここに書く
	# (例) File.delete ln
	#
end

File.delete $db
