//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmfind5_20230918"
#define   IWM_COPYRIGHT       "Copyright (C)2009-2023 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"
#include "sqlite3.h"

INT       main();
WS        *sql_escape(WS *pW);
VOID      ifind10($struct_iFinfoW *FI, WIN32_FIND_DATAW F, WS *dir, INT depth);
VOID      ifind21(WS *dir, INT dirId);
VOID      ifind22($struct_iFinfoW *FI, INT dirId, WS *fname);
VOID      sql_exec(sqlite3 *db, CONST MS *sql, sqlite3_callback cb);
INT       sql_columnName(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_std(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_countOnly(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_exec(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
BOOL      sql_saveOrLoadMemdb(sqlite3 *mem_db, WS *ofn, BOOL save_load);
VOID      print_footer();
VOID      print_version();
VOID      print_help();

UINT      $BufSize = 500000;  // Tmp初期サイズ
CONST UINT $BufSizeDMZ = 100;
MS        *$Buf = 0;          // Tmp文字列
MS        *$BufEnd = 0;       // Tmp文字列 \0
MS        *$BufEOD = 0;       // Tmp文字列 EOD
UINT      $iDirId = 0;        // Dir数
UINT      $lAllCnt = 0;       // 検索数
UINT      $iCall_ifind10 = 0; // ifind10()が呼ばれた回数
UINT      $iStepCnt = 0;      // CurrentDir位置
UINT      $LN = 0;            // 処理行数 <= NTFSの最大ファイル数(2^32 - 1 = 4,294,967,295)
MS        *$sqlU = 0;
sqlite3   *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;

#define   MEMDB               L":memory:"
#define   OLDDB               (L"iwmfind.db."IWM_VERSION)
#define   CREATE_T_DIR \
			"CREATE TABLE T_DIR( \
				dir_id    INTEGER, \
				dir       TEXT, \
				step_byte INTEGER \
			);"
#define   INSERT_T_DIR \
			"INSERT INTO T_DIR( \
				dir_id, \
				dir, \
				step_byte \
			) VALUES(?, ?, ?);"
#define   CREATE_T_FILE \
			"CREATE TABLE T_FILE( \
				dir_id    INTEGER, \
				id        INTEGER, \
				name      TEXT, \
				attr_num  INTEGER, \
				ctime_cjd REAL, \
				mtime_cjd REAL, \
				atime_cjd REAL, \
				size      INTEGER, \
				ln        INTEGER);"
#define   INSERT_T_FILE \
			"INSERT INTO T_FILE( \
				dir_id, \
				id, \
				name, \
				attr_num, \
				ctime_cjd, \
				mtime_cjd, \
				atime_cjd, \
				size, \
				ln \
			) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);"
#define   CREATE_VIEW \
			"CREATE VIEW V_INDEX AS SELECT \
				T_FILE.ln AS LN, \
				(T_DIR.dir || T_FILE.name) AS path, \
				T_DIR.dir_id AS dir_id, \
				T_DIR.dir AS dir, \
				T_FILE.id AS id, \
				T_FILE.name AS name, \
				(CASE T_FILE.name WHEN '' THEN 'd' ELSE 'f' END) AS type, \
				T_FILE.attr_num AS attr_num, \
				((CASE T_FILE.name WHEN '' THEN 'd' ELSE 'f' END) || (CASE 1&T_FILE.attr_num WHEN 1 THEN 'r' ELSE '-' END) || (CASE 2&T_FILE.attr_num WHEN 2 THEN 'h' ELSE '-' END) || (CASE 4&T_FILE.attr_num WHEN 4 THEN 's' ELSE '-' END) || (CASE 32&T_FILE.attr_num WHEN 32 THEN 'a' ELSE '-' END)) AS attr, \
				T_FILE.ctime_cjd AS ctime_cjd, \
				datetime(T_FILE.ctime_cjd - 0.5) AS ctime, \
				T_FILE.mtime_cjd AS mtime_cjd, \
				datetime(T_FILE.mtime_cjd - 0.5) AS mtime, \
				T_FILE.atime_cjd AS atime_cjd, \
				datetime(T_FILE.atime_cjd - 0.5) AS atime, \
				T_FILE.size AS size, \
				T_DIR.step_byte AS step_byte \
				FROM T_FILE LEFT JOIN T_DIR ON T_FILE.dir_id=T_DIR.dir_id;"
#define   UPDATE_EXEC99_1 \
			"UPDATE T_FILE SET id=0 WHERE (id) NOT IN (SELECT id FROM V_INDEX %s);"
#define   DELETE_EXEC99_1 \
			"DELETE FROM T_FILE WHERE id=0;"
#define   SELECT_VIEW \
			"SELECT %S FROM V_INDEX %S %S ORDER BY %S;"
#define   OP_SELECT_0 \
			L"LN,path"
#define   OP_SELECT_MKDIR \
			L"step_byte,dir,name,path"
#define   OP_SELECT_EXTRACT \
			L"path,name"
#define   OP_SELECT_RP \
			L"type,path"
#define   OP_SELECT_RM \
			L"path,dir,attr_num"
#define   I_MKDIR              1
#define   I_CP                 2
#define   I_MV                 3
#define   I_MV2                4
#define   I_EXT                5
#define   I_EXT2               6
#define   I_TB                11
#define   I_TB2               12
#define   I_REP               20
#define   I_RM                31
#define   I_RM2               32
//
// 検索Dir
//
WS **$waDirList = {};
INT $waDirListSize = 0;
//
// 入力DB
// -in=Str | -i=Str
//
WS *$wpInDbn = MEMDB;
WS *$wpInTmp = L"";
//
// 出力DB
// -out=Str | -o=Str
//
WS *$wpOutDbn = L"";
//
// select
// -select=Str | -s=Str
//
WS *$wpSelect = OP_SELECT_0;
INT $iSelectPosNumber = 0; // "LN"の配列位置
//
// where
// -where=Str | -w=Str
//
WS *$wpWhere1 = L"";
WS *$wpWhere2 = L"";
//
// group by
// -group=Str | -g=Str
//
WS *$wpGroup = L"";
//
// order by
// -sort=Str | -st=Str
//
WS *$wpSort = L"";
//
// ヘッダ情報を非表示／初期値は表示
// -noheader | -nh
//
BOOL $bPrintHeader = TRUE;
//
// フッタ情報を非表示／初期値は表示
// -nofooter | -nf
//
BOOL $bPrintFooter = TRUE;
//
// 出力をStrで囲む
// -quote=Str | -qt=Str
//
MS *$mpQuote = "";
//
// 出力をStrで区切る
// -separate=Str | -sp=Str
//
MS *$mpSeparate = " | ";
//
// 検索Dir位置
// -depth=NUM1,NUM2 | -d=NUM1,NUM2
//
INT $iDepthMin = 0; // 階層～開始位置(相対)
INT $iDepthMax = 0; // 階層～終了位置(相対)
//
// mkdir Str
// --mkdir=Str | --md=Str
//
WS *$wpMd = L"";
WS *$wpMdOp = L"";
//
// mkdir Str & copy
// --copy=Str | --cp=Str
//
WS *$wpCp = L"";
//
// mkdir Str & move Str
// --move=Str | --mv=Str
//
WS *$wpMv = L"";
//
// mkdir Str & move Str & rmdir
// --move2=Str | --mv2=Str
//
WS *$wpMv2 = L"";
//
// copy Str
// --extract=Str | --ext=Str
//
WS *$wpExt = L"";
//
// move Str
// --extract2=Str | --ext2=Str
//
WS *$wpExt2 = L"";
//
// TrashBox
// --tb  | --trashbox
// --tb2 | --trashbox2
//
INT $iTrashbox = 0;
//
// replace results with Str
// --replace=Str | --rep=Str
//
WS *$wpRep = L"";
WS *$wpRepOp = L"";
//
// Remove
// --rm  | --remove
// --rm2 | --remove2
//
INT $iRm = 0;
//
// 優先順 (安全)I_MD > (危険)I_RM2
//  I_MD   = --mkdir
//  I_CP   = --cp
//  I_MV   = --mv
//  I_MV2  = --mv2
//  I_EXT  = --ext
//  I_EXT2 = --ext2
//  I_TB   = --tb
//  I_TB2  = --tb2
//  I_REP  = --rep
//  I_RM   = --rm
//  I_RM2  = --rm2
//
INT $iExec = 0;

INT
main()
{
	// lib_iwmutil2 初期化
	imain_begin();

	// カーソル隠す
	P1(IESC_CURSOR_OFF);

	// -h | -help
	if(! $ARGC || iCLI_getOptMatch(0, L"-h", L"--help"))
	{
		print_help();
		imain_end();
	}

	// -v | -version
	if(iCLI_getOptMatch(0, L"-v", L"--version"))
	{
		print_version();
		imain_end();
	}

	INT i1 = 0, i2 = 0;
	WS *wp1 = 0, *wp2 = 0;
	MS *mp1 = 0;

	/*
		$waDirList取得で $iDepthMax を使うため先に、
			-recursive
			-depth
		をチェック
	*/
	for(i1 = 0; i1 < $ARGC; i1++)
	{
		// -r | -recursive
		if(iCLI_getOptMatch(i1, L"-r", L"-recursive"))
		{
			$iDepthMin = 0;
			$iDepthMax = IMAX_PATH;
		}

		// -d=NUM1,NUM2 | -depth=NUM1,NUM2
		if((wp1 = iCLI_getOptValue(i1, L"-d=", L"-depth=")))
		{
			WS **wa1 = iwaa_split(wp1, L", ", TRUE);
				i2 = iwan_size(wa1);
				if(i2 > 1)
				{
					$iDepthMin = _wtoi(wa1[0]);
					$iDepthMax = _wtoi(wa1[1]);
					if($iDepthMax > IMAX_PATH)
					{
						$iDepthMax = IMAX_PATH;
					}
					if($iDepthMin > $iDepthMax)
					{
						i2 = $iDepthMin;
						$iDepthMin = $iDepthMax;
						$iDepthMax = i2;
					}
				}
				else if(i2 == 1)
				{
					$iDepthMin = $iDepthMax = _wtoi(wa1[0]);
				}
				else
				{
					$iDepthMin = $iDepthMax = 0;
				}
			ifree(wa1);
		}
	}

	// Dir存在チェック
	for(i1 = 0; i1 < $ARGC; i1++)
	{
		if(*$ARGV[i1] != '-' && ! iFchk_existPathW($ARGV[i1]))
		{
			MS *mp1 = W2M($ARGV[i1]);
				P("%s[Err] Dir '%s' は存在しない!%s\n", IESC_ERR1, mp1, IESC_RESET);
			ifree(mp1);
		}
	}

	// $waDirList を作成
	$waDirList = ($iDepthMax == IMAX_PATH ? iwaa_higherDir($ARGV) : iwaa_getDirFile($ARGV, 1));
	$waDirListSize = iwan_size($waDirList);

	// Main Loop
	for(i1 = 0; i1 < $ARGC; i1++)
	{
		// -i | -in
		if((wp1 = iCLI_getOptValue(i1, L"-i=", L"-in=")))
		{
			if(! iFchk_existPathW(wp1))
			{
				MS *mp1 = W2M(wp1);
					P("%s[Err] -in '%s' は存在しない!%s\n", IESC_ERR1, mp1, IESC_RESET);
				ifree(mp1);
				imain_end();
			}
			else if($waDirListSize)
			{
				P("%s[Err] Dir と -in は併用できない!%s\n", IESC_ERR1, IESC_RESET);
				imain_end();
			}
			else
			{
				$wpInDbn = $wpInTmp = wp1;
				// -in のときは -recursive 自動付与
				$iDepthMin = 0;
				$iDepthMax = IMAX_PATH;
			}
		}

		// -o | -out
		if((wp1 = iCLI_getOptValue(i1, L"-o=", L"-out=")))
		{
			$wpOutDbn = wp1;
		}

		// --md | --mkdir
		if((wp1 = iCLI_getOptValue(i1, L"--md=", L"--mkdir=")))
		{
			$wpMd = wp1;
		}

		// --cp | --copy
		if((wp1 = iCLI_getOptValue(i1, L"--cp=", L"--copy=")))
		{
			$wpCp = wp1;
		}

		// --mv | --move
		if((wp1 = iCLI_getOptValue(i1, L"--mv=", L"--move=")))
		{
			$wpMv = wp1;
		}

		// --mv2 | --move2
		if((wp1 = iCLI_getOptValue(i1, L"--mv2=", L"--move2=")))
		{
			$wpMv2 = wp1;
		}

		// --ext | --extract
		if((wp1 = iCLI_getOptValue(i1, L"--ext=", L"--extract=")))
		{
			$wpExt = wp1;
		}

		// --ext2 | --extract2
		if((wp1 = iCLI_getOptValue(i1, L"--ext2=", L"--extract2=")))
		{
			$wpExt2 = wp1;
		}

		// --tb | --trashbox
		if(iCLI_getOptMatch(i1, L"--tb", L"--trashbox"))
		{
			$iTrashbox = I_TB;
		}

		// --tb2 | --trashbox2
		if(iCLI_getOptMatch(i1, L"--tb2", L"--trashbox2"))
		{
			$iTrashbox = I_TB2;
		}

		// --rep | --replace
		if((wp1 = iCLI_getOptValue(i1, L"--rep=", L"--replace=")))
		{
			$wpRep = wp1;
		}

		// --rm | --remove
		if(iCLI_getOptMatch(i1, L"--rm", L"--remove"))
		{
			$iRm = I_RM;
		}

		// --rm2 | --remove2
		if(iCLI_getOptMatch(i1, L"--rm2", L"--remove2"))
		{
			$iRm = I_RM2;
		}

		// -s | -select
		/*
			(none)     : OP_SELECT_0
			-select="" : Err
			-select="Column1,Column2,..."
		*/
		if((wp1 = iCLI_getOptValue(i1, L"-s=", L"-select=")))
		{
			// "AS" 対応のため " " (空白)は不可
			// (例) -s="dir||name AS PATH"
			WS **wa1 = iwaa_split(wp1, L",", TRUE);
				if(wa1[0])
				{
					WS **wa2 = iwaa_simplify(wa1, TRUE); // 重複排除
						$wpSelect = iwas_join(wa2, L",");
					ifree(wa2);
				}
			ifree(wa1);
		}

		// -st | -sort
		if((wp1 = iCLI_getOptValue(i1, L"-st=", L"-sort=")))
		{
			$wpSort = wp1;
		}

		// -w | -where
		if((wp1 = iCLI_getOptValue(i1, L"-w=", L"-where=")))
		{
			$wpWhere1 = sql_escape(wp1);
			$wpWhere2 = iws_cats(2, L"WHERE ", $wpWhere1);
		}

		// -g | -group
		if((wp1 = iCLI_getOptValue(i1, L"-g=", L"-group=")))
		{
			$wpGroup = iws_cats(2, L"GROUP BY ", wp1);
		}

		// -nh | -noheader
		if(iCLI_getOptMatch(i1, L"-nh", L"-noheader"))
		{
			$bPrintHeader = FALSE;
		}

		// -nf | -nofooter
		if(iCLI_getOptMatch(i1, L"-nf", L"-nofooter"))
		{
			$bPrintFooter = FALSE;
		}

		// -qt | -quote
		if((wp1 = iCLI_getOptValue(i1, L"-qt=", L"-quote=")))
		{
			wp2 = iws_conv_escape(wp1);
				// 文字数制限
				if(wcslen(wp2) > 4)
				{
					wp2[4] = 0;
				}
				$mpQuote = W2M(wp2);
			ifree(wp2);
		}

		// -sp | -separate
		if((wp1 = iCLI_getOptValue(i1, L"-sp=", L"-separate=")))
		{
			wp2 = iws_conv_escape(wp1);
				// 文字数制限
				if(wcslen(wp2) > 4)
				{
					wp2[4] = 0;
				}
				$mpSeparate = W2M(wp2);
			ifree(wp2);
		}
	}

	// --[exec] 関係を一括変換
	if(*$wpMd || *$wpCp || *$wpMv || *$wpMv2 || *$wpExt || *$wpExt2 || $iTrashbox || *$wpRep || $iRm)
	{
		$bPrintFooter = FALSE; // フッタ情報を非表示

		if(*$wpMd)
		{
			$iExec = I_MKDIR;
			$wpMdOp = iFget_ApathW($wpMd);
		}
		else if(*$wpCp)
		{
			$iExec = I_CP;
			$wpMdOp = iFget_ApathW($wpCp);
		}
		else if(*$wpMv)
		{
			$iExec = I_MV;
			$wpMdOp = iFget_ApathW($wpMv);
		}
		else if(*$wpMv2)
		{
			$iExec = I_MV2;
			$wpMdOp = iFget_ApathW($wpMv2);
		}
		else if(*$wpExt)
		{
			$iExec = I_EXT;
			$wpMdOp = iFget_ApathW($wpExt);
		}
		else if(*$wpExt2)
		{
			$iExec = I_EXT2;
			$wpMdOp = iFget_ApathW($wpExt2);
		}
		else if($iTrashbox)
		{
			$iExec = $iTrashbox;
		}
		else if(*$wpRep)
		{
			if(! iFchk_existPathW($wpRep))
			{
				MS *mp1 = W2M($wpRep);
					P("%s[Err] --replace '%s' は存在しない!%s\n", IESC_ERR1, mp1, IESC_RESET);
				ifree(mp1);
				imain_end();
			}
			else
			{
				$iExec = I_REP;
				$wpRepOp = iws_clone($wpRep);
			}
		}
		else if($iRm)
		{
			$iExec = $iRm;
		}
		else
		{
		}

		if($iExec <= I_EXT2)
		{
			$wpSelect = ($iExec <= I_MV2 ? OP_SELECT_MKDIR : OP_SELECT_EXTRACT);
		}
		else if($iExec == I_REP)
		{
			$wpSelect = OP_SELECT_RP;
		}
		else
		{
			$wpSelect = OP_SELECT_RM;
		}
	}

	// -sort 関係を一括変換
	if($iExec >= I_MV)
	{
		// Dir削除時必要なものは "order by lower(path) desc"
		$wpSort = iws_clone(L"lower(path) desc");
	}
	else if(! *$wpSort)
	{
		$wpSort = iws_clone(L"lower(dir),lower(name)");
	}
	else
	{
		// path, dir, name, ext
		// ソートは、大文字・小文字を区別しない
		wp1 = $wpSort;
		wp2 = iws_replace(wp1, L"path", L"lower(path)", FALSE);
		ifree(wp1);
		wp1 = iws_replace(wp2, L"dir", L"lower(dir)", FALSE);
		ifree(wp2);
		$wpSort = iws_replace(wp1, L"name", L"lower(name)", FALSE);
		ifree(wp1);
	}

	// -in DBを指定
	if(sqlite3_open16($wpInDbn, &$iDbs))
	{
		mp1 = W2M($wpInDbn);
			P("%s[Err] -in '%s' を開けない!%s\n", IESC_ERR1, mp1, IESC_RESET);
		ifree(mp1);
		sqlite3_close($iDbs); // ErrでもDB解放
		imain_end();
	}
	else
	{
		// UTF-8 出力領域
		$Buf = icalloc_MS($BufSize + $BufSizeDMZ);
		$BufEnd = $Buf;
		$BufEOD = $Buf + $BufSize;

		// DB構築
		if(! *$wpInTmp)
		{
			$struct_iFinfoW *FI = iFinfo_allocW();
				WIN32_FIND_DATAW F;
				// PRAGMA設定
				sql_exec($iDbs, "PRAGMA encoding = 'UTF-8';", 0);
				/* 2022-09-01修正
					＜入力＞     UTF-16
					＜内部処理＞ UTF-16
					＜Sqlite3＞  UTF-16 => UTF-8
					＜出力＞     UTF-8
				*/
				// TABLE作成
				sql_exec($iDbs, CREATE_T_DIR, 0);
				sql_exec($iDbs, CREATE_T_FILE, 0);
				// VIEW作成
				sql_exec($iDbs, CREATE_VIEW, 0);
				// 前処理
				sqlite3_prepare($iDbs, INSERT_T_DIR, imn_len(INSERT_T_DIR), &$stmt1, 0);
				sqlite3_prepare($iDbs, INSERT_T_FILE, imn_len(INSERT_T_FILE), &$stmt2, 0);
				// トランザクション開始
				sql_exec($iDbs, "BEGIN", 0);
				// 経過表示
				fprintf(stderr, IESC_OPT21);
				// 検索データ DB書込
				for(i2 = 0; (wp1 = $waDirList[i2]); i2++)
				{
					$iStepCnt = iwn_len(wp1);
					// 本処理
					ifind10(FI, F, wp1, 0);
				}
				// 経過表示をクリア
				fprintf(stderr, "\r\033[0K" IESC_RESET);
				// トランザクション終了
				sql_exec($iDbs, "COMMIT", 0);
				// 後処理
				sqlite3_finalize($stmt2);
				sqlite3_finalize($stmt1);
			iFinfo_freeW(FI);
		}
		// 結果
		if(*$wpOutDbn)
		{
			// 存在する場合、削除
			DeleteFileW($wpOutDbn);
			// $wpInTmp, $wpOutDbn 両指定、別ファイル名のとき
			if(*$wpInTmp)
			{
				CopyFileW($wpInDbn, OLDDB, FALSE);
			}
			// WHERE Str のとき不要データ削除
			if(*$wpWhere1)
			{
				sql_exec($iDbs, "BEGIN", 0);         // トランザクション開始
				MS *mp1 = W2M($wpWhere1);
				MS *mp2 = ims_cats(2, "WHERE ", mp1);
					sprintf($Buf, UPDATE_EXEC99_1, mp2);
				sql_exec($iDbs, $Buf, 0);            // フラグを立てる
				sql_exec($iDbs, DELETE_EXEC99_1, 0); // 不要データ削除
				sql_exec($iDbs, "COMMIT", 0);        // トランザクション終了
				sql_exec($iDbs, "VACUUM", 0);        // VACUUM
			}
			// $wpInTmp, $wpOutDbn 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
			sql_saveOrLoadMemdb($iDbs, (*$wpInTmp ? $wpInDbn : $wpOutDbn), TRUE);
			// 結果数を取得
 			sql_exec($iDbs, "SELECT id FROM V_INDEX;", sql_result_countOnly);
		}
		else
		{
			// SQL作成 UTF-8／Sqlite3対応
			wp1 = M2W(SELECT_VIEW);
			wp2 = iws_sprintf(wp1, $wpSelect, $wpWhere2, $wpGroup, $wpSort);
				$sqlU = W2M(wp2);
			ifree(wp2);
			ifree(wp1);
			// SQL実行
			if($iExec)
			{
				sql_exec($iDbs, $sqlU, sql_result_exec);
			}
			else
			{
				// カラム名出力／[LN]位置取得
				MS *mp1 = W2M($wpSelect);
				MS *mp2 = ims_cats(3, "SELECT ", mp1, " FROM V_INDEX LIMIT 1;");
					sql_exec($iDbs, mp2, sql_columnName);
				ifree(mp2);
				ifree(mp1);
				// 結果出力
				sql_exec($iDbs, $sqlU, sql_result_std);
				ifree($Buf);
			}
		}
		// DB解放
		sqlite3_close($iDbs);
		// $wpInDbn, $wpOutDbn ファイル名をswap
		if(*$wpInTmp)
		{
			MoveFileW($wpInDbn, $wpOutDbn);
			MoveFileW(OLDDB, $wpInDbn);
		}
		// 作業用ファイル削除
		DeleteFileW(OLDDB);
	}

	// フッタ部
	if($bPrintFooter)
	{
		print_footer();
	}

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	imain_end();
}

WS
*sql_escape(
	WS *pW
)
{
	WS *wp1 = iws_clone(pW);
	WS *wp2 = iws_replace(wp1, L";", L" ", FALSE); // ";" => " " SQLインジェクション回避
	ifree(wp1);
	wp1 = iws_replace(wp2, L"*", L"%", FALSE); // "*" => "%"
	ifree(wp2);
	wp2 = iws_replace(wp1, L"?", L"_", FALSE); // "?" => "_"
	ifree(wp1);
	// 現在時間
	INT *aiNow = (INT*)idate_cjdToiAryYmdhns(idate_nowToCjd(TRUE));
	// [] を日付に変換
	wp1 = idate_replace_format_ymdhns(
			wp2,
			L"[", L"]",
			L"'",
			aiNow[0],
			aiNow[1],
			aiNow[2],
			aiNow[3],
			aiNow[4],
			aiNow[5]
	);
	ifree(aiNow);
	ifree(wp2);
	return wp1;
}

VOID
ifind10(
	$struct_iFinfoW *FI,
	WIN32_FIND_DATAW F,
	WS *dir,
	INT depth
)
{
	if(depth > $iDepthMax)
	{
		return;
	}
	// FI->sPathW 末尾に "*" を付与
	UINT dirLen = iwn_cpy(FI->sPathW, dir);
		*(FI->sPathW + dirLen)     = '*';
		*(FI->sPathW + dirLen + 1) = 0;
	HANDLE hfind = FindFirstFileW(FI->sPathW, &F);
		// アクセス不可なフォルダ等はスルー
		if(hfind == INVALID_HANDLE_VALUE)
		{
			return;
		}
		// 読み飛ばす Depth
		BOOL bMinDepthFlg = (depth >= $iDepthMin ? TRUE : FALSE);
		// dirIdを逐次生成
		INT dirId = (++$iDirId);
		// Dir
		if(bMinDepthFlg)
		{
			ifind21(dir, dirId);
			iFinfo_initW(FI, &F, dir, NULL);
			ifind22(FI, dirId, L"");
		}
		// File
		do
		{
			if(iFinfo_initW(FI, &F, dir, F.cFileName))
			{
				if(FI->bType)
				{
					WS *wp1 = iws_clone(FI->sPathW);
						// 下位Dirへ
						ifind10(FI, F, wp1, (depth + 1));
					ifree(wp1);
				}
				else if(bMinDepthFlg)
				{
					ifind22(FI, dirId, F.cFileName);
				}
				else
				{
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
	// 経過表示
	++$iCall_ifind10;
	if($iCall_ifind10 >= 5000)
	{
		// 行消去
		fprintf(stderr, "\r\033[0K> %u", $lAllCnt);
		$iCall_ifind10 = 0;
	}
}

VOID
ifind21(
	WS *dir,
	INT dirId
)
{
	sqlite3_reset($stmt1);
		sqlite3_bind_int64($stmt1,  1, dirId);
		sqlite3_bind_text16($stmt1, 2, dir, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt1,    3, $iStepCnt);
	sqlite3_step($stmt1);
}

VOID
ifind22(
	$struct_iFinfoW *FI,
	INT dirId,
	WS *fname
)
{
	++$lAllCnt;
	sqlite3_reset($stmt2);
		sqlite3_bind_int($stmt2,    1, dirId);
		sqlite3_bind_int($stmt2,    2, $lAllCnt);
		sqlite3_bind_text16($stmt2, 3, fname, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt2,    4, FI->uAttr);
		sqlite3_bind_double($stmt2, 5, FI->cjdCtime);
		sqlite3_bind_double($stmt2, 6, FI->cjdMtime);
		sqlite3_bind_double($stmt2, 7, FI->cjdAtime);
		sqlite3_bind_int64($stmt2,  8, FI->uFsize);
		sqlite3_bind_int($stmt2,    9, 0);
	sqlite3_step($stmt2);
}

VOID
sql_exec(
	sqlite3 *db,
	CONST MS *sql,
	sqlite3_callback cb
)
{
	MS *p_err = 0; // SQLiteが使用
	$LN = 0;
	if(sqlite3_exec(db, sql, cb, 0, &p_err))
	{
		P("%s[Err] 構文エラー%s\n      %s\n      %s\n", IESC_ERR1, IESC_RESET, p_err, sql);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}
	// sql_result_std() 対応
	QP($Buf, ($BufEnd - $Buf));
}

INT
sql_columnName(
	VOID *option,
	INT iColumnCount,
	MS **sColumnValues,
	MS **sColumnNames
)
{
	// [LN]位置取得 [0..n]
	// この時点で[LN]の重複は排除されていること
	$iSelectPosNumber = -1;
	for(INT _i1 = 0; _i1 < iColumnCount; _i1++)
	{
		WS *wp1 = M2W(sColumnNames[_i1]);
			if(iwb_cmppi(wp1, L"LN"))
			{
				$iSelectPosNumber = _i1;
			}
		ifree(wp1);
	}
	// カラム名出力
	if($bPrintHeader)
	{
		P1(IESC_OPT21);
		for(INT _i1 = 0; _i1 < iColumnCount; _i1++)
		{
			P1("[");
			P1(sColumnNames[_i1]);
			P1("]");
			if((_i1 + 1) < iColumnCount)
			{
				P1($mpSeparate);
			}
		}
		P2(IESC_RESET);
	}
	return SQLITE_OK;
}

INT
sql_result_std(
	VOID *option,
	INT iColumnCount,
	MS **sColumnValues,
	MS **sColumnNames
)
{
	++$LN;
	// UINTだと遅い?
	INT i1 = 0;
	while(i1 < iColumnCount)
	{
		// [LN]
		if(i1 == $iSelectPosNumber)
		{
			// sprintf() は遅いので多用しない
			$BufEnd += imn_cpy($BufEnd, $mpQuote);
			$BufEnd += sprintf($BufEnd, "%u", $LN);
			$BufEnd += imn_cpy($BufEnd, $mpQuote);
		}
		else
		{
			if($BufEOD <= ($BufEnd + (UINT)strlen(sColumnValues[i1])))
			{
				// Buf を Print
				QP($Buf, ($BufEnd - $Buf));
				// Realloc
				//   Realloc版は出力量を可変とすることで、数万件から数十万件まで安定して高速になる。
				//   No-Realloc版は出力量が固定のため、特化領域でのみ高速になる。
				if($BufSize < 10000000)
				{
					$BufSize *= 2;
					$Buf = irealloc_MS($Buf, $BufSize + $BufSizeDMZ);
					$BufEOD = $Buf + $BufSize;
				}
				$Buf[0] = 0;
				$BufEnd = $Buf;
			}
			$BufEnd += imn_cpy($BufEnd, $mpQuote);
			$BufEnd += imn_cpy($BufEnd, sColumnValues[i1]);
			$BufEnd += imn_cpy($BufEnd, $mpQuote);
		}
		++i1;
		$BufEnd += imn_cpy($BufEnd, (i1 == iColumnCount ? "\n" : $mpSeparate));
	}
	return SQLITE_OK;
}

INT
sql_result_countOnly(
	VOID *option,
	INT iColumnCount,
	MS **sColumnValues,
	MS **sColumnNames
)
{
	++$LN;
	return SQLITE_OK;
}

INT
sql_result_exec(
	VOID *option,
	INT iColumnCount,
	MS **sColumnValues,
	MS **sColumnNames
)
{
	INT i1 = 0;
	WS *wp1 = 0, *wp2 = 0, *wp3 = 0, *wp4 = 0, *wp5 = 0;
	switch($iExec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// $wpMdOp\以下には、$waDirList\以下のDirを作成
			i1  = atoi(sColumnValues[0]); // step_cnt
			wp1 = M2W(sColumnValues[1]);  // dir + "\"
			wp2 = M2W(sColumnValues[2]);  // name
			wp3 = M2W(sColumnValues[3]);  // path
			wp4 = iws_cats(2, $wpMdOp, (wp1 + i1));
			wp5 = iws_cats(2, wp4, wp2);
				// mkdir
				if(imk_dirW(wp4))
				{
					P1("\033[96m+ ");
					P1W(wp4);
					P2("\033[0m");
					++$LN;
				}
				// 優先処理
				if($iExec == I_CP)
				{
					if(CopyFileW(wp3, wp5, FALSE))
					{
						P1("\033[96m+ ");
						P1W(wp5);
						P2("\033[0m");
						++$LN;
					}
				}
				else if($iExec >= I_MV)
				{
					// ReadOnly属性(1)を解除
					if((GetFileAttributesW(wp5) & FILE_ATTRIBUTE_READONLY))
					{
						SetFileAttributesW(wp5, FALSE);
					}
					// 既存File削除
					if(iFchk_existPathW(wp5))
					{
						DeleteFileW(wp5);
					}
					if(MoveFileW(wp3, wp5))
					{
						P1("\033[96m+ ");
						P1W(wp5);
						P2("\033[0m");
						P1("\033[94m- ");
						P1W(wp3);
						P2("\033[0m");
						++$LN;
					}
					// rmdir
					if($iExec == I_MV2)
					{
						if(RemoveDirectoryW(wp1))
						{
							P1("\033[94m- ");
							P1(sColumnValues[1]);
							P2("\033[0m");
							++$LN;
						}
					}
				}
				else
				{
				}
			ifree(wp5);
			ifree(wp4);
			ifree(wp3);
			ifree(wp2);
			ifree(wp1);
		break;

		case(I_EXT):  // --extract
		case(I_EXT2): // --extract2
			wp1 = M2W(sColumnValues[0]); // path
			wp2 = M2W(sColumnValues[1]); // name
				// mkdir
				if(imk_dirW($wpMdOp))
				{
					P1("\033[96m+ ");
					P1W($wpMdOp);
					P2("\033[0m");
					++$LN;
				}
				// 先
				wp3 = iws_cats(2, $wpMdOp, wp2);
				// I_EXT, I_EXT2共、同名Fileは上書き
				if($iExec == I_EXT)
				{
					if(CopyFileW(wp1, wp3, FALSE))
					{
						P1("\033[96m+ ");
						P1W(wp3);
						P2("\033[0m");
						++$LN;
					}
				}
				else if($iExec == I_EXT2)
				{
					// ReadOnly属性(1)を解除
					if((GetFileAttributesW(wp3) & FILE_ATTRIBUTE_READONLY))
					{
						SetFileAttributesW(wp3, FALSE);
					}
					// Dir存在していれば削除しておく
					if(iFchk_existPathW(wp3))
					{
						DeleteFileW(wp3);
					}
					if(MoveFileW(wp1, wp3))
					{
						P1("\033[96m+ ");
						P1W(wp3);
						P2("\033[0m");
						P1("\033[94m- ");
						P1W(wp1);
						P2("\033[0m");
						++$LN;
					}
				}
				else
				{
				}
			ifree(wp3);
			ifree(wp2);
			ifree(wp1);
		break;

		case(I_TB):  // --trashbox
		case(I_TB2): // --trashbox2
			wp1 = M2W(sColumnValues[0]); // path
			i1 = atoi(sColumnValues[2]);
				// ReadOnly属性(1)を解除
				if((i1 & FILE_ATTRIBUTE_READONLY))
				{
					SetFileAttributesW(wp1, FALSE);
				}
				// Fileをゴミ箱へ移動
				if(! (i1 & FILE_ATTRIBUTE_DIRECTORY) && imv_trashW(wp1))
				{
					P1("\033[92m- ");
					P1W(wp1);
					P2("\033[0m");
					++$LN;
				}
				// Dirをゴミ箱へ移動
				if($iExec == I_TB2 && (i1 & FILE_ATTRIBUTE_DIRECTORY) && imv_trashW(wp1))
				{
					P1("\033[92m- ");
					P1W(wp1);
					P2("\033[0m");
					++$LN;
				}
			ifree(wp1);
		break;

		case(I_REP): // --replace
			wp1 = M2W(sColumnValues[0]); // type
			wp2 = M2W(sColumnValues[1]); // path
				if(*wp1 == 'f' && CopyFileW($wpRepOp, wp2, FALSE))
				{
					P1("\033[96m+ ");
					P1W(wp2);
					P2("\033[0m");
					++$LN;
				}
			ifree(wp2);
			ifree(wp1);
		break;

		case(I_RM):  // --remove
		case(I_RM2): // --remove2
			wp1 = M2W(sColumnValues[0]); // path
				// ReadOnly属性(1)を解除
				if((atoi(sColumnValues[2]) & FILE_ATTRIBUTE_READONLY))
				{
					SetFileAttributesW(wp1, FALSE);
				}
				// File削除
				if(DeleteFileW(wp1))
				{
					P1("\033[94m- ");
					P1W(wp1);
					P2("\033[0m");
					++$LN;
				}
				// 空Dir削除
				if($iExec == I_RM2)
				{
					wp2 = M2W(sColumnValues[1]); // dir
						// 空Dirである
						if(RemoveDirectoryW(wp2))
						{
							P1("\033[94m- ");
							P1W(wp2);
							P2("\033[0m");
							++$LN;
						}
					ifree(wp2);
				}
			ifree(wp1);
		break;
	}

	return SQLITE_OK;
}

BOOL
sql_saveOrLoadMemdb(
	sqlite3 *mem_db, // ":memory:"
	WS *ofn,         // Filename
	BOOL save_load   // TRUE=save／FALSE=load
)
{
	INT err = 0;
	sqlite3 *pFile;
	sqlite3_backup *pBackup;
	sqlite3 *pTo;
	sqlite3 *pFrom;

	if(! (err = sqlite3_open16(ofn, &pFile)))
	{
		if(save_load)
		{
			pFrom = mem_db;
			pTo =pFile;
		}
		else
		{
			pFrom = pFile;
			pTo = mem_db;
		}
		pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
		if(pBackup)
		{
			sqlite3_backup_step(pBackup, -1);
			sqlite3_backup_finish(pBackup);
		}
		err = sqlite3_errcode(pTo);
	}
	sqlite3_close(pFile);

	return (! err ? TRUE : FALSE);
}

VOID
print_footer()
{
	MS *mp1 = 0;
	P(IESC_OPT21);
	P(
		"-- %lld row%s in set ( %.3f sec)\n",
		$LN,
		($LN > 1 ? "s" : ""), // 複数形
		iExecSec_next()
	);
	P(IESC_OPT22);
	P2("--");
	for(INT _i1 = 0; _i1 < $waDirListSize; _i1++)
	{
		mp1 = W2M($waDirList[_i1]);
			P("--  '%s'\n", mp1);
		ifree(mp1);
	}
	if($sqlU)
	{
		P("--  '%s'\n", $sqlU);
	}
	if($wpInDbn)
	{
		mp1 = W2M($wpInDbn);
			P("--  -in        '%s'\n", mp1);
		ifree(mp1);
	}
	if($wpOutDbn)
	{
		mp1 = W2M($wpOutDbn);
			P("--  -out       '%s'\n", mp1);
		ifree(mp1);
	}
	if(! $bPrintHeader)
	{
		P2("--  -noheader");
	}
	if($mpQuote)
	{
		P("--  -quote     '%s'\n", $mpQuote);
	}
	if($mpSeparate)
	{
		P("--  -separate  '%s'\n", $mpSeparate);
	}
	P2("--");
	P(IESC_RESET);
}

VOID
print_version()
{
	P1(IESC_STR2);
	LN(80);
	P(
		" %s\n"
		"    Ver.%s+%s+SQLite%s\n"
		, IWM_COPYRIGHT, IWM_VERSION, LIB_IWMUTIL_VERSION, SQLITE_VERSION
	);
	LN(80);
	P1(IESC_RESET);
}

VOID
print_help()
{
	MS *_cmd = W2M($CMD);

	print_version();
	P(
		IESC_TITLE1	" ファイル検索 "
		IESC_RESET	"\n"
		IESC_STR1	"    %s"
		IESC_OPT1	" [Dir]"
		IESC_OPT2	" [Option]\n"
		IESC_LBL1	"        or\n"
		IESC_STR1	"    %s"
		IESC_OPT2	" [Option]"
		IESC_OPT1	" [Dir]\n\n"
		IESC_LBL1	" (例１)"
		IESC_STR1	" 検索\n"
					"    %s"
		IESC_OPT1	" Dir"
		IESC_OPT2	" -r -s=\"LN,path,size\" -w=\"name like '*.exe'\"\n\n"
		IESC_LBL1	" (例２)"
		IESC_STR1	" 検索結果をファイルへ保存\n"
					"    %s"
		IESC_OPT1	" Dir1 Dir2"
		IESC_OPT2	" -r -o=File\n\n"
		IESC_LBL1	" (例３)"
		IESC_STR1	" 検索対象をファイルから読込\n"
					"    %s"
		IESC_OPT2	" -i=File\n\n"
		, _cmd, _cmd, _cmd, _cmd, _cmd
	);
	P1(
		IESC_OPT1	" [Dir]\n"
		IESC_STR1	"    検索対象Dir／複数指定可\n"
		IESC_LBL1	"    (例)"
		IESC_STR1	" \"c:\\\" \".\"\n\n"
		IESC_OPT2	" [Option]\n"
		IESC_LBL2	"  [基本操作]\n"
		IESC_OPT21	"    -recursive | -r\n"
		IESC_STR1	"        全階層を検索\n\n"
		IESC_OPT21	"    -depth=Num1,Num2 | -d=Num1,Num2\n"
		IESC_STR1	"        検索する階層を指定\n"
		IESC_LBL1	"        (例１)"
		IESC_STR1	" -d=0\n"
					"               0階層のみ検索\n\n"
		IESC_LBL1	"        (例２)"
		IESC_STR1	" -d=0,2\n"
					"               0～2階層を検索\n\n"
		IESC_OPT22	"        CurrentDir は階層=0\n\n"
		IESC_OPT21	"    -out=File | -o=File\n"
		IESC_STR1	"        出力ファイル\n\n"
		IESC_OPT21	"    -in=File | -i=File\n"
		IESC_STR1	"        入力ファイル\n"
		IESC_OPT22	"        検索対象Dirと併用できない\n\n"
		IESC_LBL2	"  [SQL関連]\n"
		IESC_OPT21	"    -select=Column1,Column2,... | -s=Column1,Column2,...\n"
		IESC_STR1	"        LN        (Num) 連番／1回のみ指定可\n"
					"        path      (Str) dir\\name\n"
					"        dir       (Str) ディレクトリ名\n"
					"        name      (Str) ファイル名\n"
					"        type      (Str) ディレクトリ = d／ファイル = f\n"
					"        attr_num  (Num) 属性\n"
					"        attr      (Str) 属性 \"[d|f][r|-][h|-][s|-][a|-]\"\n"
					"                        [dir|file][read-only][hidden][system][archive]\n"
					"        size      (Num) ファイルサイズ = byte\n"
					"        ctime_cjd (Num) 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5\n"
					"        ctime     (Str) 作成日時     \"yyyy-mm-dd hh:nn:ss\"\n"
					"        mtime_cjd (Num) 更新日時     ctime_cjd参照\n"
					"        mtime     (Str) 更新日時     ctime参照\n"
					"        atime_cjd (Num) アクセス日時 ctime_cjd参照\n"
					"        atime     (Str) アクセス日時 ctime参照\n"
					"        *         全項目表示\n\n"
		IESC_OPT22	"        (補１) Column指定なし\n"
		IESC_STR1	"             LN,path を表示\n\n"
		IESC_OPT22	"        (補２) SQLite関数を利用可能\n"
		IESC_STR1	"             abs(X)                        changes()                     char(X1,X2,...,XN)\n"
					"             coalesce(X,Y,...)             format(FORMAT,...)            glob(X,Y)\n"
					"             hex(X)                        ifnull(X,Y)                   iif(X,Y,Z)\n"
					"             instr(X,Y)                    last_insert_rowid()           length(X)\n"
					"             like(X,Y)                     like(X,Y,Z)                   likelihood(X,Y)\n"
					"             likely(X)                     load_extension(X)             load_extension(X,Y)\n"
					"             lower(X)                      ltrim(X)                      ltrim(X,Y)\n"
					"             max(X,Y,...)                  min(X,Y,...)                  nullif(X,Y)\n"
					"             octet_length(X)               printf(FORMAT,...)            quote(X)\n"
					"             random()                      randomblob(N)                 replace(X,Y,Z)\n"
					"             round(X)                      round(X,Y)                    rtrim(X)\n"
					"             rtrim(X,Y)                    sign(X)                       soundex(X)\n"
					"             sqlite_compileoption_get(N)   sqlite_compileoption_used(X)  sqlite_offset(X)\n"
					"             sqlite_source_id()            sqlite_version()              substr(X,Y)\n"
					"             substr(X,Y,Z)                 substring(X,Y)                substring(X,Y,Z)\n"
					"             total_changes()               trim(X)                       trim(X,Y)\n"
					"             typeof(X)                     unhex(X)                      unhex(X,Y)\n"
					"             unicode(X)                    unlikely(X)                   upper(X)\n"
					"             zeroblob(N)\n"
		IESC_LBL2	"             (参考) http://www.sqlite.org/lang_corefunc.html\n\n"
		IESC_OPT21	"    -where=Str | -w=Str\n"
		IESC_LBL1	"        (例１)"
		IESC_STR1	" \"size <= 100 or size > 1000000\"\n\n"
		IESC_LBL1	"        (例２)"
		IESC_STR1	" \"type = 'f' and name like 'abc??.*'\"\n"
					"               '?' '_' は任意の1文字\n"
					"               '*' '%%' は任意の0文字以上\n\n"
		IESC_LBL1	"        (例３)"
		IESC_STR1	" 基準日 \"2010-12-10 12:30:00\" のとき\n"
					"               \"ctime >= [-10d]\"  : ctime >= '2010-11-30 12:30:00'\n"
					"               \"ctime >= [-10D]\"  : ctime >= '2010-11-30 00:00:00'\n"
					"               \"ctime >= [-10d%%]\" : ctime >= '2010-11-30 %%'\n"
					"               \"ctime like [%%]\"   : ctime like '2010-12-10 %%'\n"
					"               (年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s\n\n"
		IESC_OPT21	"    -group=Str | -g=Str\n"
		IESC_LBL1	"        (例)"
		IESC_STR1	" -g=\"Str1, Str2\"\n"
					"             Str1とStr2をグループ毎にまとめる\n\n"
		IESC_OPT21	"    -sort=\"Str [ASC|DESC]\" | -st=\"Str [ASC|DESC]\"\n"
		IESC_LBL1	"        (例)"
		IESC_STR1	" -st=\"Str1 ASC, Str2 DESC\"\n"
					"             Str1を順ソート, Str2を逆順ソート\n\n"
		IESC_LBL2	"  [出力フォーマット]\n"
		IESC_OPT21	"    -noheader | -nh\n"
		IESC_STR1	"        ヘッダ情報を表示しない\n\n"
		IESC_OPT21	"    -nofooter | -nf\n"
		IESC_STR1	"        フッタ情報を表示しない\n\n"
		IESC_OPT21	"    -quote=Str | -qt=Str\n"
		IESC_STR1	"        囲み文字\n"
		IESC_LBL1	"        (例)"
		IESC_STR1	" -qt=\"'\"\n\n"
		IESC_OPT21	"    -separate=Str | -sp=Str\n"
		IESC_STR1	"        区切り文字\n"
		IESC_LBL1	"        (例)"
		IESC_STR1	" -sp=\"\\t\"\n\n"
		IESC_LBL2	"  [出力結果を操作]\n"
		IESC_OPT21	"    --mkdir=Dir | --md=Dir\n"
		IESC_STR1	"        検索結果のDirをコピー作成する (-recursive のとき 階層維持)\n\n"
		IESC_OPT21	"    --copy=Dir | --cp=Dir\n"
		IESC_STR1	"        --mkdir + 検索結果をDirにコピーする (-recursive のとき 階層維持)\n\n"
		IESC_OPT21	"    --move=Dir | --mv=Dir\n"
		IESC_STR1	"        --mkdir + 検索結果をDirに移動する (-recursive のとき 階層維持)\n\n"
		IESC_OPT21	"    --move2=Dir | --mv2=Dir\n"
		IESC_STR1	"        --mkdir + --move + 移動元の空Dirを削除する (-recursive のとき 階層維持)\n\n"
		IESC_OPT21	"    --extract=Dir | --ext=Dir\n"
		IESC_STR1	"        --mkdir + 検索結果ファイルのみ抽出しDirにコピーする\n"
					"        階層を維持しない／同名ファイルは上書き\n\n"
		IESC_OPT21	"    --extract2=Dir | --ext2=Dir\n"
		IESC_STR1	"        --mkdir + 検索結果ファイルのみ抽出しDirに移動する\n"
					"        階層を維持しない／同名ファイルは上書き\n\n"
		IESC_OPT21	"    --remove | --rm\n"
		IESC_STR1	"        検索結果のFileのみ削除する (Dirは削除しない) \n\n"
		IESC_OPT21	"    --remove2 | --rm2\n"
		IESC_STR1	"        --remove + 空Dirを削除する\n\n"
		IESC_OPT21	"    --trashbox | --tb\n"
		IESC_STR1	"        検索結果のFileのみゴミ箱へ移動する (Dirは移動しない) \n\n"
		IESC_OPT21	"    --trashbox2 | --tb2\n"
		IESC_STR1	"        --trashbox + Dirをゴミ箱へ移動する\n"
		IESC_OPT22	"        (注) Dir以下すべてをゴミ箱へ移動する\n\n"
		IESC_OPT21	"    --replace=File | --rep=File\n"
		IESC_STR1	"        検索結果(複数) をFileの内容で置換(上書き)する／ファイル名は変更しない\n"
		IESC_LBL1	"        (例)"
		IESC_STR1	" -w=\"name like 'before.txt'\" --rep=\".\\after.txt\"\n\n"
	);
	P1(IESC_STR2);
	LN(80);
	P1(IESC_RESET);

	ifree(_cmd);
}
