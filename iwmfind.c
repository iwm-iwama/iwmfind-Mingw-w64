//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmfind5_20220926"
#define   IWM_COPYRIGHT       "Copyright (C)2009-2022 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"
#include "sqlite3.h"

INT       main();
WCS       *sql_escape(WCS *pW);
VOID      ifind10($struct_iFinfoW *FI, WIN32_FIND_DATAW F, WCS *dir, INT depth);
VOID      ifind10_CallCnt(INT iCnt);
VOID      ifind21(WCS *dir, INT dirId, INT depth);
VOID      ifind22($struct_iFinfoW *FI, INT dirId, WCS *fname);
VOID      sql_exec(sqlite3 *db, CONST MBS *sql, sqlite3_callback cb);
INT       sql_columnName(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT       sql_result_std(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT       sql_result_countOnly(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT       sql_result_exec(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
BOOL      sql_saveOrLoadMemdb(sqlite3 *mem_db, WCS *ofn, BOOL save_load);
VOID      print_footer();
VOID      print_version();
VOID      print_help();

#define   BUF_MAXCNT          5000
#define   BUF_SIZE_MAX        (IMAX_PATH * BUF_MAXCNT)
#define   BUF_SIZE_DMZ        (IMAX_PATH * 2)
MBS       *$mpBuf = 0;        // Tmp文字列
MBS       *$mpBufE = 0;       // Tmp文字列末尾
MBS       *$mpBufMax = 0;     // Tmp文字列Max点
INT       $iDirId = 0;        // Dir数
INT64     $lAllCnt = 0;       // 検索数
INT       $iCall_ifind10 = 0; // ifind10()が呼ばれた回数
INT       $iStepCnt = 0;      // CurrentDir位置
INT64     $lRowCnt = 0;       // 処理行数 <= NTFSの最大ファイル数(2^32 - 1 = 4,294,967,295)
MBS       *$sqlU = 0;
sqlite3   *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;

// リセット
#define   PRGB00()            P1("\033[0m")
// ラベル
#define   PRGB01()            P1("\033[38;2;255;255;0m")    // 黄
#define   PRGB02()            P1("\033[38;2;255;255;255m")  // 白
// 入力例／注
#define   PRGB11()            P1("\033[38;2;255;255;100m")  // 黄
#define   PRGB12()            P1("\033[38;2;255;220;150m")  // 橙
#define   PRGB13()            P1("\033[38;2;100;100;255m")  // 青
// オプション
#define   PRGB21()            P1("\033[38;2;80;255;255m")   // 水
#define   PRGB22()            P1("\033[38;2;255;100;255m")  // 紅紫
// 本文
#define   PRGB91()            P1("\033[38;2;255;255;255m")  // 白
#define   PRGB92()            P1("\033[38;2;200;200;200m")  // 銀

#define   MEMDB               L":memory:"
#define   OLDDB               (L"iwmfind.db."IWM_VERSION)
#define   CREATE_T_DIR \
			"CREATE TABLE T_DIR( \
				dir_id    INTEGER, \
				dir       TEXT, \
				depth     INTEGER, \
				step_byte INTEGER \
			);"
#define   INSERT_T_DIR \
			"INSERT INTO T_DIR( \
				dir_id, \
				dir, \
				depth, \
				step_byte \
			) VALUES(?, ?, ?, ?);"
#define   CREATE_T_FILE \
			"CREATE TABLE T_FILE( \
				id        INTEGER, \
				dir_id    INTEGER, \
				name      TEXT, \
				attr_num  INTEGER, \
				ctime_cjd REAL, \
				mtime_cjd REAL, \
				atime_cjd REAL, \
				size      INTEGER, \
				number    INTEGER, \
				flg       INTEGER \
			);"
#define   INSERT_T_FILE \
			"INSERT INTO T_FILE( \
				id, \
				dir_id, \
				name, \
				attr_num, \
				ctime_cjd, \
				mtime_cjd, \
				atime_cjd, \
				size, \
				number, \
				flg \
			) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
#define   CREATE_VIEW \
			"CREATE VIEW V_INDEX AS SELECT \
				T_FILE.id AS id, \
				(T_DIR.dir || T_FILE.name) AS path, \
				T_DIR.dir AS dir, \
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
				T_FILE.number AS LN, \
				T_FILE.flg AS flg, \
				depth, \
				step_byte \
				FROM T_FILE LEFT JOIN T_DIR ON T_FILE.dir_id=T_DIR.dir_id;"
#define   UPDATE_EXEC99_1 \
			"UPDATE T_FILE SET flg=1 WHERE id IN (SELECT id FROM V_INDEX %s);"
#define   DELETE_EXEC99_1 \
			"DELETE FROM T_FILE WHERE flg IS NULL;"
#define   UPDATE_EXEC99_2 \
			"UPDATE T_FILE SET flg=NULL;"
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
#define   I_DEL               11
#define   I_DEL2              12
#define   I_REP               20
#define   I_RM                31
#define   I_RM2               32
//
// 現在時間
//
INT *$aiNow = { 0 };
//
// 検索Dir
//
WCS **$waDirList = { 0 };
INT $waDirListSize = 0;
//
// 入力DB
// -in=Str | -i=Str
//
WCS *$wpIn = L"";
MBS *$mpIn = "";
WCS *$wpInDbn = MEMDB;
MBS *$mpInDbn = "";
//
// 出力DB
// -out=Str | -o=Str
//
WCS *$wpOut = L"";
WCS *$wpOutDbn = L"";
MBS *$mpOutDbn = "";
//
// select
// -select=Str | -s=Str
//
WCS *$wpSelect = OP_SELECT_0;
INT $iSelectPosNumber = 0; // "LN"の配列位置
//
// where
// -where=Str | -w=Str
//
WCS *$wpWhere1 = L"";
WCS *$wpWhere2 = L"";
//
// group by
// -group=Str | -g=Str
//
WCS *$wpGroup = L"";
//
// order by
// -sort=Str | -st=Str
//
WCS *$wpSort = L"";
//
// ヘッダ情報を非表示
// -noheader | -nh
//
BOOL $bNoHeader = FALSE;
//
// フッタ情報を非表示
// -nofooter | -nf
//
BOOL $bNoFooter = FALSE;
//
// 出力をStrで囲む
// -quote=Str | -qt=Str
//
MBS *$mpQuote = "";
//
// 出力をStrで区切る
// -separate=Str | -sp=Str
//
MBS *$mpSeparate = " | ";
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
WCS *$wpMd = L"";
WCS *$wpMdOp = L"";
//
// mkdir Str & copy
// --copy=Str | --cp=Str
//
WCS *$wpCp = L"";
//
// mkdir Str & move Str
// --move=Str | --mv=Str
//
WCS *$wpMv = L"";
//
// mkdir Str & move Str & rmdir
// --move2=Str | --mv2=Str
//
WCS *$wpMv2 = L"";
//
// copy Str
// --extract=Str | --ext=Str
//
WCS *$wpExt = L"";
//
// move Str
// --extract2=Str | --ext2=Str
//
WCS *$wpExt2 = L"";
//
// --del  | --delete
// --del2 | --delete2
//
INT $iDel = 0;
//
// replace results with Str
// --replace=Str | --rep=Str
//
WCS *$wpRep = L"";
WCS *$wpRepOp = L"";
//
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
//  I_DEL  = --del
//  I_DEL2 = --del2
//  I_REP  = --rep
//  I_RM   = --rm
//  I_RM2  = --rm2
//
INT $iExec = 0;

INT
main()
{
	// lib_iwmutil 初期化
	iExecSec_init();       //=> $ExecSecBgn
	iCLI_getCommandLine(); //=> $CMD, $ARGC, $ARGV, $ARGS
	iConsole_EscOn();

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

	// 現在時間
	$aiNow = (INT*)idate_cjd_to_iAryYmdhns(idate_nowToCjd(TRUE));

	INT i1 = 0, i2 = 0;
	WCS *wp1 = 0, *wp2 = 0;

	// [0..]
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
			WCS **wa1 = iwa_split(wp1, L", ", TRUE);
				i2 = iwa_size(wa1);
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

	INT iArgsPos = 0;

	// [0..n]
	for(i1 = 0; i1 < $ARGC; i1++)
	{
		if(*$ARGV[i1] == '-')
		{
			break;
		}
		// Dir不在
		if(iFchk_typePathW($ARGV[i1]) != 1)
		{
			MBS *mp1 = W2U($ARGV[i1]);
				P("[Err] Dir(%d) '%s' は存在しない!\n", (i1 + 1), mp1);
			ifree(mp1);
		}
	}
	iArgsPos = i1;

	// $waDirList を作成
	if(iArgsPos)
	{
		// 条件別Dir取得
		$waDirList = ($iDepthMax == IMAX_PATH ? iwa_higherDir($ARGV) : iwa_getDir($ARGV));
		$waDirListSize = iwa_size($waDirList);
	}

	// [n..]
	for(i1 = iArgsPos; i1 < $ARGC; i1++)
	{
		// -i | -in
		if((wp1 = iCLI_getOptValue(i1, L"-i=", L"-in=")))
		{
			if(iFchk_typePathW(wp1) != 2)
			{
				MBS *mp1 = W2U(wp1);
					P("[Err] -in '%s' は存在しない!\n", mp1);
				ifree(mp1);
				imain_end();
			}
			else if($waDirListSize)
			{
				P2("[Err] Dir と -in は併用できない!");
				imain_end();
			}
			else
			{
				$wpIn = $wpInDbn = wp1;
				$mpIn = $mpInDbn = W2U(wp1);

				// -in のときは -recursive 自動付与
				$iDepthMin = 0;
				$iDepthMax = IMAX_PATH;
			}
		}

		// -o | -out
		if((wp1 = iCLI_getOptValue(i1, L"-o=", L"-out=")))
		{
			$wpOut = $wpOutDbn = wp1;
			$mpOutDbn = W2U(wp1);
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

		// --del | --delete
		if(iCLI_getOptMatch(i1, L"--del", L"--delete"))
		{
			$iDel = I_DEL;
		}

		// --del2 | --delete2
		if(iCLI_getOptMatch(i1, L"--del2", L"--delete2"))
		{
			$iDel = I_DEL2;
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
			WCS **wa1 = iwa_split(wp1, L",", TRUE);
				if(wa1[0])
				{
					// "LN"位置を求める
					WCS **wa2 = iwa_simplify(wa1, TRUE); // LN表示は１個のみなので重複排除
						$iSelectPosNumber = 0;
						while((wp2 = wa2[$iSelectPosNumber]))
						{
							if(iwb_cmppi(wp2, L"LN"))
							{
								break;
							}
							++$iSelectPosNumber;
						}
						if(! wp2)
						{
							$iSelectPosNumber = -1;
						}
						$wpSelect = iwa_join(wa2, L",");
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
			$bNoHeader = TRUE;
		}

		// -nf | -nofooter
		if(iCLI_getOptMatch(i1, L"-nf", L"-nofooter"))
		{
			$bNoFooter = TRUE;
		}

		// -qt | -quote
		if((wp1 = iCLI_getOptValue(i1, L"-qt=", L"-quote=")))
		{
			wp2 = iws_conv_escape(wp1);
				$mpQuote = W2U(wp2);
			ifree(wp2);
		}

		// -sp | -separate
		if((wp1 = iCLI_getOptValue(i1, L"-sp=", L"-separate=")))
		{
			wp2 = iws_conv_escape(wp1);
				$mpSeparate = W2U(wp2);
			ifree(wp2);
		}
	}

	// Err
	if(! $waDirListSize && ! *$wpIn)
	{
		P2("[Err] Dir もしくは -in を指定してください!");
		imain_end();
	}

	// --[exec] 関係を一括変換
	if(*$wpMd || *$wpCp || *$wpMv || *$wpMv2 || *$wpExt || *$wpExt2 || $iDel || *$wpRep || $iRm)
	{
		$bNoFooter = TRUE; // フッタ情報を非表示

		if(*$wpMd)
		{
			$iExec = I_MKDIR;
			$wpMdOp = iws_clone($wpMd);
		}
		else if(*$wpCp)
		{
			$iExec = I_CP;
			$wpMdOp = iws_clone($wpCp);
		}
		else if(*$wpMv)
		{
			$iExec = I_MV;
			$wpMdOp = iws_clone($wpMv);
		}
		else if(*$wpMv2)
		{
			$iExec = I_MV2;
			$wpMdOp = iws_clone($wpMv2);
		}
		else if(*$wpExt)
		{
			$iExec = I_EXT;
			$wpMdOp = iws_clone($wpExt);
		}
		else if(*$wpExt2)
		{
			$iExec = I_EXT2;
			$wpMdOp = iws_clone($wpExt2);
		}
		else if($iDel)
		{
			$iExec = $iDel;
		}
		else if(*$wpRep)
		{
			if(iFchk_typePathW($wpRep) != 2)
			{
				MBS *mp1 = W2U($wpRep);
					P("[Err] --replace '%s' は存在しない!\n", mp1);
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
			wp1 = iFget_ApathW($wpMdOp);
				$wpMdOp = iws_cutYenR(wp1);
			ifree(wp1);
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
		// Dir削除時必要なものは "order by desc"
		ifree($wpSort);
		$wpSort = iws_clone(L"lower(path) desc");
	}
	else if(! *$wpSort)
	{
		ifree($wpSort);
		$wpSort = iws_clone(L"lower(dir),lower(name)");
	}
	else
	{
		// path, dir, name, ext
		// ソートは、大文字・小文字を区別しない
		wp1 = iws_clone($wpSort);
		ifree($wpSort);
		wp2 = iws_replace(wp1, L"path", L"lower(path)", FALSE);
		ifree(wp1);
		wp1 = iws_replace(wp2, L"dir", L"lower(dir)", FALSE);
		ifree(wp2);
		wp2 = iws_replace(wp1, L"name", L"lower(name)", FALSE);
		ifree(wp1);
		$wpSort = iws_clone(wp1);
		ifree(wp1);
	}

	// SQL作成 UTF-8（Sqlite3対応）
	wp1 = U2W(SELECT_VIEW);
	wp2 = iws_sprintf(wp1, $wpSelect, $wpWhere2, $wpGroup, $wpSort);
		$sqlU = W2U(wp2);
	ifree(wp2);
	ifree(wp1);

	// -in DBを指定
	if(sqlite3_open16($wpInDbn, &$iDbs))
	{
		P("[Err] -in '%s' を開けない!\n", $mpInDbn);
		sqlite3_close($iDbs); // ErrでもDB解放
		imain_end();
	}
	else
	{
		// UTF-8 出力領域
		$mpBuf = icalloc_MBS(BUF_SIZE_MAX + BUF_SIZE_DMZ);
		$mpBufE = $mpBuf;
		$mpBufMax = $mpBuf + BUF_SIZE_MAX;

		// DB構築
		if(! *$wpIn)
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
				// 検索データ DB書込
				for(i2 = 0; (wp1 = $waDirList[i2]); i2++)
				{
					$iStepCnt = iwn_len(wp1);
					// 本処理
					ifind10(FI, F, wp1, 0);
				}
				// 経過表示をクリア
				ifind10_CallCnt(0);
				// トランザクション終了
				sql_exec($iDbs, "COMMIT", 0);
				// 後処理
				sqlite3_finalize($stmt2);
				sqlite3_finalize($stmt1);
			iFinfo_freeW(FI);
		}
		// 結果
		if(*$wpOut)
		{
			// 存在する場合、削除
			DeleteFileW($wpOutDbn);
			// $wpIn, $wpOut両指定、別ファイル名のとき
			if(*$wpIn)
			{
				CopyFileW($wpInDbn, OLDDB, FALSE);
			}
			// WHERE Str のとき不要データ削除
			if(*$wpWhere1)
			{
				sql_exec($iDbs, "BEGIN", 0);         // トランザクション開始
				MBS *mp1 = W2U($wpWhere1);
				MBS *mp2 = ims_cats(2, "WHERE ", mp1);
					sprintf($mpBuf, UPDATE_EXEC99_1, mp2);
				ifree(mp2);
				ifree(mp1);
				sql_exec($iDbs, $mpBuf, 0);          // フラグを立てる
				sql_exec($iDbs, DELETE_EXEC99_1, 0); // 不要データ削除
				sql_exec($iDbs, UPDATE_EXEC99_2, 0); // フラグ初期化
				sql_exec($iDbs, "COMMIT", 0);        // トランザクション終了
				sql_exec($iDbs, "VACUUM", 0);        // VACUUM
			}
			// $wpIn, $wpOut 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
			sql_saveOrLoadMemdb($iDbs, (*$wpIn ? $wpInDbn : $wpOutDbn), TRUE);
			// outDb
			sql_exec($iDbs, "SELECT LN FROM V_INDEX;", sql_result_countOnly); // "SELECT *" は遅い
		}
		else
		{
			if($iExec)
			{
				sql_exec($iDbs, $sqlU, sql_result_exec);
			}
			else
			{
				// カラム名表示
				if(! $bNoHeader)
				{
					MBS *mp1 = W2U($wpSelect);
					MBS *mp2 = ims_cats(3, "SELECT ", mp1, " FROM V_INDEX WHERE id=1;");
						sql_exec($iDbs, mp2, sql_columnName);
					ifree(mp2);
					ifree(mp1);
				}
				// 結果出力
				sql_exec($iDbs, $sqlU, sql_result_std);
			}
		}
		// DB解放
		sqlite3_close($iDbs);
		// $wpIn, $wpOut ファイル名をswap
		if(*$wpIn)
		{
			MoveFileW($wpInDbn, $wpOutDbn);
			MoveFileW(OLDDB, $wpInDbn);
		}
		// 作業用ファイル削除
		DeleteFileW(OLDDB);
	}

	// フッタ部
	if(! $bNoFooter)
	{
		print_footer();
	}

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	imain_end();
}

WCS
*sql_escape(
	WCS *pW
)
{
	WCS *wp1 = iws_clone(pW);
	WCS *wp2 = iws_replace(wp1, L";", L" ", FALSE); // ";" => " " SQLインジェクション回避
	ifree(wp1);
	wp1 = iws_replace(wp2, L"*", L"%", FALSE); // "*" => "%"
	ifree(wp2);
	wp2 = iws_replace(wp1, L"?", L"_", FALSE); // "?" => "_"
	ifree(wp1);
	// [] を日付に変換
	wp1 = idate_replace_format_ymdhns(
			wp2,
			L"[", L"]",
			L"'",
			$aiNow[0],
			$aiNow[1],
			$aiNow[2],
			$aiNow[3],
			$aiNow[4],
			$aiNow[5]
	);
	ifree(wp2);
	return wp1;
}

/* 2016-08-19
	【留意】Dirの表示について
		d:\aaa\ 以下の、
			d:\aaa\bbb\
			d:\aaa\ccc\
			d:\aaa\ddd.txt
		を表示したときの違いは次のとおり。

		①iwmls.exe（ls、dir互換）
			d:\aaa\bbb\
			d:\aaa\ccc\
			d:\aaa\ddd.txt

		②iwmfind.exe（BaseDirとFileを表示）
			d:\aaa\
			d:\aaa\ddd.txt

		※DirとFileを別テーブルで管理／joinして使用するため、このような仕様にならざるを得ない。
*/
VOID
ifind10(
	$struct_iFinfoW *FI,
	WIN32_FIND_DATAW F,
	WCS *dir,
	INT depth
)
{
	if(depth > $iDepthMax)
	{
		return;
	}
	// FI->fullnameW 末尾に "*" を付与
	UINT dirLen = iwn_cpy(FI->fullnameW, dir);
		*(FI->fullnameW + dirLen)     = '*';
		*(FI->fullnameW + dirLen + 1) = 0;
	HANDLE hfind = FindFirstFileW(FI->fullnameW, &F);
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
			ifind21(dir, dirId, depth);
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
					WCS *wp1 = iws_clone(FI->fullnameW);
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
	ifind10_CallCnt(++$iCall_ifind10);
}

VOID
ifind10_CallCnt(
	INT iCnt // 0=クリア
)
{
	if(! iCnt)
	{
		// 行消去／カーソル戻す
		fputs("\r\033[0K\033[?25h", stderr);
		return;
	}

	if(iCnt >= BUF_MAXCNT)
	{
		PRGB21();
		// 行消去／カーソル消す／カウント描画
		fprintf(stderr, "\r\033[0K\033[?25l> %lld", $lAllCnt);
		$iCall_ifind10 = 0;
	}
}

VOID
ifind21(
	WCS *dir,
	INT dirId,
	INT depth
)
{
	sqlite3_reset($stmt1);
		sqlite3_bind_int64($stmt1,  1, dirId);
		sqlite3_bind_text16($stmt1, 2, dir, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt1,    3, depth);
		sqlite3_bind_int($stmt1,    4, $iStepCnt);
	sqlite3_step($stmt1);
}

VOID
ifind22(
	$struct_iFinfoW *FI,
	INT dirId,
	WCS *fname
)
{
	sqlite3_reset($stmt2);
		sqlite3_bind_int64($stmt2,  1, ++$lAllCnt);
		sqlite3_bind_int64($stmt2,  2, dirId);
		sqlite3_bind_text16($stmt2, 3, fname, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt2,    4, FI->uAttr);
		sqlite3_bind_double($stmt2, 5, FI->cjdCtime);
		sqlite3_bind_double($stmt2, 6, FI->cjdMtime);
		sqlite3_bind_double($stmt2, 7, FI->cjdAtime);
		sqlite3_bind_int64 ($stmt2, 8, FI->iFsize);
	sqlite3_step($stmt2);
}

VOID
sql_exec(
	sqlite3 *db,
	CONST MBS *sql,
	sqlite3_callback cb
)
{
	MBS *p_err = 0; // SQLiteが使用
	$lRowCnt = 0;

	if(sqlite3_exec(db, sql, cb, 0, &p_err))
	{
		P("[Err] 構文エラー\n    %s\n    %s\n", p_err, sql);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}

	// sql_result_std() 対応
	if($mpBufE > $mpBuf)
	{
		QP2($mpBuf, ($mpBufE - $mpBuf));
	}
}

INT
sql_columnName(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	PRGB21();
	INT i1 = 0;
	while(TRUE)
	{
		P1("[");
		P1(sColumnNames[i1]);
		P1("]");
		if(++i1 < iColumnCount)
		{
			P1($mpSeparate);
		}
		else
		{
			break;
		}
	}
	PRGB00();
	NL();
	return SQLITE_OK;
}

INT
sql_result_std(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	++$lRowCnt;
	INT i1 = 0;
	while(i1 < iColumnCount)
	{
		// [LN]
		if(i1 == $iSelectPosNumber)
		{
			$mpBufE += sprintf(
				$mpBufE,
				"%s%lld%s",
				$mpQuote,
				$lRowCnt,
				$mpQuote
			);
		}
		else
		{
			$mpBufE += sprintf(
				$mpBufE,
				"%s%s%s",
				$mpQuote,
				sColumnValues[i1],
				$mpQuote
			);
		}
		++i1;
		$mpBufE += imn_cpy($mpBufE, (i1 == iColumnCount ? "\n" : $mpSeparate));
	}
	// Buf を Print
	if($mpBufE > $mpBufMax)
	{
		QP2($mpBuf, ($mpBufE - $mpBuf));
		$mpBufE = $mpBuf;
	}
	return SQLITE_OK;
}

INT
sql_result_countOnly(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	++$lRowCnt;
	return SQLITE_OK;
}

INT
sql_result_exec(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	INT i1 = 0;
	WCS *wp1 = 0, *wp2 = 0, *wp3 = 0, *wp4 = 0, *wp5 = 0;
	switch($iExec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// $wpMdOp\以下には、$waDirList\以下のDirを作成
			i1  = atoi(sColumnValues[0]); // step_cnt
			wp1 = U2W(sColumnValues[1]);  // dir + "\"
			wp2 = U2W(sColumnValues[2]);  // name
			wp3 = U2W(sColumnValues[3]);  // path
			wp4 = iws_cats(3, $wpMdOp, L"\\", (wp1 + i1));
			wp5 = iws_cats(2, wp4, wp2);
				// mkdir
				if(imk_dirW(wp4))
				{
					P1("md\t=> ");
					P2W(wp4);
					++$lRowCnt;
				}
				// 優先処理
				if($iExec == I_CP)
				{
					if(CopyFileW(wp3, wp5, FALSE))
					{
						P1("cp\t<= ");
						P2(sColumnValues[3]);
						++$lRowCnt;
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
					if(iFchk_typePathW(wp5))
					{
						DeleteFileW(wp5);
					}
					if(MoveFileW(wp3, wp5))
					{
						P1("mv\t<= ");
						P2(sColumnValues[3]);
						++$lRowCnt;
					}
					// rmdir
					if($iExec == I_MV2)
					{
						if(RemoveDirectoryW(wp1))
						{
							P1("rd\t=> ");
							P2(sColumnValues[1]);
							++$lRowCnt;
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
			wp1 = U2W(sColumnValues[0]); // path
			wp2 = U2W(sColumnValues[1]); // name
				// mkdir
				if(imk_dirW($wpMdOp))
				{
					P1("md\t=> ");
					P2W($wpMdOp);
					++$lRowCnt;
				}
				// 先
				wp3 = iws_cats(3, $wpMdOp, L"\\", wp2);
				// I_EXT, I_EXT2共、同名Fileは上書き
				if($iExec == I_EXT)
				{
					if(CopyFileW(wp1, wp3, FALSE))
					{
						P1("cp\t<= ");
						P2W(wp3);
						++$lRowCnt;
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
					if(iFchk_typePathW(wp3))
					{
						DeleteFileW(wp3);
					}
					if(MoveFileW(wp1, wp3))
					{
						P1("mv\t<= ");
						P2W(wp3);
						++$lRowCnt;
					}
				}
				else
				{
				}
			ifree(wp3);
			ifree(wp2);
			ifree(wp1);
		break;

		case(I_DEL):  // --delete
		case(I_DEL2): // --delete2
			wp1 = U2W(sColumnValues[0]); // path
			i1 = atoi(sColumnValues[2]);
				// ReadOnly属性(1)を解除
				if((i1 & FILE_ATTRIBUTE_READONLY))
				{
					SetFileAttributesW(wp1, FALSE);
				}
				// Fileをゴミ箱へ移動
				if(! (i1 & FILE_ATTRIBUTE_DIRECTORY) && imv_trashW(wp1))
				{
					P1("delFile\t=> ");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
				// Dirをゴミ箱へ移動
				if($iExec == I_DEL2 && (i1 & FILE_ATTRIBUTE_DIRECTORY) && imv_trashW(wp1))
				{
					P1("delDir\t=> ");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
			ifree(wp1);
		break;

		case(I_REP): // --replace
			wp1 = U2W(sColumnValues[0]); // type
			wp2 = U2W(sColumnValues[1]); // path
				if(*wp1 == 'f' && CopyFileW($wpRepOp, wp2, FALSE))
				{
					P1("rep\t=> ");
					P2(sColumnValues[1]);
					++$lRowCnt;
				}
			ifree(wp2);
			ifree(wp1);
		break;

		case(I_RM):  // --remove
		case(I_RM2): // --remove2
			wp1 = U2W(sColumnValues[0]); // path
				// ReadOnly属性(1)を解除
				if((atoi(sColumnValues[2]) & FILE_ATTRIBUTE_READONLY))
				{
					SetFileAttributesW(wp1, FALSE);
				}
				// File削除
				if(DeleteFileW(wp1))
				{
					P1("rm\t=> ");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
				// 空Dir削除
				if($iExec == I_RM2)
				{
					wp2 = U2W(sColumnValues[1]); // dir
						// 空Dirである
						if(RemoveDirectoryW(wp2))
						{
							P1("rd\t=> ");
							P2(sColumnValues[1]);
							++$lRowCnt;
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
	WCS *ofn,        // Filename
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
	PRGB21();
		LN();
		P(
			"-- %lld row%s in set ( %.3f sec)\n",
			$lRowCnt,
			($lRowCnt > 1 ? "s" : ""), // 複数形
			iExecSec_next()
		);
	PRGB22();
		P2("--");
		for(INT _i1 = 0; _i1 < $waDirListSize; _i1++)
		{
			MBS *mp1 = W2U($waDirList[_i1]);
				P("--  '%s'\n", mp1);
			ifree(mp1);
		}
		P("--  '%s'\n", $sqlU);
		P("--  -depth     '%d,%d'\n", $iDepthMin, $iDepthMax);
		if(*$mpIn)
		{
			P("--  -in        '%s'\n", $mpIn);
		}
		if(*$mpOutDbn)
		{
			P("--  -out       '%s'\n", $mpOutDbn);
		}
		if($bNoFooter)
		{
			P2("--  -nofooter");
		}
		if(*$mpQuote)
		{
			P("--  -quote     '%s'\n", $mpQuote);
		}
		if(*$mpSeparate)
		{
			P("--  -separate  '%s'\n", $mpSeparate);
		}
		P2("--");
	PRGB00();
}

VOID
print_version()
{
	PRGB92();
	LN();
	P (" %s\n", IWM_COPYRIGHT);
	P ("   Ver.%s+%s+SQLite%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION, SQLITE_VERSION);
	LN();
	PRGB00();
}

VOID
print_help()
{
	MBS *_cmd = W2U($CMD);
	MBS *_select0 = W2U(OP_SELECT_0);

	print_version();
	PRGB01();
	P2("\033[48;2;50;50;200m ファイル検索 \033[0m");
	NL();
	PRGB02();
	P ("\033[48;2;200;50;50m %s [Dir] [Option] \033[0m\n\n", _cmd);
	PRGB11();
	P1(" (例１) ");
	PRGB91();
	P2("検索");
	P ("   %s \033[38;2;255;150;150mDir \033[38;2;150;150;255m-r -s=\"LN,path,size\" -w=\"name like '*.exe'\"\n\n", _cmd);
	PRGB11();
	P1(" (例２) ");
	PRGB91();
	P2("検索結果をファイルへ保存");
	P ("   %s \033[38;2;255;150;150mDir1 Dir2 \033[38;2;150;150;255m-r -o=File\n\n", _cmd);
	PRGB11();
	P1(" (例３) ");
	PRGB91();
	P2("検索対象をファイルから読込");
	P ("   %s \033[38;2;150;150;255m-i=File\n\n", _cmd);
	PRGB02();
	P2("\033[48;2;200;50;50m [Dir] \033[0m");
	PRGB91();
	P2("   検索対象Dir");
	PRGB11();
	P1("   (例) ");
	PRGB91();
	P2("\"c:\\\" \".\" (複数指定可)");
	NL();
	PRGB02();
	P2("\033[48;2;200;50;50m [Option] \033[0m");
	PRGB13();
	P2(" [基本操作]");
	PRGB21();
	P2("   -recursive | -r");
	PRGB91();
	P2("       全階層を検索");
	NL();
	PRGB21();
	P2("   -depth=Num1,Num2 | -d=Num1,Num2");
	PRGB91();
	P2("       検索する階層を指定");
	PRGB11();
	P1("       (例１) ");
	PRGB91();
	P2("-d=\"1\"");
	P2("              1階層のみ検索");
	PRGB11();
	P1("       (例２) ");
	PRGB91();
	P2("-d=\"3\",\"5\"");
	P2("              3～5階層を検索");
	NL();
	PRGB22();
	P2("       ※１ CurrentDir は \"0\"");
	P2("       ※２ -depth と -where における depth の挙動の違い");
	PRGB91();
	P2("            \033[38;2;255;150;150m◇速い\033[39m -depth は指定された階層のみ検索を行う");
	P2("            \033[38;2;150;150;255m◇遅い\033[39m -where内でのdepthによる検索は全階層のDir／Fileに対して行う");
	NL();
	PRGB21();
	P2("   -out=File | -o=File");
	PRGB91();
	P2("       出力ファイル");
	NL();
	PRGB21();
	P2("   -in=File | -i=File");
	PRGB91();
	P2("       入力ファイル");
	PRGB12();
	P2("       検索対象Dirと併用できない");
	NL();
	PRGB13();
	P2(" [SQL関連]");
	PRGB21();
	P2("   -select=Column1,Column2,... | -s=Column1,Column2,...");
	PRGB91();
	P2("       LN        (Num) 連番／1回のみ指定可");
	P2("       path      (Str) dir\\name");
	P2("       dir       (Str) ディレクトリ名");
	P2("       name      (Str) ファイル名");
	P2("       depth     (Num) ディレクトリ階層 = 0..");
	P2("       type      (Str) ディレクトリ = d／ファイル = f");
	P2("       attr_num  (Num) 属性");
	P2("       attr      (Str) 属性 \"[d|f][r|-][h|-][s|-][a|-]\"");
	P2("                       [dir|file][read-only][hidden][system][archive]");
	P2("       size      (Num) ファイルサイズ = byte");
	P2("       ctime_cjd (Num) 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5");
	P2("       ctime     (Str) 作成日時     \"yyyy-mm-dd hh:nn:ss\"");
	P2("       mtime_cjd (Num) 更新日時     ctime_cjd参照");
	P2("       mtime     (Str) 更新日時     ctime参照");
	P2("       atime_cjd (Num) アクセス日時 ctime_cjd参照");
	P2("       atime     (Str) アクセス日時 ctime参照");
	P2("       *         全項目表示");
	NL();
	PRGB22();
	P2("       ※１ Column指定なしの場合");
	PRGB91();
	P ("            %s を表示\n", _select0);
	PRGB22();
	P2("       ※２ SQLite演算子／関数を利用可能");
	PRGB91();
	P2("            abs(X)  changes()  char(X1,X2,...,XN)  coalesce(X,Y,...)  format(FORMAT,...)");
	P2("            glob(X,Y)  hex(X)  ifnull(X,Y)  iif(X,Y,Z)  instr(X,Y)  last_insert_rowid()  length(X)");
	P2("            like(X,Y)  like(X,Y,Z)  likelihood(X,Y)  likely(X)  load_extension(X)  load_extension(X,Y)");
	P2("            lower(X)  ltrim(X)  ltrim(X,Y)  max(X,Y,...)  min(X,Y,...)  nullif(X,Y)  printf(FORMAT,...)");
	P2("            quote(X)  random()  randomblob(N)  replace(X,Y,Z)  round(X)  round(X,Y)");
	P2("            rtrim(X)  rtrim(X,Y)  sign(X)  soundex(X)");
	P2("            sqlite_compileoption_get(N)  sqlite_compileoption_used(X)");
	P2("            sqlite_offset(X)  sqlite_source_id()  sqlite_version()");
	P2("            substr(X,Y)  substr(X,Y,Z)  substring(X,Y)  substring(X,Y,Z)");
	P2("            total_changes()  trim(X)  trim(X,Y)  typeof(X)  unicode(X)  unlikely(X)  upper(X)  zeroblob(N)");
	PRGB13();
	P2("           (参考) http://www.sqlite.org/lang_corefunc.html");
	NL();
	PRGB21();
	P2("   -where=Str | -w=Str");
	PRGB11();
	P1("       (例１) ");
	PRGB91();
	P2("\"size <= 100 or size > 1000000\"");
	PRGB11();
	P1("       (例２) ");
	PRGB91();
	P2("\"type = 'f' and name like 'abc??.*'\"");
	P2("              '?' '_' は任意の1文字");
	P2("              '*' '%' は任意の0文字以上");
	PRGB11();
	P1("       (例３) ");
	PRGB91();
	P2("基準日 \"2010-12-10 12:30:00\" のとき");
	P2("              \"ctime >= [-10d]\"  : ctime >= '2010-11-30 12:30:00'");
	P2("              \"ctime >= [-10D]\"  : ctime >= '2010-11-30 00:00:00'");
	P2("              \"ctime >= [-10d%]\" : ctime >= '2010-11-30 %'");
	P2("              \"ctime like [%]\"   : ctime like '2010-12-10 %'");
	P2("              (年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s");
	NL();
	PRGB21();
	P2("   -group=Str | -g=Str");
	PRGB11();
	P1("       (例) ");
	PRGB91();
	P2("-g=\"Str1, Str2\"");
	P2("            Str1とStr2をグループ毎にまとめる");
	NL();
	PRGB21();
	P2("   -sort=\"Str [ASC|DESC]\" | -st=\"Str [ASC|DESC]\"");
	PRGB11();
	P1("       (例) ");
	PRGB91();
	P2("-st=\"Str1 ASC, Str2 DESC\"");
	P2("            Str1を順ソート, Str2を逆順ソート");
	NL();
	PRGB13();
	P2(" [出力フォーマット]");
	PRGB21();
	P2("   -noheader | -nh");
	PRGB91();
	P2("       ヘッダ情報を表示しない");
	NL();
	PRGB21();
	P2("   -nofooter | -nf");
	PRGB91();
	P2("       フッタ情報を表示しない");
	NL();
	PRGB21();
	P2("   -quote=Str | -qt=Str");
	PRGB91();
	P2("       囲み文字");
	PRGB11();
	P1("       (例) ");
	PRGB91();
	P2("-qt=\"'\"");
	NL();
	PRGB21();
	P2("   -separate=Str | -sp=Str");
	PRGB91();
	P2("       区切り文字");
	PRGB11();
	P1("       (例) ");
	PRGB91();
	P2("-sp=\"\\t\"");
	NL();
	PRGB13();
	P2(" [出力結果を操作]");
	PRGB21();
	P2("   --mkdir=Dir | --md=Dir");
	PRGB91();
	P2("       検索結果のDirをコピー作成する (-recursive のとき 階層維持)");
	NL();
	PRGB21();
	P2("   --copy=Dir | --cp=Dir");
	PRGB91();
	P2("       --mkdir + 検索結果をDirにコピーする (-recursive のとき 階層維持)");
	NL();
	PRGB21();
	P2("   --move=Dir | --mv=Dir");
	PRGB91();
	P2("       --mkdir + 検索結果をDirに移動する (-recursive のとき 階層維持)");
	NL();
	PRGB21();
	P2("   --move2=Dir | --mv2=Dir");
	PRGB91();
	P2("       --mkdir + --move + 移動元の空Dirを削除する (-recursive のとき 階層維持)");
	NL();
	PRGB21();
	P2("   --extract=Dir | --ext=Dir");
	PRGB91();
	P2("       --mkdir + 検索結果ファイルのみ抽出しDirにコピーする");
	PRGB12();
	P2("       階層を維持しない／同名ファイルは上書き");
	NL();
	PRGB21();
	P2("   --extract2=Dir | --ext2=Dir");
	PRGB91();
	P2("       --mkdir + 検索結果ファイルのみ抽出しDirに移動する");
	PRGB12();
	P2("       階層を維持しない／同名ファイルは上書き");
	NL();
	PRGB21();
	P2("   --remove | --rm");
	PRGB91();
	P2("       検索結果のFileのみ削除する（Dirは削除しない）");
	NL();
	PRGB21();
	P2("   --remove2 | --rm2");
	PRGB91();
	P2("       --remove + 空Dirを削除する");
	NL();
	PRGB21();
	P2("   --delete | --del");
	PRGB91();
	P2("       検索結果のFileのみゴミ箱へ移動する（Dirは移動しない）");
	NL();
	PRGB21();
	P2("   --delete2 | --del2");
	PRGB91();
	P2("       --delete + Dirをゴミ箱へ移動する");
	NL();
	PRGB21();
	P2("   --replace=File | --rep=File");
	PRGB91();
	P2("       検索結果(複数) をFileの内容で置換(上書き)する／ファイル名は変更しない");
	PRGB11();
	P1("       (例) ");
	PRGB91();
	P2("-w=\"name like 'foo.txt'\" --rep=\".\\foo.txt\"");
	NL();
	PRGB92();
	LN();
	PRGB00();

	ifree(_select0);
	ifree(_cmd);
}
