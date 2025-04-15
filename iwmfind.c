//------------------------------------------------------------------------------
#define   IWM_COPYRIGHT       "(C)2009-2025 iwm-iwama"
#define   IWM_FILENAME        "iwmfind"
#define   IWM_UPDATE          "20250413"
//------------------------------------------------------------------------------
#include "lib_iwmutil2.h"
#include "sqlite3.h"

INT       main();
WS        *sql_escape(WS *pW);
VOID      ifind10($struct_iFinfo *FI, WIN32_FIND_DATAW F, WS *dir, INT depth);
VOID      ifind21(INT dirId, WS *dir);
VOID      ifind22($struct_iFinfo *FI, INT dirId, WS *fname);
VOID      sql_exec(sqlite3 *db, CONST MS *sql, sqlite3_callback cb);
INT       sql_columnName(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_std(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_countOnly(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
INT       sql_result_exec(VOID *option, INT iColumnCount, MS **sColumnValues, MS **sColumnNames);
BOOL      sql_saveOrLoadMemdb(sqlite3 *mem_db, WS *ofn, BOOL save_load);
VOID      print_footer();
VOID      print_version();
VOID      print_help();

$struct_iVBM        *IVBM = 0;   // Variable Buf
UINT      BufSizeMax = 10000000; // Buf最大サイズ
UINT      DirId = 0;             // Dir数
UINT      AllCnt = 0;            // 検索数
UINT      CallCnt_ifind10 = 0;   // ifind10()が呼ばれた回数
UINT      StepPos = 0;           // CurrentDir位置
UINT      RowCnt = 0;            // 処理行数 <= NTFSの最大ファイル数(2^32 - 1 = 4_294_967_295)
MS        *SqlCmd = 0;
sqlite3   *InDbs = 0;
sqlite3_stmt        *Stmt1 = 0, *Stmt2 = 0;

#define   MEMDB               L":memory:"
#define   OLDDB               (L"iwmfind.db."IWM_FILENAME)
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
#define   I_MKDIR    1
#define   I_CP       2
#define   I_MV       3
#define   I_MV2      4
#define   I_EXT      5
#define   I_EXT2     6
#define   I_TB      11
#define   I_REP     20
#define   I_RM      31
#define   I_RM2     32
/*
	以下の "_" で始まる変数はオプションに応じて動的に作成される。
	これらの解放は imain_end() で一括処理している。
*/
//
// 検索Dir
//
WS **AryInDir = {};
INT AryInDirSize = 0;
//
// 入力DB
// -in=Str | -i=Str
//
WS *_InDbn = MEMDB;
WS *_InTmp = L"";
//
// 出力DB
// -out=Str | -o=Str
//
WS *_OutDbn = L"";
//
// select
// -select=Str | -s=Str
//
WS *_Select = OP_SELECT_0;
INT _SelectPosLN = 0; // "LN"の配列位置
//
// where
// -where=Str | -w=Str
//
WS *_Where1 = L"";
WS *_Where2 = L"";
//
// group by
// -group=Str | -g=Str
//
WS *_Group = L"";
//
// order by
// -sort=Str | -st=Str
//
WS *_Sort = L"";
//
// ヘッダ情報を非表示／初期値は表示
// -noheader | -nh
//
BOOL _Header = TRUE;
//
// フッタ情報を非表示／初期値は表示
// -nofooter | -nf
//
BOOL _Footer = TRUE;
//
// 出力をStrで囲む
// -quote=Str | -qt=Str
//
MS *_Quote = "";
UINT _QuoteLen = 0;
//
// 出力をStrで区切る
// -separate=Str | -sp=Str
//
MS *_Separate = " | ";
UINT _SeparateLen = 3;
//
// 検索Dir位置
// -depth=NUM1,NUM2 | -d=NUM1,NUM2
//
INT _DepthMin = 0; // 階層～開始位置(相対)
INT _DepthMax = 0; // 階層～終了位置(相対)
//
// mkdir Str
// --mkdir=Str | --md=Str
//
WS *_Md = L"";
WS *_MdOp = L"";
//
// mkdir Str & copy
// --copy=Str | --cp=Str
//
WS *_Cp = L"";
//
// mkdir Str & move Str
// --move=Str | --mv=Str
//
WS *_Mv = L"";
//
// mkdir Str & move Str & rmdir
// --move2=Str | --mv2=Str
//
WS *_Mv2 = L"";
//
// copy Str
// --extract=Str | --ext=Str
//
WS *_Ext = L"";
//
// move Str
// --extract2=Str | --ext2=Str
//
WS *_Ext2 = L"";
//
// TrashBox
// --trashbox  | --tb
//
INT _Trashbox = 0;
$struct_iVBM        *IVBM_trashbox = 0;
//
// replace results with Str
// --replace=Str | --rep=Str
//
WS *_Rep = L"";
WS *_RepOp = L"";
//
// Remove
// --remove  | --rm
// --remove2 | --rm2
//
INT _Rm = 0;
//
// 優先順 (安全)I_MD > (危険)I_RM2
//  I_MD   = --mkdir
//  I_CP   = --cp
//  I_MV   = --mv
//  I_MV2  = --mv2
//  I_EXT  = --ext
//  I_EXT2 = --ext2
//  I_TB   = --tb
//  I_REP  = --rep
//  I_RM   = --rm
//  I_RM2  = --rm2
//
INT I_exec = 0;
//
// Debug
// -debug
//
BOOL _Debug = FALSE;

INT
main()
{
	// lib_iwmutil2 初期化
	imain_begin();

	///iCLI_VarList();

	// -h | --help
	if(! $ARGC || iCLI_getOptMatch(0, L"-h", L"--help"))
	{
		print_version();
		print_help();
		imain_end();
	}

	// -v | --version
	if(iCLI_getOptMatch(0, L"-v", L"--version"))
	{
		print_version();
		imain_end();
	}

	WS *wp1 = 0, *wp2 = 0;
	MS *mp1 = 0, *mp2 = 0, *mp3 = 0;

	// AryInDir取得で _DepthMax を使うため
	//   -recursive
	//   -depth
	// をチェック
	for(UINT _u1 = 0; _u1 < $ARGC; _u1++)
	{
		// -r | -recursive
		if(iCLI_getOptMatch(_u1, L"-r", L"-recursive"))
		{
			_DepthMin = 0;
			_DepthMax = IMAX_PATHW;
		}

		// -d=NUM1,NUM2 | -depth=NUM1,NUM2
		if((wp1 = iCLI_getOptValue(_u1, L"-d=", L"-depth=")))
		{
			WS **wa1 = iwaa_split(wp1, TRUE, 1, L",");
				UINT _u2 = iwan_size(wa1);
				if(_u2 > 1)
				{
					_DepthMin = _wtoi(wa1[0]);
					_DepthMax = _wtoi(wa1[1]);
					if(_DepthMax > IMAX_PATHW)
					{
						_DepthMax = IMAX_PATHW;
					}
					if(_DepthMin > _DepthMax)
					{
						_u2 = _DepthMin;
						_DepthMin = _DepthMax;
						_DepthMax = _u2;
					}
				}
				else if(_u2 == 1)
				{
					_DepthMin = _wtoi(wa1[0]);
					_DepthMax = _DepthMin;
				}
				else
				{
					_DepthMin = 0;
					_DepthMax = 0;
				}
			ifree(wa1);
		}
	}

	// Dir存在チェック
	for(UINT _u1 = 0; _u1 < $ARGC; _u1++)
	{
		if(*$ARGV[_u1] != '-' && ! iF_chkExistPath($ARGV[_u1]))
		{
			mp1 = W2M($ARGV[_u1]);
				P(
					IESC_FALSE1
					"[Err] Dir '%s' は存在しない!"
					IESC_RESET
					"\n"
					, mp1
				);
			ifree(mp1);
		}
	}

	// AryInDir を作成
	AryInDir = (_DepthMax == IMAX_PATHW ? iwaa_higherDir($ARGV) : iwaa_getDirFile($ARGV, 1));
	AryInDirSize = iwan_size(AryInDir);

	// Main Loop
	for(UINT _u1 = 0; _u1 < $ARGC; _u1++)
	{
		// -i | -in
		if((wp1 = iCLI_getOptValue(_u1, L"-i=", L"-in=")))
		{
			if(! iF_chkExistPath(wp1))
			{
				mp1 = W2M(wp1);
					P(
						IESC_FALSE1
						"[Err] -in '%s' は存在しない!"
						IESC_RESET
						"\n"
						, mp1
					);
				ifree(mp1);
				imain_end();
			}
			else if(AryInDirSize)
			{
				P2(
					IESC_FALSE1
					"[Err] Dir と -in は併用できない!"
					IESC_RESET
				);
				imain_end();
			}
			else
			{
				_InDbn = wp1;
				_InTmp = _InDbn;
				// -in のときは -recursive 自動付与
				_DepthMin = 0;
				_DepthMax = IMAX_PATHW;
			}
		}

		// -o | -out
		if((wp1 = iCLI_getOptValue(_u1, L"-o=", L"-out=")))
		{
			_OutDbn = wp1;
		}

		// --md | --mkdir
		if((wp1 = iCLI_getOptValue(_u1, L"--md=", L"--mkdir=")))
		{
			_Md = wp1;
		}

		// --cp | --copy
		if((wp1 = iCLI_getOptValue(_u1, L"--cp=", L"--copy=")))
		{
			_Cp = wp1;
		}

		// --mv | --move
		if((wp1 = iCLI_getOptValue(_u1, L"--mv=", L"--move=")))
		{
			_Mv = wp1;
		}

		// --mv2 | --move2
		if((wp1 = iCLI_getOptValue(_u1, L"--mv2=", L"--move2=")))
		{
			_Mv2 = wp1;
		}

		// --ext | --extract
		if((wp1 = iCLI_getOptValue(_u1, L"--ext=", L"--extract=")))
		{
			_Ext = wp1;
		}

		// --ext2 | --extract2
		if((wp1 = iCLI_getOptValue(_u1, L"--ext2=", L"--extract2=")))
		{
			_Ext2 = wp1;
		}

		// --tb | --trashbox
		if(iCLI_getOptMatch(_u1, L"--tb", L"--trashbox"))
		{
			_Trashbox = I_TB;
		}

		// --rep | --replace
		if((wp1 = iCLI_getOptValue(_u1, L"--rep=", L"--replace=")))
		{
			_Rep = wp1;
		}

		// --rm | --remove
		if(iCLI_getOptMatch(_u1, L"--rm", L"--remove"))
		{
			_Rm = I_RM;
		}

		// --rm2 | --remove2
		if(iCLI_getOptMatch(_u1, L"--rm2", L"--remove2"))
		{
			_Rm = I_RM2;
		}

		// -s | -select
		//   (none)     : OP_SELECT_0
		//   -select="" : Err
		//   -select="Column1,Column2,..."
		if((wp1 = iCLI_getOptValue(_u1, L"-s=", L"-select=")))
		{
			// "AS" 対応のため " " (空白)は不可
			// (例) -s="dir||name AS PATH"
			WS **wa1 = iwaa_split(wp1, TRUE, 1, L",");
				if(wa1[0])
				{
					WS **wa2 = iwaa_uniq(wa1, TRUE); // 重複排除
						_Select = iwas_join(wa2, L",");
					ifree(wa2);
				}
			ifree(wa1);
		}

		// -st | -sort
		if((wp1 = iCLI_getOptValue(_u1, L"-st=", L"-sort=")))
		{
			_Sort = wp1;
		}

		// -w | -where
		if((wp1 = iCLI_getOptValue(_u1, L"-w=", L"-where=")))
		{
			_Where1 = sql_escape(wp1);
			_Where2 = iws_cats(2, L"WHERE ", _Where1);
		}

		// -g | -group
		if((wp1 = iCLI_getOptValue(_u1, L"-g=", L"-group=")))
		{
			_Group = iws_cats(2, L"GROUP BY ", wp1);
		}

		// -nh | -noheader
		if(iCLI_getOptMatch(_u1, L"-nh", L"-noheader"))
		{
			_Header = FALSE;
		}

		// -nf | -nofooter
		if(iCLI_getOptMatch(_u1, L"-nf", L"-nofooter"))
		{
			_Footer = FALSE;
		}

		// -qt | -quote
		if((wp1 = iCLI_getOptValue(_u1, L"-qt=", L"-quote=")))
		{
			wp2 = iws_cnv_escape(wp1);
				_Quote = W2M(wp2);
				_QuoteLen = strlen(_Quote);
			ifree(wp2);
		}

		// -sp | -separate
		if((wp1 = iCLI_getOptValue(_u1, L"-sp=", L"-separate=")))
		{
			wp2 = iws_cnv_escape(wp1);
				_Separate = W2M(wp2);
				_SeparateLen = strlen(_Separate);
			ifree(wp2);
		}

		// -debug
		if(iCLI_getOptMatch(_u1, L"-debug", NULL))
		{
			_Debug = TRUE;
		}
	}

	// --[exec] 関係を一括変換
	if(*_Md || *_Cp || *_Mv || *_Mv2 || *_Ext || *_Ext2 || _Trashbox || *_Rep || _Rm)
	{
		_Footer = FALSE; // フッタ情報を非表示

		if(*_Md)
		{
			I_exec = I_MKDIR;
			_MdOp = iF_getAPath(_Md);
		}
		else if(*_Cp)
		{
			I_exec = I_CP;
			_MdOp = iF_getAPath(_Cp);
		}
		else if(*_Mv)
		{
			I_exec = I_MV;
			_MdOp = iF_getAPath(_Mv);
		}
		else if(*_Mv2)
		{
			I_exec = I_MV2;
			_MdOp = iF_getAPath(_Mv2);
		}
		else if(*_Ext)
		{
			I_exec = I_EXT;
			_MdOp = iF_getAPath(_Ext);
		}
		else if(*_Ext2)
		{
			I_exec = I_EXT2;
			_MdOp = iF_getAPath(_Ext2);
		}
		else if(_Trashbox)
		{
			I_exec = _Trashbox;
		}
		else if(*_Rep)
		{
			if(! iF_chkExistPath(_Rep))
			{
				mp1 = W2M(_Rep);
					P(
						IESC_FALSE1
						"[Err] --replace '%s' は存在しない!"
						IESC_RESET
						"\n"
						, mp1
					);
				ifree(mp1);
				imain_end();
			}
			else
			{
				I_exec = I_REP;
				_RepOp = iws_clone(_Rep);
			}
		}
		else if(_Rm)
		{
			I_exec = _Rm;
		}
		else
		{
		}

		if(I_exec <= I_EXT2)
		{
			_Select = (I_exec <= I_MV2 ? OP_SELECT_MKDIR : OP_SELECT_EXTRACT);
		}
		else if(I_exec == I_REP)
		{
			_Select = OP_SELECT_RP;
		}
		else
		{
			_Select = OP_SELECT_RM;
		}
	}

	// -sort 関係を一括変換
	if(I_exec >= I_MV)
	{
		// Dir削除時必要なものは "order by lower(path) desc"
		_Sort = iws_clone(L"lower(path) desc");
	}
	else if(! *_Sort)
	{
		_Sort = iws_clone(L"lower(dir),lower(name)");
	}
	else
	{
		// path, dir, name, ext
		// ソートは、大文字・小文字を区別しない
		wp1 = iws_replace(_Sort, L"path", L"lower(path)", TRUE);
		ifree(_Sort);
		wp2 = iws_replace(wp1, L"dir", L"lower(dir)", TRUE);
		ifree(wp1);
		_Sort = iws_replace(wp2, L"name", L"lower(name)", TRUE);
		ifree(wp2);
	}

	// -in DBを指定
	if(sqlite3_open16(_InDbn, &InDbs))
	{
		mp1 = W2M(_InDbn);
			P(
				IESC_FALSE1
				"[Err] -in '%s' を開けない!"
				IESC_RESET
				"\n"
				, mp1
			);
		ifree(mp1);
		sqlite3_close(InDbs); // ErrでもDB解放
		imain_end();
	}
	else
	{
		// Buf作成
		IVBM = iVBM_alloc();

		// ゴミ箱へ移動
		if(_Trashbox)
		{
			IVBM_trashbox = iVBM_alloc();
		}

		// DB構築
		if(! *_InTmp)
		{
			$struct_iFinfo *FI = iFinfo_alloc();
				WIN32_FIND_DATAW F;
				// PRAGMA設定
				sql_exec(InDbs, "PRAGMA encoding = 'UTF-8';", 0);
				// TABLE作成
				sql_exec(InDbs, CREATE_T_DIR, 0);
				sql_exec(InDbs, CREATE_T_FILE, 0);
				// VIEW作成
				sql_exec(InDbs, CREATE_VIEW, 0);
				// 前処理
				sqlite3_prepare_v2(InDbs, INSERT_T_DIR, strlen(INSERT_T_DIR), &Stmt1, 0);
				sqlite3_prepare_v2(InDbs, INSERT_T_FILE, strlen(INSERT_T_FILE), &Stmt2, 0);
				// トランザクション開始
				sql_exec(InDbs, "BEGIN", 0);
				// 検索データ DB書込
				UINT _u2 = 0;
				while((wp1 = AryInDir[_u2++]))
				{
					StepPos = wcslen(wp1);
					// 本処理
					ifind10(FI, F, wp1, 0);
				}
				// トランザクション終了
				sql_exec(InDbs, "COMMIT", 0);
				// 後処理
				sqlite3_finalize(Stmt2);
				sqlite3_finalize(Stmt1);
			iFinfo_free(FI);
		}
		// 結果
		if(*_OutDbn)
		{
			// 存在する場合、削除
			DeleteFileW(_OutDbn);
			// _InTmp, _OutDbn 両指定、別ファイル名のとき
			if(*_InTmp)
			{
				CopyFileW(_InDbn, OLDDB, FALSE);
			}
			// WHERE Str のとき不要データ削除
			if(*_Where1)
			{
				mp1 = W2M(_Where1);
				mp2 = ims_cats(2, "WHERE ", mp1);
				ifree(mp1);
				mp3 = ims_sprintf(UPDATE_EXEC99_1, mp2);
				ifree(mp2);
					sql_exec(InDbs, "BEGIN", 0);         // トランザクション開始
					sql_exec(InDbs, mp3, 0);             // フラグを立てる
					sql_exec(InDbs, DELETE_EXEC99_1, 0); // 不要データ削除
					sql_exec(InDbs, "COMMIT", 0);        // トランザクション終了
					sql_exec(InDbs, "VACUUM", 0);        // VACUUM
				ifree(mp3);
			}
			// _InTmp, _OutDbn 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
			sql_saveOrLoadMemdb(InDbs, (*_InTmp ? _InDbn : _OutDbn), TRUE);
			// 結果数を取得
 			sql_exec(InDbs, "SELECT id FROM V_INDEX;", sql_result_countOnly);
		}
		else
		{
			// SQL作成 UTF-8／Sqlite3対応
			//   手間だが M2W() => W2M() でエンコードしないとUTF-8が使用できない
			wp1 = M2W(SELECT_VIEW);
			wp2 = iws_sprintf(wp1, _Select, _Where2, _Group, _Sort);
			ifree(wp1);
			SqlCmd = W2M(wp2);
			ifree(wp2);
			// SQL実行
			if(I_exec)
			{
				sql_exec(InDbs, SqlCmd, sql_result_exec);
			}
			else
			{
				// カラム名出力／[LN]位置取得
				mp1 = W2M(_Select);
				mp2 = ims_cats(3, "SELECT ", mp1, " FROM V_INDEX LIMIT 1;");
				ifree(mp1);
					sql_exec(InDbs, mp2, sql_columnName);
				ifree(mp2);
				// 結果出力
				sql_exec(InDbs, SqlCmd, sql_result_std);
			}
		}
		// DB解放
		sqlite3_close(InDbs);
		// _InDbn, _OutDbn ファイル名をswap
		if(*_InTmp)
		{
			MoveFileW(_InDbn, _OutDbn);
			MoveFileW(OLDDB, _InDbn);
		}
		// 作業用ファイル削除
		DeleteFileW(OLDDB);

		// Buf解放
		iVBM_freeAll(IVBM);

		// 事後バッチ処理
		// ゴミ箱へ移動
		if(_Trashbox)
		{
			wp1 = M2W(iVBM_getStr(IVBM_trashbox));
			WS **awp1 = iF_trash(wp1);
			ifree(wp1);
				P1("\033[92m");
				iwav_print2(awp1, L"- ", L"\n");
				P1("\033[0m");
			ifree(awp1);
			iVBM_freeAll(IVBM_trashbox);
		}
	}

	// フッタ部
	if(_Footer)
	{
		print_footer();
	}

	// Debug
	if(_Debug)
	{
		idebug_map();
		///ifree_all();
		///idebug_map();
	}

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
	$struct_iDV *IDV = iDV_alloc();
		iDV_set2(IDV, idate_nowToCjd(TRUE));
		// [] を日付に変換
		wp1 = idate_replace_format_ymdhns(
			wp2,
			L"[", L"]",
			L"'",
			IDV->y,
			IDV->m,
			IDV->d,
			IDV->h,
			IDV->n,
			IDV->s
		);
	iDV_free(IDV);
	ifree(wp2);
	return wp1;
}

VOID
ifind10(
	$struct_iFinfo *FI,
	WIN32_FIND_DATAW F,
	WS *dir,
	INT depth
)
{
	if(depth > _DepthMax)
	{
		return;
	}
	// FI->sPath 末尾に "*" を付与
	UINT dirLen = iwn_cpy(FI->sPath, dir);
		*(FI->sPath + dirLen)     = '*';
		*(FI->sPath + dirLen + 1) = 0;
	HANDLE hfind = FindFirstFileW(FI->sPath, &F);
		// アクセス不可なフォルダ等はスルー
		if(hfind == INVALID_HANDLE_VALUE)
		{
			return;
		}
		// 読み飛ばす Depth
		BOOL bMinDepthFlg = (depth >= _DepthMin ? TRUE : FALSE);
		// dirIdを逐次生成
		INT dirId = (++DirId);
		// Dir
		if(bMinDepthFlg)
		{
			ifind21(dirId, dir);
			iFinfo_init(FI, &F, dir, NULL);
			ifind22(FI, dirId, L"");
		}
		// File
		do
		{
			if(iFinfo_init(FI, &F, dir, F.cFileName))
			{
				if(FI->bType)
				{
					UINT u1 = wcslen(FI->sPath) + 1;
					WS ws1[u1];
						wmemcpy(ws1, FI->sPath, u1);
						// 下位Dirへ
						ifind10(FI, F, ws1, (depth + 1));
				}
				else if(bMinDepthFlg)
				{
					ifind22(FI, dirId, F.cFileName);
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
	// 経過表示
	++CallCnt_ifind10;
	if(CallCnt_ifind10 >= 10000)
	{
		fprintf(stderr, "%s> %u%s\033[0K\r", IESC_OPT21, AllCnt, IESC_RESET);
		CallCnt_ifind10 = 0;
	}
}

VOID
ifind21(
	INT dirId,
	WS *dir
)
{
	sqlite3_reset(Stmt1);
		sqlite3_bind_int64(Stmt1,  1, dirId);
		sqlite3_bind_text16(Stmt1, 2, dir, -1, SQLITE_STATIC);
		sqlite3_bind_int(Stmt1,    3, StepPos);
	sqlite3_step(Stmt1);
}

VOID
ifind22(
	$struct_iFinfo *FI,
	INT dirId,
	WS *fname
)
{
	++AllCnt;
	sqlite3_reset(Stmt2);
		sqlite3_bind_int(Stmt2,    1, dirId);
		sqlite3_bind_int(Stmt2,    2, AllCnt);
		sqlite3_bind_text16(Stmt2, 3, fname, -1, SQLITE_STATIC);
		sqlite3_bind_int(Stmt2,    4, FI->uAttr);
		sqlite3_bind_double(Stmt2, 5, FI->ctime_cjd);
		sqlite3_bind_double(Stmt2, 6, FI->mtime_cjd);
		sqlite3_bind_double(Stmt2, 7, FI->atime_cjd);
		sqlite3_bind_int64(Stmt2,  8, FI->uFsize);
		sqlite3_bind_int(Stmt2,    9, 0);
	sqlite3_step(Stmt2);
}

VOID
sql_exec(
	sqlite3 *db,
	CONST MS *sql,
	sqlite3_callback cb
)
{
	MS *p_err = 0; // SQLiteが使用
	RowCnt = 0;
	if(sqlite3_exec(db, sql, cb, 0, &p_err))
	{
		P(
			IESC_FALSE1
			"[Err] 構文エラー"
			IESC_RESET
			"\n    %s\n    %s"
			, p_err
			, sql
		);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}
	// sql_result_std() 対応
	QP(IVBM->str, IVBM->length);
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
	_SelectPosLN = -1;
	for(INT _i1 = 0; _i1 < iColumnCount; _i1++)
	{
		if(! strcmp(CharUpperA(sColumnNames[_i1]), "LN"))
		{
			_SelectPosLN = _i1;
			break;
		}
	}
	// カラム名出力
	if(_Header)
	{
		P1(IESC_OPT21);
		for(INT _i1 = 0; _i1 < iColumnCount; _i1++)
		{
			P1("[");
			P1(sColumnNames[_i1]);
			P1("]");
			if((_i1 + 1) < iColumnCount)
			{
				P1(_Separate);
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
	++RowCnt;
	INT i1 = 0;
	while(i1 < iColumnCount)
	{
		iVBM_push(IVBM, _Quote, _QuoteLen);
		// [LN]
		if(i1 == _SelectPosLN)
		{
			// 本来 iVBM_push_sprintf() だが効率優先
			MS mpTmp[16];
			INT iLen = sprintf(mpTmp, "%u", RowCnt);
			iVBM_push(IVBM, mpTmp, iLen);
		}
		else
		{
			iVBM_push2(IVBM, sColumnValues[i1]);
		}
		iVBM_push(IVBM, _Quote, _QuoteLen);
		++i1;
		if(i1 == iColumnCount)
		{
			iVBM_push(IVBM, "\n", 1);
			if(IVBM->length >= BufSizeMax)
			{
				QP(IVBM->str, IVBM->length);
				// 本来 iVBM_clear(IVBM) だが効率優先
				IVBM->freeSize += IVBM->length;
				IVBM->length = 0;
			}
		}
		else
		{
			iVBM_push(IVBM, _Separate, _SeparateLen);
		}
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
	++RowCnt;
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
	switch(I_exec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// _MdOp\以下には、AryInDir\以下のDirを作成
			i1  = atoi(sColumnValues[0]); // step_cnt
			wp1 = M2W(sColumnValues[1]);  // dir + "\"
			wp2 = M2W(sColumnValues[2]);  // name
			wp3 = M2W(sColumnValues[3]);  // path
			wp4 = iws_cats(2, _MdOp, (wp1 + i1));
			wp5 = iws_cats(2, wp4, wp2);
				// mkdir
				if(iF_mkdir(wp4))
				{
					P1("\033[96m+ ");
					P1W(wp4);
					P2("\033[0m");
					++RowCnt;
				}
				// 優先処理
				if(I_exec == I_CP)
				{
					if(CopyFileW(wp3, wp5, FALSE))
					{
						P1("\033[96m+ ");
						P1W(wp5);
						P2("\033[0m");
						++RowCnt;
					}
				}
				else if(I_exec >= I_MV)
				{
					// ReadOnly属性(1)を解除
					if((GetFileAttributesW(wp5) & FILE_ATTRIBUTE_READONLY))
					{
						SetFileAttributesW(wp5, FALSE);
					}
					// 既存File削除
					if(iF_chkExistPath(wp5))
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
						++RowCnt;
					}
					// rmdir
					if(I_exec == I_MV2)
					{
						if(RemoveDirectoryW(wp1))
						{
							P1("\033[94m- ");
							P1(sColumnValues[1]);
							P2("\033[0m");
							++RowCnt;
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
				if(iF_mkdir(_MdOp))
				{
					P1("\033[96m+ ");
					P1W(_MdOp);
					P2("\033[0m");
					++RowCnt;
				}
				// 先
				wp3 = iws_cats(2, _MdOp, wp2);
				// I_EXT, I_EXT2共、同名Fileは上書き
				if(I_exec == I_EXT)
				{
					if(CopyFileW(wp1, wp3, FALSE))
					{
						P1("\033[96m+ ");
						P1W(wp3);
						P2("\033[0m");
						++RowCnt;
					}
				}
				else if(I_exec == I_EXT2)
				{
					// ReadOnly属性(1)を解除
					if((GetFileAttributesW(wp3) & FILE_ATTRIBUTE_READONLY))
					{
						SetFileAttributesW(wp3, FALSE);
					}
					// Dir存在していれば削除しておく
					if(iF_chkExistPath(wp3))
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
						++RowCnt;
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
			i1 = atoi(sColumnValues[2]);
			// ReadOnly属性(1)を解除
			if((i1 & FILE_ATTRIBUTE_READONLY))
			{
				SetFileAttributesW(wp1, FALSE);
			}
			iVBM_push2(IVBM_trashbox, sColumnValues[0]);
			iVBM_push(IVBM_trashbox, "\n", 1);
		break;

		case(I_REP): // --replace
			wp1 = M2W(sColumnValues[0]); // type
			wp2 = M2W(sColumnValues[1]); // path
				if(*wp1 == 'f' && CopyFileW(_RepOp, wp2, FALSE))
				{
					P1("\033[96m+ ");
					P1W(wp2);
					P2("\033[0m");
					++RowCnt;
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
					++RowCnt;
				}
				// 空Dir削除
				if(I_exec == I_RM2)
				{
					wp2 = M2W(sColumnValues[1]); // dir
						// 空Dirである
						if(RemoveDirectoryW(wp2))
						{
							P1("\033[94m- ");
							P1W(wp2);
							P2("\033[0m");
							++RowCnt;
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
	P1(IESC_OPT21);
	P(
		"-- %u row%s in set (%.3f sec)\n"
		, RowCnt
		, (RowCnt > 1 ? "s" : "") // 複数形
		, iExecSec_next()
	);
	P1(IESC_OPT22);
	P2("--");
	iwav_print2(AryInDir, L"--  '", L"'\n");
	if(SqlCmd)
	{
		P("--  '%s'\n", SqlCmd);
	}
	if(_InDbn)
	{
		mp1 = W2M(_InDbn);
			P("--  -in        '%s'\n", mp1);
		ifree(mp1);
	}
	if(_OutDbn)
	{
		mp1 = W2M(_OutDbn);
			P("--  -out       '%s'\n", mp1);
		ifree(mp1);
	}
	if(! _Header)
	{
		P2("--  -noheader");
	}
	if(_Quote)
	{
		P("--  -quote     '%s'\n", _Quote);
	}
	if(_Separate)
	{
		P("--  -separate  '%s'\n", _Separate);
	}
	P2("--");
	P1(IESC_RESET);
}

VOID
print_version()
{
	P1(IESC_STR2);
	LN(80);
	P1(
		"\033[2G"	IWM_COPYRIGHT	"\n"
		"\033[5G"	IWM_FILENAME	"_"	IWM_UPDATE	" + "	LIB_IWMUTIL_FILENAME	" + SQLite"	SQLITE_VERSION	"\n"
	);
	LN(80);
	P1(IESC_RESET);
}

VOID
print_help()
{
	P1(
		"\033[1G"	IESC_TITLE1	" ファイル検索 "	IESC_RESET	"\n"
		"\n"
		"\033[5G"	IESC_STR1	IWM_FILENAME	IESC_OPT1	" [Dir]"	IESC_OPT2	" [Option]"	"\n"
		"\033[9G"	IESC_LBL1	"or"	"\n"
		"\033[5G"	IESC_STR1	IWM_FILENAME	IESC_OPT2	" [Option]"	IESC_OPT1	" [Dir]"	"\n"
		"\n"
		"\033[2G"	IESC_LBL1	"(例１)"	IESC_STR1	" 検索"	"\n"
		"\033[5G"	IWM_FILENAME	IESC_OPT1	" Dir"	IESC_OPT2	" -r -s=\"LN,path,size\" -w=\"name like '*.exe'\""	"\n"
		"\n"
		"\033[2G"	IESC_LBL1	"(例２)"	IESC_STR1	" 検索結果をファイルへ保存"	"\n"
		"\033[5G"	IWM_FILENAME	IESC_OPT1	" Dir1 Dir2"	IESC_OPT2	" -r -o=File"	"\n"
		"\n"
		"\033[2G"	IESC_LBL1	"(例３)"	IESC_STR1	" 検索対象をファイルから読込"	"\n"
		"\033[5G"	IWM_FILENAME	IESC_OPT2	" -i=File"	"\n"
		"\n"
		"\033[2G"	IESC_OPT1	"[Dir]"	"\n"
		"\033[5G"	IESC_STR1	"検索対象Dir／複数指定可"	"\n"
		"\033[5G"	IESC_LBL1	"(例)"	IESC_STR1	" \"c:\" \"d:\""	"\n"
		"\n"
		"\033[2G"	IESC_OPT2	"[Option]"	"\n"
		"\033[4G"	IESC_LBL2	"[基本操作]"	"\n"
		"\033[5G"	IESC_OPT21	"-recursive | -r"	"\n"
		"\033[9G"	IESC_STR1	"全階層を検索"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-depth=Num1,Num2 | -d=Num1,Num2"	"\n"
		"\033[9G"	IESC_STR1	"検索する階層を指定"	"\n"
		"\033[9G"	IESC_OPT22	"CurrentDir は階層=0"	"\n"
		"\n"
		"\033[9G"	IESC_LBL1	"(例１)"	IESC_STR1	" -d=0"	"\n"
		"\033[16G"	IESC_STR1	"0階層のみ検索"	"\n"
		"\n"
		"\033[9G"	IESC_LBL1	"(例２)"	IESC_STR1	" -d=0,2"	"\n"
		"\033[16G"	IESC_STR1	"0～2階層を検索"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-out=File | -o=File"	"\n"
		"\033[9G"	IESC_STR1	"出力ファイル"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-in=File | -i=File"	"\n"
		"\033[9G"	IESC_STR1	"入力ファイル"	"\n"
		"\033[9G"	IESC_OPT22	"検索対象Dirと併用できない"	"\n"
		"\n"
		"\033[4G"	IESC_LBL2	"[SQL関連]"	"\n"
		"\033[5G"	IESC_OPT21	"-select=Column1,Column2,... | -s=Column1,Column2,..."	"\n"
		IESC_STR1
		"\033[9G"	"LN        : 連番／1回のみ指定可"	"\n"
		"\033[9G"	"path      : dir\\name"	"\n"
		"\033[9G"	"dir       : ディレクトリ名"	"\n"
		"\033[9G"	"name      : ファイル名"	"\n"
		"\033[9G"	"type      : ディレクトリ = d／ファイル = f"	"\n"
		"\033[9G"	"attr_num  : 属性"	"\n"
		"\033[9G"	"attr      : 属性 \"[d|f][r|-][h|-][s|-][a|-]\""	"\n"
		"\033[9G"	"            [dir|file][read-only][hidden][system][archive]"	"\n"
		"\033[9G"	"size      : ファイルサイズ = byte"	"\n"
		"\033[9G"	"ctime_cjd : 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5"	"\n"
		"\033[9G"	"ctime     : 作成日時     \"yyyy-mm-dd hh:nn:ss\""	"\n"
		"\033[9G"	"mtime_cjd : 更新日時     ctime_cjd参照"	"\n"
		"\033[9G"	"mtime     : 更新日時     ctime参照"	"\n"
		"\033[9G"	"atime_cjd : アクセス日時 ctime_cjd参照"	"\n"
		"\033[9G"	"atime     : アクセス日時 ctime参照"	"\n"
		"\033[9G"	"*         : 全項目表示"	"\n"
		"\n"
		"\033[9G"	IESC_OPT22	"(補１) 初期値"	"\n"
		"\033[14G"	IESC_STR1	"-select=LN,path"	"\n"
		"\n"
		"\033[9G"	IESC_OPT22	"(補２) SQLite関数を利用可能"	"\n"
		IESC_STR1
		"\033[14G"	"abs(X)"	"\033[44G"	"avg(X)"	"\033[74G"	"changes()"	"\n"
		"\033[14G"	"char(X1,X2,...,XN)"	"\033[44G"	"coalesce(X,Y,...)"	"\033[74G"	"concat(X,...)"	"\n"
		"\033[14G"	"concat_ws(SEP,X,...)"	"\033[44G"	"count(*)"	"\033[74G"	"count(X)"	"\n"
		"\033[14G"	"format(FORMAT,...)"	"\033[44G"	"glob(X,Y)"	"\033[74G"	"group_concat(X)"	"\n"
		"\033[14G"	"group_concat(X,Y)"	"\033[44G"	"hex(X)"	"\033[74G"	"if(B1,V1,...)"	"\n"
		"\033[14G"	"ifnull(X,Y)"	"\033[44G"	"iif(B1,V1,...)"	"\033[74G"	"instr(X,Y)"	"\n"
		"\033[14G"	"last_insert_rowid()"	"\033[44G"	"length(X)"	"\033[74G"	"like(X,Y)"	"\n"
		"\033[14G"	"like(X,Y,Z)"	"\033[44G"	"likelihood(X,Y)"	"\033[74G"	"likely(X)"	"\n"
		"\033[14G"	"load_extension(X)"	"\033[44G"	"load_extension(X,Y)"	"\033[74G"	"lower(X)"	"\n"
		"\033[14G"	"ltrim(X)"	"\033[44G"	"ltrim(X,Y)"	"\033[74G"	"max(X)"	"\n"
		"\033[14G"	"max(X,Y,...)"	"\033[44G"	"min(X)"	"\033[74G"	"min(X,Y,...)"	"\n"
		"\033[14G"	"nullif(X,Y)"	"\033[44G"	"octet_length(X)"	"\033[74G"	"printf(FORMAT,...)"	"\n"
		"\033[14G"	"quote(X)"	"\033[44G"	"random()"	"\033[74G"	"randomblob(N)"	"\n"
		"\033[14G"	"replace(X,Y,Z)"	"\033[44G"	"round(X)"	"\033[74G"	"round(X,Y)"	"\n"
		"\033[14G"	"rtrim(X)"	"\033[44G"	"rtrim(X,Y)"	"\033[74G"	"sign(X)"	"\n"
		"\033[14G"	"soundex(X)"	"\033[44G"	"sqlite_compileoption_get(N)"	"\033[74G"	"sqlite_compileoption_used(X)"	"\n"
		"\033[14G"	"sqlite_offset(X)"	"\033[44G"	"sqlite_source_id()"	"\033[74G"	"sqlite_version()"	"\n"
		"\033[14G"	"string_agg(X,Y)"	"\033[44G"	"substr(X,Y)"	"\033[74G"	"substr(X,Y,Z)"	"\n"
		"\033[14G"	"substring(X,Y)"	"\033[44G"	"substring(X,Y,Z)"	"\033[74G"	"sum(X)"	"\n"
		"\033[14G"	"total(X)"	"\033[44G"	"total_changes()"	"\033[74G"	"trim(X)"	"\n"
		"\033[14G"	"trim(X,Y)"	"\033[44G"	"typeof(X)"	"\033[74G"	"unhex(X)"	"\n"
		"\033[14G"	"unhex(X,Y)"	"\033[44G"	"unicode(X)"	"\033[74G"	"unlikely(X)"	"\n"
		"\033[14G"	"upper(X)"	"\033[44G"	"zeroblob(N)"	"\033[74G"	""	"\n"
		IESC_LBL2
		"\033[14G"	"(参考)"	"\n"
		"\033[18G"	"https://www.sqlite.org/lang_aggfunc.html#aggfunclist"	"\n"
		"\033[18G"	"https://www.sqlite.org/lang_corefunc.html"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-where=Str | -w=Str"	"\n"
		"\033[9G"	IESC_LBL1	"(例１)"	IESC_STR1	" \"size <= 100 or size > 1000000\""	"\n"
		"\n"
		"\033[9G"	IESC_LBL1	"(例２)"	IESC_STR1	" \"name like '%.jpg' or name like '%.png'\""	"\n"
		"\033[16G"	"'?' '_' は任意の1文字"	"\n"
		"\033[16G"	"'*' '%' は任意の0文字以上"	"\n"
		"\n"
		"\033[9G"	IESC_LBL1	"(例３)"	IESC_STR1	" 基準日 \"2010-12-10 12:30:00\" のとき"	"\n"
		"\033[16G"	"\"ctime >= [-10d]\"  : ctime >= '2010-11-30 12:30:00'"	"\n"
		"\033[16G"	"\"ctime >= [-10D]\"  : ctime >= '2010-11-30 00:00:00'"	"\n"
		"\033[16G"	"\"ctime >= [-10d%]\" : ctime >= '2010-11-30 %'"	"\n"
		"\033[16G"	"\"ctime like [%]\"   : ctime like '2010-12-10 %'"	"\n"
		"\033[16G"	"(年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-group=Str | -g=Str"	"\n"
		"\033[9G"	IESC_LBL1	"(例)"	IESC_STR1	" -g=\"Str1, Str2\""	"\n"
		"\033[14G"	"Str1とStr2をグループ毎にまとめる"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-sort=\"Str [ASC|DESC]\" | -st=\"Str [ASC|DESC]\""	"\n"
		"\033[9G"	IESC_LBL1	"(例)"	IESC_STR1	" -st=\"Str1 ASC, Str2 DESC\""	"\n"
		"\033[14G"	"Str1を順ソート, Str2を逆順ソート"	"\n"
		"\n"
		"\033[4G"	IESC_LBL2	"[出力フォーマット]"	"\n"
		"\033[5G"	IESC_OPT21	"-noheader | -nh"	"\n"
		"\033[9G"	IESC_STR1	"ヘッダ情報を表示しない"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-nofooter | -nf"	"\n"
		"\033[9G"	IESC_STR1	"フッタ情報を表示しない"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-quote=Str | -qt=Str"	"\n"
		"\033[9G"	IESC_STR1	"囲み文字"	"\n"
		"\033[9G"	IESC_LBL1	"(例)"	IESC_STR1	" -qt=\"'\""	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"-separate=Str | -sp=Str"	"\n"
		"\033[9G"	IESC_STR1	"区切り文字"	"\n"
		"\033[9G"	IESC_LBL1	"(例)"	IESC_STR1	" -sp=\"\\t\""	"\n"
		"\n"
		"\033[4G"	IESC_LBL2	"[出力結果を操作]"	"\n"
		"\033[5G"	IESC_OPT21	"--mkdir=Dir | --md=Dir"	"\n"
		"\033[9G"	IESC_STR1	"検索結果のDirをコピー作成する (-recursive のとき 階層維持)"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--copy=Dir | --cp=Dir"	"\n"
		"\033[9G"	IESC_STR1	"--mkdir + 検索結果をDirにコピーする (-recursive のとき 階層維持)"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--move=Dir | --mv=Dir"	"\n"
		"\033[9G"	IESC_STR1	"--mkdir + 検索結果をDirに移動する (-recursive のとき 階層維持)"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--move2=Dir | --mv2=Dir"	"\n"
		"\033[9G"	IESC_STR1	"--mkdir + --move + 移動元の空Dirを削除する (-recursive のとき 階層維持)"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--extract=Dir | --ext=Dir"	"\n"
		"\033[9G"	IESC_STR1	"--mkdir + 検索結果ファイルのみ抽出しDirにコピーする"	"\n"
		"\033[9G"	"階層を維持しない／同名ファイルは上書き"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--extract2=Dir | --ext2=Dir"	"\n"
		"\033[9G"	IESC_STR1	"--mkdir + 検索結果ファイルのみ抽出しDirに移動する"	"\n"
		"\033[9G"	"階層を維持しない／同名ファイルは上書き"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--remove | --rm"	"\n"
		"\033[9G"	IESC_STR1	"検索結果のFileのみ削除する (Dirは削除しない) "	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--remove2 | --rm2"	"\n"
		"\033[9G"	IESC_STR1	"--remove + 空Dirを削除する"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--trashbox | --tb"	"\n"
		"\033[9G"	IESC_STR1	"検索結果をゴミ箱へ移動する"	"\n"
		"\n"
		"\033[5G"	IESC_OPT21	"--replace=File | --rep=File"	"\n"
		"\033[9G"	IESC_STR1	"検索結果(複数) をFileの内容で置換(上書き)する／ファイル名は変更しない"	"\n"
		"\033[9G"	IESC_LBL1	"(例)"	IESC_STR1	" -w=\"name like 'before.txt'\" --rep=\".\\after.txt\""	"\n"
		"\n"
	);
	P1(IESC_STR2);
	LN(80);
	P1(IESC_RESET);
}
