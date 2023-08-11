//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmfind5_20230809"
#define   IWM_COPYRIGHT       "Copyright (C)2009-2023 iwm-iwama"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"
#include "sqlite3.h"

INT       main();
WS        *sql_escape(WS *pW);
VOID      ifind10($struct_iFinfoW *FI, WIN32_FIND_DATAW F, WS *dir, INT depth);
VOID      ifind10_CallCnt(INT iCnt);
VOID      ifind21(WS *dir, INT dirId, INT depth);
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

MS        *$mpBuf = 0;         // Tmp文字列
MS        *$mpBufEnd = 0;      // Tmp文字列末尾
UINT      $uBufSize = 1000000; // Tmp初期サイズ
INT       $iDirId = 0;         // Dir数
INT64     $lAllCnt = 0;        // 検索数
INT       $iCall_ifind10 = 0;  // ifind10()が呼ばれた回数
INT       $iStepCnt = 0;       // CurrentDir位置
INT64     $lRowCnt = 0;        // 処理行数 <= NTFSの最大ファイル数(2^32 - 1 = 4,294,967,295)
MS        *$sqlU = 0;
sqlite3   *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;

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
// --del  | --delete
// --del2 | --delete2
//
INT $iDel = 0;
//
// replace results with Str
// --replace=Str | --rep=Str
//
WS *$wpRep = L"";
WS *$wpRepOp = L"";
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
	// lib_iwmutil2 初期化
	imain_begin();

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
		if(*$ARGV[i1] != '-' && iFchk_typePathW($ARGV[i1]) != 1)
		{
			MS *mp1 = W2M($ARGV[i1]);
				P("%s[Err] Dir '%s' は存在しない!%s\n", ICLR_ERR1, mp1, ICLR_RESET);
			ifree(mp1);
		}
	}

	// $waDirList を作成
	$waDirList = ($iDepthMax == IMAX_PATH ? iwaa_higherDir($ARGV) : iwaa_getDir($ARGV));
	$waDirListSize = iwan_size($waDirList);

	// Main Loop
	for(i1 = 0; i1 < $ARGC; i1++)
	{
		// -i | -in
		if((wp1 = iCLI_getOptValue(i1, L"-i=", L"-in=")))
		{
			if(iFchk_typePathW(wp1) != 2)
			{
				MS *mp1 = W2M(wp1);
					P("%s[Err] -in '%s' は存在しない!%s\n", ICLR_ERR1, mp1, ICLR_RESET);
				ifree(mp1);
				imain_end();
			}
			else if($waDirListSize)
			{
				P("%s[Err] Dir と -in は併用できない!%s\n", ICLR_ERR1, ICLR_RESET);
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
			WS **wa1 = iwaa_split(wp1, L",", TRUE);
				if(wa1[0])
				{
					// "LN"位置を求める
					WS **wa2 = iwaa_simplify(wa1, TRUE); // LN表示は１個のみなので重複排除
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
				$mpQuote = W2M(wp2);
			ifree(wp2);
		}

		// -sp | -separate
		if((wp1 = iCLI_getOptValue(i1, L"-sp=", L"-separate=")))
		{
			wp2 = iws_conv_escape(wp1);
				$mpSeparate = W2M(wp2);
			ifree(wp2);
		}
	}

	// --[exec] 関係を一括変換
	if(*$wpMd || *$wpCp || *$wpMv || *$wpMv2 || *$wpExt || *$wpExt2 || $iDel || *$wpRep || $iRm)
	{
		$bNoFooter = TRUE; // フッタ情報を非表示

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
		else if($iDel)
		{
			$iExec = $iDel;
		}
		else if(*$wpRep)
		{
			if(iFchk_typePathW($wpRep) != 2)
			{
				MS *mp1 = W2M($wpRep);
					P("%s[Err] --replace '%s' は存在しない!%s\n", ICLR_ERR1, mp1, ICLR_RESET);
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

	// SQL作成 UTF-8（Sqlite3対応）
	wp1 = M2W(SELECT_VIEW);
	wp2 = iws_sprintf(wp1, $wpSelect, $wpWhere2, $wpGroup, $wpSort);
		$sqlU = W2M(wp2);
	ifree(wp2);
	ifree(wp1);

	// -in DBを指定
	if(sqlite3_open16($wpInDbn, &$iDbs))
	{
		mp1 = W2M($wpInDbn);
			P("%s[Err] -in '%s' を開けない!%s\n", ICLR_ERR1, mp1, ICLR_RESET);
		ifree(mp1);
		sqlite3_close($iDbs); // ErrでもDB解放
		imain_end();
	}
	else
	{
		// UTF-8 出力領域
		$mpBuf = icalloc_MS($uBufSize);
		$mpBufEnd = $mpBuf;

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
					sprintf($mpBuf, UPDATE_EXEC99_1, mp2);
				ifree(mp2);
				ifree(mp1);
				sql_exec($iDbs, $mpBuf, 0);          // フラグを立てる
				sql_exec($iDbs, DELETE_EXEC99_1, 0); // 不要データ削除
				sql_exec($iDbs, UPDATE_EXEC99_2, 0); // フラグ初期化
				sql_exec($iDbs, "COMMIT", 0);        // トランザクション終了
				sql_exec($iDbs, "VACUUM", 0);        // VACUUM
			}
			// $wpInTmp, $wpOutDbn 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
			sql_saveOrLoadMemdb($iDbs, (*$wpInTmp ? $wpInDbn : $wpOutDbn), TRUE);
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
					MS *mp1 = W2M($wpSelect);
					MS *mp2 = ims_cats(3, "SELECT ", mp1, " FROM V_INDEX WHERE id=1;");
						sql_exec($iDbs, mp2, sql_columnName);
					ifree(mp2);
					ifree(mp1);
				}
				// 結果出力
				sql_exec($iDbs, $sqlU, sql_result_std);
				ifree($mpBuf);
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
	if(! $bNoFooter)
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
					WS *wp1 = iws_clone(FI->fullnameW);
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

	if(iCnt >= 5000)
	{
		P(ICLR_OPT21);
		// 行消去／カーソル消す／カウント描画
		fprintf(stderr, "\r\033[0K\033[?25l> %lld", $lAllCnt);
		$iCall_ifind10 = 0;
	}
}

VOID
ifind21(
	WS *dir,
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
	WS *fname
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
		sqlite3_bind_int64 ($stmt2, 8, FI->uFsize);
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
	$lRowCnt = 0;

	if(sqlite3_exec(db, sql, cb, 0, &p_err))
	{
		P("%s[Err] 構文エラー%s\n      %s\n      %s\n", ICLR_ERR1, ICLR_RESET, p_err, sql);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}

	// sql_result_std() 対応
	QP($mpBuf, ($mpBufEnd - $mpBuf));
}

INT
sql_columnName(
	VOID *option,
	INT iColumnCount,
	MS **sColumnValues,
	MS **sColumnNames
)
{
	P(ICLR_OPT21);
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
	P(ICLR_RESET);
	NL();
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
	++$lRowCnt;
	INT i1 = 0;
	while(i1 < iColumnCount)
	{
		// [LN]
		if(i1 == $iSelectPosNumber)
		{
			$mpBufEnd += imn_cpy($mpBufEnd, $mpQuote);
			$mpBufEnd += sprintf($mpBufEnd, "%lld", $lRowCnt);
			$mpBufEnd += imn_cpy($mpBufEnd, $mpQuote);
		}
		else
		{
			$mpBufEnd += imn_cpy($mpBufEnd, $mpQuote);
			UINT64 _u1 = $mpBufEnd - $mpBuf + strlen(sColumnValues[i1]);
			if(_u1 > $uBufSize)
			{
				UINT64 _u2 = $mpBufEnd - $mpBuf;
				// Realloc
				if(_u1 < 5000000)
				{
					$uBufSize += _u1;
					$mpBuf = irealloc_MS($mpBuf, $uBufSize);
				}
				// Buf を Print
				QP($mpBuf, _u2);
				*$mpBuf = 0;
				$mpBufEnd = $mpBuf;
			}
			$mpBufEnd += imn_cpy($mpBufEnd, sColumnValues[i1]);
			$mpBufEnd += imn_cpy($mpBufEnd, $mpQuote);
		}
		++i1;
		$mpBufEnd += imn_cpy($mpBufEnd, (i1 == iColumnCount ? "\n" : $mpSeparate));
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
	++$lRowCnt;
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
					P1("md\t");
					P2W(wp4);
					++$lRowCnt;
				}
				// 優先処理
				if($iExec == I_CP)
				{
					if(CopyFileW(wp3, wp5, FALSE))
					{
						P1("cp\t");
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
						P1("mv\t");
						P2(sColumnValues[3]);
						++$lRowCnt;
					}
					// rmdir
					if($iExec == I_MV2)
					{
						if(RemoveDirectoryW(wp1))
						{
							P1("rd\t");
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
			wp1 = M2W(sColumnValues[0]); // path
			wp2 = M2W(sColumnValues[1]); // name
				// mkdir
				if(imk_dirW($wpMdOp))
				{
					P1("md\t");
					P2W($wpMdOp);
					++$lRowCnt;
				}
				// 先
				wp3 = iws_cats(2, $wpMdOp, wp2);
				// I_EXT, I_EXT2共、同名Fileは上書き
				if($iExec == I_EXT)
				{
					if(CopyFileW(wp1, wp3, FALSE))
					{
						P1("cp\t");
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
						P1("mv\t");
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
					P1("delFile\t");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
				// Dirをゴミ箱へ移動
				if($iExec == I_DEL2 && (i1 & FILE_ATTRIBUTE_DIRECTORY) && imv_trashW(wp1))
				{
					P1("delDir\t");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
			ifree(wp1);
		break;

		case(I_REP): // --replace
			wp1 = M2W(sColumnValues[0]); // type
			wp2 = M2W(sColumnValues[1]); // path
				if(*wp1 == 'f' && CopyFileW($wpRepOp, wp2, FALSE))
				{
					P1("rep\t");
					P2(sColumnValues[1]);
					++$lRowCnt;
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
					P1("rm\t");
					P2(sColumnValues[0]);
					++$lRowCnt;
				}
				// 空Dir削除
				if($iExec == I_RM2)
				{
					wp2 = M2W(sColumnValues[1]); // dir
						// 空Dirである
						if(RemoveDirectoryW(wp2))
						{
							P1("rd\t");
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
	P(ICLR_OPT21);
	LN(80);
	P(
		"-- %lld row%s in set ( %.3f sec)\n",
		$lRowCnt,
		($lRowCnt > 1 ? "s" : ""), // 複数形
		iExecSec_next()
	);
	P(ICLR_OPT22);
	P2("--");
	for(INT _i1 = 0; _i1 < $waDirListSize; _i1++)
	{
		mp1 = W2M($waDirList[_i1]);
			P("--  '%s'\n", mp1);
		ifree(mp1);
	}
	P("--  '%s'\n", $sqlU);
	P("--  -depth     '%d,%d'\n", $iDepthMin, $iDepthMax);
	if(*$wpInDbn)
	{
		mp1 = W2M($wpInDbn);
			P("--  -in        '%s'\n", mp1);
		ifree(mp1);
	}
	if(*$wpOutDbn)
	{
		mp1 = W2M($wpOutDbn);
			P("--  -out       '%s'\n", mp1);
		ifree(mp1);
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
	P(ICLR_RESET);
}

VOID
print_version()
{
	P(ICLR_STR2);
	LN(80);
	P(" %s\n", IWM_COPYRIGHT);
	P("    Ver.%s+%s+SQLite%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION, SQLITE_VERSION);
	LN(80);
	P(ICLR_RESET);
}

VOID
print_help()
{
	MS *_cmd = W2M($CMD);
	MS *_select0 = W2M(OP_SELECT_0);

	print_version();
	P("%s ファイル検索 %s\n", ICLR_TITLE1, ICLR_RESET);
	P("%s    %s %s[Dir] %s[Option]\n", ICLR_STR1, _cmd, ICLR_OPT1, ICLR_OPT2);
	P("%s        or\n", ICLR_LBL1);
	P("%s    %s %s[Option] %s[Dir]\n", ICLR_STR1, _cmd, ICLR_OPT2, ICLR_OPT1);
	P("\n");
	P("%s (例１) %s検索\n", ICLR_LBL1, ICLR_STR1);
	P("%s    %s %sDir %s-r -s=\"LN,path,size\" -w=\"name like '*.exe'\"\n", ICLR_STR1, _cmd, ICLR_OPT1, ICLR_OPT2);
	P("\n");
	P("%s (例２) %s検索結果をファイルへ保存\n", ICLR_LBL1, ICLR_STR1);
	P("%s    %s %sDir1 Dir2 %s-r -o=File\n", ICLR_STR1, _cmd, ICLR_OPT1, ICLR_OPT2);
	P("\n");
	P("%s (例３) %s検索対象をファイルから読込\n", ICLR_LBL1, ICLR_STR1);
	P("%s    %s %s-i=File\n", ICLR_STR1, _cmd, ICLR_OPT2);
	P("\n");
	P("%s [Dir]\n", ICLR_OPT1);
	P("%s    検索対象Dir\n", ICLR_STR1);
	P("%s    (例) %s\"c:\\\" \".\" (複数指定可)\n", ICLR_LBL1, ICLR_STR1);
	P("\n");
	P("%s [Option]\n", ICLR_OPT2);
	P("%s  [基本操作]\n", ICLR_LBL2);
	P("%s    -recursive | -r\n", ICLR_OPT21);
	P("%s        全階層を検索\n", ICLR_STR1);
	P("\n");
	P("%s    -depth=Num1,Num2 | -d=Num1,Num2\n", ICLR_OPT21);
	P("%s        検索する階層を指定\n", ICLR_STR1);
	P("%s        (例１) %s-d=\"1\"\n", ICLR_LBL1, ICLR_STR1);
	P("%s               1階層のみ検索\n", ICLR_STR1);
	P("\n");
	P("%s        (例２) %s-d=\"3\",\"5\"\n", ICLR_LBL1, ICLR_STR1);
	P("%s               3～5階層を検索\n", ICLR_STR1);
	P("\n");
	P("%s        ※１ CurrentDir は \"0\"\n", ICLR_OPT22);
	P("%s        ※２ -depth と -where における depth の挙動の違い\n", ICLR_OPT22);
	P("%s            ◇速い %s-depth は指定された階層のみ検索を行う\n", ICLR_OPT1, ICLR_STR1);
	P("%s            ◇遅い %s-where内でのdepthによる検索は全階層のDir／Fileに対して行う\n", ICLR_OPT2, ICLR_STR1);
	P("\n");
	P("%s    -out=File | -o=File\n", ICLR_OPT21);
	P("%s        出力ファイル\n", ICLR_STR1);
	P("\n");
	P("%s    -in=File | -i=File\n", ICLR_OPT21);
	P("%s        入力ファイル\n", ICLR_STR1);
	P("%s        検索対象Dirと併用できない\n", ICLR_OPT22);
	P("\n");
	P("%s  [SQL関連]\n", ICLR_LBL2);
	P("%s    -select=Column1,Column2,... | -s=Column1,Column2,...\n", ICLR_OPT21);
	P("%s        LN        (Num) 連番／1回のみ指定可\n", ICLR_STR1);
	P("%s        path      (Str) dir\\name\n", ICLR_STR1);
	P("%s        dir       (Str) ディレクトリ名\n", ICLR_STR1);
	P("%s        name      (Str) ファイル名\n", ICLR_STR1);
	P("%s        depth     (Num) ディレクトリ階層 = 0..\n", ICLR_STR1);
	P("%s        type      (Str) ディレクトリ = d／ファイル = f\n", ICLR_STR1);
	P("%s        attr_num  (Num) 属性\n", ICLR_STR1);
	P("%s        attr      (Str) 属性 \"[d|f][r|-][h|-][s|-][a|-]\"\n", ICLR_STR1);
	P("%s                        [dir|file][read-only][hidden][system][archive]\n", ICLR_STR1);
	P("%s        size      (Num) ファイルサイズ = byte\n", ICLR_STR1);
	P("%s        ctime_cjd (Num) 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5\n", ICLR_STR1);
	P("%s        ctime     (Str) 作成日時     \"yyyy-mm-dd hh:nn:ss\"\n", ICLR_STR1);
	P("%s        mtime_cjd (Num) 更新日時     ctime_cjd参照\n", ICLR_STR1);
	P("%s        mtime     (Str) 更新日時     ctime参照\n", ICLR_STR1);
	P("%s        atime_cjd (Num) アクセス日時 ctime_cjd参照\n", ICLR_STR1);
	P("%s        atime     (Str) アクセス日時 ctime参照\n", ICLR_STR1);
	P("%s        *         全項目表示\n", ICLR_STR1);
	P("\n");
	P("%s        ※１ Column指定なしの場合\n", ICLR_OPT22);
	P("%s             %s を表示\n", ICLR_STR1, _select0);
	P("%s        ※２ SQLite演算子／関数を利用可能\n", ICLR_OPT22);
	P("%s             abs(X)  changes()  char(X1,X2,...,XN)  coalesce(X,Y,...)  format(FORMAT,...)\n", ICLR_STR1);
	P("%s             glob(X,Y)  hex(X)  ifnull(X,Y)  iif(X,Y,Z)  instr(X,Y)  last_insert_rowid()  length(X)\n", ICLR_STR1);
	P("%s             like(X,Y)  like(X,Y,Z)  likelihood(X,Y)  likely(X)  load_extension(X)  load_extension(X,Y)\n", ICLR_STR1);
	P("%s             lower(X)  ltrim(X)  ltrim(X,Y)  max(X,Y,...)  min(X,Y,...)  nullif(X,Y)  P(FORMAT,...)\n", ICLR_STR1);
	P("%s             quote(X)  random()  randomblob(N)  replace(X,Y,Z)  round(X)  round(X,Y)\n", ICLR_STR1);
	P("%s             rtrim(X)  rtrim(X,Y)  sign(X)  soundex(X)\n", ICLR_STR1);
	P("%s             sqlite_compileoption_get(N)  sqlite_compileoption_used(X)\n", ICLR_STR1);
	P("%s             sqlite_offset(X)  sqlite_source_id()  sqlite_version()\n", ICLR_STR1);
	P("%s             substr(X,Y)  substr(X,Y,Z)  substring(X,Y)  substring(X,Y,Z)\n", ICLR_STR1);
	P("%s             total_changes()  trim(X)  trim(X,Y)  typeof(X)  unicode(X)  unlikely(X)  upper(X)  zeroblob(N)\n", ICLR_STR1);
	P("%s             (参考) http://www.sqlite.org/lang_corefunc.html\n", ICLR_LBL2);
	P("\n");
	P("%s    -where=Str | -w=Str\n", ICLR_OPT21);
	P("%s        (例１) %s\"size <= 100 or size > 1000000\"\n", ICLR_LBL1, ICLR_STR1);
	P("\n");
	P("%s        (例２) %s\"type = 'f' and name like 'abc??.*'\"\n", ICLR_LBL1, ICLR_STR1);
	P("%s               '?' '_' は任意の1文字\n", ICLR_STR1);
	P("%s               '*' '%%' は任意の0文字以上\n", ICLR_STR1);
	P("\n");
	P("%s        (例３) %s基準日 \"2010-12-10 12:30:00\" のとき\n", ICLR_LBL1, ICLR_STR1);
	P("%s               \"ctime >= [-10d]\"  : ctime >= '2010-11-30 12:30:00'\n", ICLR_STR1);
	P("%s               \"ctime >= [-10D]\"  : ctime >= '2010-11-30 00:00:00'\n", ICLR_STR1);
	P("%s               \"ctime >= [-10d%%]\" : ctime >= '2010-11-30 %%'\n", ICLR_STR1);
	P("%s               \"ctime like [%%]\"   : ctime like '2010-12-10 %%'\n", ICLR_STR1);
	P("%s               (年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s\n", ICLR_STR1);
	P("\n");
	P("%s    -group=Str | -g=Str\n", ICLR_OPT21);
	P("%s        (例) %s-g=\"Str1, Str2\"\n", ICLR_LBL1, ICLR_STR1);
	P("%s             Str1とStr2をグループ毎にまとめる\n", ICLR_STR1);
	P("\n");
	P("%s    -sort=\"Str [ASC|DESC]\" | -st=\"Str [ASC|DESC]\"\n", ICLR_OPT21);
	P("%s        (例) %s-st=\"Str1 ASC, Str2 DESC\"\n", ICLR_LBL1, ICLR_STR1);
	P("%s             Str1を順ソート, Str2を逆順ソート\n", ICLR_STR1);
	P("\n");
	P("%s  [出力フォーマット]\n", ICLR_LBL2);
	P("%s    -noheader | -nh\n", ICLR_OPT21);
	P("%s        ヘッダ情報を表示しない\n", ICLR_STR1);
	P("\n");
	P("%s    -nofooter | -nf\n", ICLR_OPT21);
	P("%s        フッタ情報を表示しない\n", ICLR_STR1);
	P("\n");
	P("%s    -quote=Str | -qt=Str\n", ICLR_OPT21);
	P("%s        囲み文字\n", ICLR_STR1);
	P("%s        (例) %s-qt=\"'\"\n", ICLR_LBL1, ICLR_STR1);
	P("\n");
	P("%s    -separate=Str | -sp=Str\n", ICLR_OPT21);
	P("%s        区切り文字\n", ICLR_STR1);
	P("%s        (例) %s-sp=\"\\t\"\n", ICLR_LBL1, ICLR_STR1);
	P("\n");
	P("%s  [出力結果を操作]\n", ICLR_LBL2);
	P("%s    --mkdir=Dir | --md=Dir\n", ICLR_OPT21);
	P("%s        検索結果のDirをコピー作成する (-recursive のとき 階層維持)\n", ICLR_STR1);
	P("\n");
	P("%s    --copy=Dir | --cp=Dir\n", ICLR_OPT21);
	P("%s        --mkdir + 検索結果をDirにコピーする (-recursive のとき 階層維持)\n", ICLR_STR1);
	P("\n");
	P("%s    --move=Dir | --mv=Dir\n", ICLR_OPT21);
	P("%s        --mkdir + 検索結果をDirに移動する (-recursive のとき 階層維持)\n", ICLR_STR1);
	P("\n");
	P("%s    --move2=Dir | --mv2=Dir\n", ICLR_OPT21);
	P("%s        --mkdir + --move + 移動元の空Dirを削除する (-recursive のとき 階層維持)\n", ICLR_STR1);
	P("\n");
	P("%s    --extract=Dir | --ext=Dir\n", ICLR_OPT21);
	P("%s        --mkdir + 検索結果ファイルのみ抽出しDirにコピーする\n", ICLR_STR1);
	P("%s        階層を維持しない／同名ファイルは上書き\n", ICLR_STR1);
	P("\n");
	P("%s    --extract2=Dir | --ext2=Dir\n", ICLR_OPT21);
	P("%s        --mkdir + 検索結果ファイルのみ抽出しDirに移動する\n", ICLR_STR1);
	P("%s        階層を維持しない／同名ファイルは上書き\n", ICLR_STR1);
	P("\n");
	P("%s    --remove | --rm\n", ICLR_OPT21);
	P("%s        検索結果のFileのみ削除する（Dirは削除しない）\n", ICLR_STR1);
	P("\n");
	P("%s    --remove2 | --rm2\n", ICLR_OPT21);
	P("%s        --remove + 空Dirを削除する\n", ICLR_STR1);
	P("\n");
	P("%s    --delete | --del\n", ICLR_OPT21);
	P("%s        検索結果のFileのみゴミ箱へ移動する（Dirは移動しない）\n", ICLR_STR1);
	P("\n");
	P("%s    --delete2 | --del2\n", ICLR_OPT21);
	P("%s        --delete + Dirをゴミ箱へ移動する\n", ICLR_STR1);
	P("\n");
	P("%s    --replace=File | --rep=File\n", ICLR_OPT21);
	P("%s        検索結果(複数) をFileの内容で置換(上書き)する／ファイル名は変更しない\n", ICLR_STR1);
	P("%s        (例) %s-w=\"name like 'before.txt'\" --rep=\".\\after.txt\"\n", ICLR_LBL1, ICLR_STR1);
	P("\n");
	P(ICLR_STR2);
	LN(80);
	P(ICLR_RESET);

	ifree(_select0);
	ifree(_cmd);
}
