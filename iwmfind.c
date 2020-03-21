//----------------------------------------------------------------
#define   IWMFIND_VERSION     "iwmfind3_20200223"
#define   IWMFIND_COPYRIGHT   "Copyright (C)2009-2020 iwm-iwama"
//----------------------------------------------------------------
#include  "lib_iwmutil.h"
#include  "sqlite3.h"
MBS  *str_encode(MBS *pM);
MBS  *sql_escape(MBS *pM);
VOID ifind10($struct_iFinfoW *FI, WCS *dir, UINT dirLenJ, INT depth);
VOID ifind21(WCS *dir, UINT dirId, INT depth);
VOID ifind22($struct_iFinfoW *FI, UINT dirId, WCS *fname);
VOID sql_exec(sqlite3 *db, const MBS *sql, sqlite3_callback cb);
INT  print_result(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
BOOL sql_saveOrLoadMemdb(sqlite3 *mem_db, MBS *ofn, BOOL save_load);
VOID print_footer();
VOID print_version();
VOID print_help();
//-----------
// 共用変数
//-----------
UINT $execMS = 0;
MBS  *$program = 0;
MBS  *$sTmp = 0;    // 後で icalloc_MBS(IGET_ARGS_LEN_MAX)
U8N  *$sqlU = 0;
UINT $uDirId = 0;   // Dir数
UINT $uAllCnt = 0;  // 検索数
UINT $uStepCnt = 0; // Currentdir位置
UINT $uRowCnt = 0;  // 処理行数
sqlite3      *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;
// [文字色] + ([背景色] * 16)
//  0 = Black    1 = Navy     2 = Green    3 = Teal
//  4 = Maroon   5 = Purple   6 = Olive    7 = Silver
//  8 = Gray     9 = Blue    10 = Lime    11 = Aqua
// 12 = Red     13 = Fuchsia 14 = Yellow  15 = White
#define   ColorHeaderFooter   (10 + ( 0 * 16))
#define   ColorExp1           (12 + ( 0 * 16))
#define   ColorExp2           (14 + ( 0 * 16))
#define   ColorExp3           (11 + ( 0 * 16))
#define   ColorText1          (15 + ( 0 * 16))
#define   ColorText2          ( 7 + ( 0 * 16))
#define   ColorBgText1        (15 + (12 * 16))

UINT _getColorDefault = 0;
#define   MEMDB     ":memory:"
#define   OLDDB     ("iwmfind.db."IWMFIND_VERSION)
#define   CREATE_T_DIR \
			"CREATE TABLE T_DIR( \
				dir_id INTEGER, \
				dir TEXT, \
				depth INTEGER, \
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
				ext_pos   INTEGER, \
				attr_num  INTEGER, \
				ctime_cjd DOUBLE, \
				mtime_cjd DOUBLE, \
				atime_cjd DOUBLE, \
				size      INTEGER, \
				number    INTEGER, \
				flg       INTEGER \
			);"
#define   INSERT_T_FILE \
			"INSERT INTO T_FILE( \
				id, \
				dir_id, \
				name, \
				ext_pos, \
				attr_num, \
				ctime_cjd, \
				mtime_cjd, \
				atime_cjd, \
				size, \
				number, \
				flg \
			) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
#define   CREATE_VIEW \
			"CREATE VIEW V_INDEX AS SELECT T_FILE.id, \
				(T_DIR.dir || T_FILE.name) AS path, \
				T_DIR.dir AS dir, \
				T_FILE.name AS name, \
				substr(T_FILE.name, T_FILE.ext_pos + 1) AS ext, \
				(CASE T_FILE.ext_pos WHEN 0 THEN 'd' ELSE 'f' END) AS type, \
				T_FILE.attr_num AS attr_num, \
				((CASE T_FILE.ext_pos WHEN 0 THEN 'd' ELSE 'f' END) || (CASE 1&T_FILE.attr_num WHEN 1 THEN 'r' ELSE '-' END) || (CASE 2&T_FILE.attr_num WHEN 2 THEN 'h' ELSE '-' END) || (CASE 4&T_FILE.attr_num WHEN 4 THEN 's' ELSE '-' END) || (CASE 32&T_FILE.attr_num WHEN 32 THEN 'a' ELSE '-' END)) AS attr, \
				T_FILE.ctime_cjd AS ctime_cjd, \
				datetime(T_FILE.ctime_cjd - 0.5) AS ctime, \
				T_FILE.mtime_cjd AS mtime_cjd, \
				datetime(T_FILE.mtime_cjd - 0.5) AS mtime, \
				T_FILE.atime_cjd AS atime_cjd, \
				datetime(T_FILE.atime_cjd - 0.5) AS atime, \
				T_FILE.size AS size, \
				T_FILE.number AS number, \
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
			"SELECT %s FROM V_INDEX %s %s %s ORDER BY %s;"
#define   OP_SELECT_0 \
			"number, path"
#define   OP_SELECT_1 \
			"number, path, depth, type, size, ctime, mtime, atime"
#define   OP_SELECT_MKDIR \
			"step_byte, dir, name, path"
#define   OP_SELECT_EXTRACT \
			"path, name"
#define   OP_SELECT_RP \
			"type, path"
#define   OP_SELECT_RM \
			"path, dir, attr_num"
#define   I_MKDIR    1
#define   I_CP       2
#define   I_MV       3
#define   I_MV2      4
#define   I_EXT1     5
#define   I_EXT2     6
#define   I_RP      11
#define   I_RM      21
#define   I_RM2     22
/*
	現在時間
*/
INT *_aiNow = 0;
/*
	検索dir
*/
MBS **_aDir = 0;
INT _aDirNum = 0;
/*
	入力DB
	-in | -i FileName
*/
MBS *_sIn = 0;
MBS *_sInDbn = 0;
/*
	出力DB
	-out | -o FileName
*/
MBS *_sOut = 0;
MBS *_sOutDbn = 0;
/*
	select 句
	-select | -s ColumnName
*/
MBS *_sSelect = 0;
INT _iSelectPosNum = 0; // "number"の配列位置
/*
	where 句
	-where | -w STR
*/
// xMBS *_where = 0;
MBS *_sWhere0 = 0;
MBS *_sWhere1 = 0;
MBS *_sWhere2 = 0;
/*
	group by 句
	-group | -g ColumnName
*/
MBS *_sGroup = 0;
/*
	order by 句
	-sort | -st ColumnName
*/
MBS *_sSort = 0;
/*
	フッタ情報を非表示
	-noguide | -ng
*/
BOOL _bNoGuide = FALSE;
/*
	出力を STR で囲む
	-quote | -qt STR
*/
MBS *_sQuote = 0;
/*
	出力の前後にコメントを付加
	-table STR1 STR2
*/
MBS *_sTable1 = 0;
MBS *_sTable2 = 0;
/*
	出力行の前後にコメントを付加
	-tr STR1 STR2
*/
MBS *_sTr1 = 0;
MBS *_sTr2 = 0;
/*
	出力を STR で区切る
	-td STR1 STR2
*/
MBS *_sTd1 = 0;
MBS *_sTd2 = 0;
/*
	HTML Table出力
		*_sTable1 = "<table border=\"1\">"
		*_sTable2 = "</table>"
		*_sTr1    = "<tr>"
		*_sTr2    = "</tr>"
		*_sTd1    = "<td>"
		*_sTd2    = "</td>"
	と同義
*/
BOOL _bHtml = FALSE;
/*
	検索Dir位置
	-depth | -d NUM1 NUM2
*/
INT _iMinDepth = 0; // 下階層～開始位置(相対)
INT _iMaxDepth = 0; // 下階層～終了位置(相対)
/*
	--mkdir | --md DIR
	mkdir DIR
*/
MBS *_sMd = 0;
MBS *_sOpMd = 0;
/*
	--copy | --cp DIR
	mkdir DIR & copy
*/
MBS *_sCp = 0;
/*
	--move | --mv DIR
	mkdir DIR & move DIR
*/
MBS *_sMv = 0;
/*
	--move2 | --mv2 DIR
	mkdir DIR & move DIR & rmdir
*/
MBS *_sMv2 = 0;
/*
	--extract1 | --ext1 DIR
	copy FileOnly
*/
MBS *_sExt1 = 0;
/*
	--extract2 | --ext2 DIR
	move FileOnly
*/
MBS *_sExt2 = 0;
/*
	--replace | --rep FILE
	replace results with FILE
*/
MBS *_sRep = 0;
MBS *_sOpRep = 0;
/*
	--rm  | --remove
	--rm2 | --remove2
*/
INT _iRm = 0;
/*
	_iExec
	優先順 (安全)I_MD > (危険)I_RM2
		I_MD  = --mkdir
		I_CP  = --cp
		I_MV  = --mv
		I_MV2 = --mv2
		I_RP  = --rep
		I_RM  = --rm
		I_RM2 = --rm2
*/
INT _iExec = 0;
/*
	_iPriority
	実行優先順
		1 = 優先
		2 = 通常以上
		3 = 通常(default)
		4 = 通常以下
		5 = アイドル時
*/
#define   PRIORITY_DEFAULT    3
INT _iPriority = PRIORITY_DEFAULT;

INT
main()
{
	// 実行時間
	$execMS = iExecSec_init();
	// 現在時間
	_aiNow = (INT*)idate_cjd_to_iAryYmdhns(idate_nowToCjd(TRUE));
	// 大域変数
	$sTmp = icalloc_MBS(IGET_ARGS_LEN_MAX + 1); // +"\0"
	_getColorDefault = iConsole_getBgcolor();   // 現在の文字色／背景色
	// 変数
	$program = iCmdline_getCmd();
	MBS **args = iCmdline_getArgs();
	INT argc = iargs_size(args);
	MBS **ap1 = {0}, **ap2 = {0}, **ap3 = {0};
	MBS *p1 = 0, *p2 = 0;
	INT i1 = 0, i2 = 0;
	// -help | -h
	ap1 = iargs_option(args, "-help", "-h");
		if($IWM_bSuccess || !**args)
		{
			print_help();
			imain_end();
		}
	ifree(ap1);
	// -version | -v
	ap1 = iargs_option(args, "-version", "-v");
		if($IWM_bSuccess)
		{
			print_version();
			LN();
			imain_end();
		}
	ifree(ap1);
	/*
		_aDir取得で _iMaxDepth を使うため先に、
			-recursive
			-depth
		をチェック
	*/
	// -recursive | -r
	ap1 = iargs_option(args, "-recursive", "-r");
		if($IWM_bSuccess)
		{
			_iMinDepth = 0;
			_iMaxDepth = ($IWM_uAryUsed ? inum_atoi(*(ap1 + 0)) : IMAX_PATHA);
		}
	ifree(ap1);
	// -depth | -d
	ap1 = iargs_option(args, "-depth", "-d");
		if($IWM_bSuccess)
		{
			if($IWM_uAryUsed > 1)
			{
				_iMinDepth = inum_atoi(*(ap1 + 0));
				_iMaxDepth = inum_atoi(*(ap1 + 1));
			}
			else if($IWM_uAryUsed == 1)
			{
				_iMinDepth = _iMaxDepth = inum_atoi(*(ap1 + 0));
			}
			else
			{
				_iMinDepth = _iMaxDepth = 0;
			}
		}
	ifree(ap1);
	/*
		_iMinDepth
		_iMaxDepth
		最終チェック
	*/
	// swap
	if(_iMinDepth > _iMaxDepth)
	{
		i1 = _iMinDepth;
		_iMinDepth = _iMaxDepth;
		_iMaxDepth = i1;
	}
	// _aDir を作成
	for(i1 = 0, i2 = 0; i1 < argc; i1++)
	{
		p1 = *(args + i1);
		if(*p1 == '-')
		{
			break;
		}
		// dir不在
		if(iFchk_typePathA(p1) != 1)
		{
			fprintf(stderr, "[Err] Dir(%d) '%s' は存在しない!\n", (i1 + 1), p1);
			++i2;
		}
	}
	if(i2)
	{
		imain_end(); // 処理中止
	}
	ap1 = icalloc_MBS_ary(i1);
		for(i1 = 0; i1 < argc; i1++)
		{
			p1 = *(args + i1);
			// 比較は null=>文字列 比較でないとエラー
			if(!p1 || *p1 == '-')
			{
				break;
			}
			if(iFchk_typePathA(p1) == 1)
			{
				*(ap1 + i1) = iFget_AdirA(p1); // 絶対PATHへ変換
			}
		}
		// 上位Dirのみ取得
		_aDir = iary_higherDir(ap1, _iMaxDepth);
		_aDirNum = $IWM_uAryUsed;
	ifree(ap1);
	// -in | -i
	ap1 = iargs_option(args, "-in", "-i");
		_sIn = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		if(*_sIn)
		{
			// DB不在
			if(iFchk_typePathA(_sIn) != 2)
			{
				fprintf(stderr, "[Err] -in '%s' は存在しない!\n", _sIn);
				imain_end();
			}
			if(_aDirNum)
			{
				fprintf(stderr, "[Err] Dir と -in は併用できない!\n");
				imain_end();
			}
			_sInDbn = ims_clone(_sIn);
			// -in のときは -recursive 自動付与
			_iMinDepth = 0;
			_iMaxDepth = IMAX_PATHA;
		}
		else
		{
			_sInDbn = MEMDB;
		}
	ifree(ap1);
	// -out | -o
	ap1 = iargs_option(args, "-out", "-o");
		_sOut = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		if(*_sOut)
		{
			// Dir or inDb
			if(!*_sIn && !_aDirNum)
			{
				fprintf(stderr, "[Err] Dir もしくは -in を指定してください!\n");
				imain_end();
			}
			_sOutDbn = ims_clone(_sOut);
		}
		else
		{
			_sOutDbn = "";
		}
	ifree(ap1);
	// Err処理
	if(!*_sIn && !*_sOut && !_aDirNum)
	{
		print_help();
		imain_end();
	}
	// --mkdir | --md
	ap1 = iargs_option(args, "--mkdir", "--md");
		_sMd = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --copy | --cp
	ap1 = iargs_option(args, "--copy", "--cp");
		_sCp = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --move | --mv
	ap1 = iargs_option(args, "--move", "--mv");
		_sMv = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --move2 | --mv2
	ap1 = iargs_option(args, "--move2", "--mv2");
		_sMv2 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --extract1 | --ext1
	ap1 = iargs_option(args, "--extract1", "--ext1");
		_sExt1 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --extract2 | --ext2
	ap1 = iargs_option(args, "--extract2", "--ext2");
		_sExt2 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --replace | --rep
	ap1 = iargs_option(args, "--replace", "--rep");
		_sRep = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --remove | --rm
	ap1 = iargs_option(args, "--remove", "--rm");
		if($IWM_bSuccess)
		{
			_iRm = I_RM;
		}
	ifree(ap1);
	// --remove2 | --rm2
	ap1 = iargs_option(args, "--remove2", "--rm2");
		if($IWM_bSuccess)
		{
			_iRm = I_RM2;
		}
	ifree(ap1);
	// --[exec]
	if(*_sMd || *_sCp || *_sMv || *_sMv2 || *_sExt1 || *_sExt2 || *_sRep || _iRm)
	{
		if(*_sMd)
		{
			_iExec = I_MKDIR;
			_sOpMd = ims_clone(_sMd);
		}
		else if(*_sCp)
		{
			_iExec = I_CP;
			_sOpMd = ims_clone(_sCp);
		}
		else if(*_sMv)
		{
			_iExec = I_MV;
			_sOpMd = ims_clone(_sMv);
		}
		else if(*_sMv2)
		{
			_iExec = I_MV2;
			_sOpMd = ims_clone(_sMv2);
		}
		else if(*_sExt1)
		{
			_iExec = I_EXT1;
			_sOpMd = ims_clone(_sExt1);
		}
		else if(*_sExt2)
		{
			_iExec = I_EXT2;
			_sOpMd = ims_clone(_sExt2);
		}
		else if(*_sRep)
		{
			if(iFchk_typePathA(_sRep) != 2)
			{
				fprintf(stderr, "[Err] --replace '%s' は存在しない!\n", _sRep);
				imain_end();
			}
			else
			{
				_iExec = I_RP;
				_sOpRep = ims_clone(_sRep);
			}
		}
		else if(_iRm)
		{
			_iExec = _iRm;
		}

		_bNoGuide = TRUE; // フッタ情報を非表示

		if(_iExec <= I_EXT2)
		{
			p1 = iFget_AdirA(_sOpMd);
				_sOpMd = ijs_cutR(p1, "\\");
			ifree(p1);
			_sSelect = (_iExec <= I_MV2 ? OP_SELECT_MKDIR : OP_SELECT_EXTRACT);
		}
		else if(_iExec == I_RP)
		{
			_sSelect = OP_SELECT_RP;
		}
		else
		{
			_sSelect = OP_SELECT_RM;
		}
	}
	else
	{
		_sOpMd = "";
		/*
			(none)         : OP_SELECT_0
			-select (none) : OP_SELECT_1
			-select COLUMN1, COLUMN2, ...
		*/
		// -select | -s
		ap1 = iargs_option(args, "-select", "-s");
			if($IWM_bSuccess)
			{
				if($IWM_uAryUsed)
				{
					// "number"位置を求める
					ap2 = ija_token(*(ap1 + 0), ", ");
						ap3 = iary_simplify(ap2, TRUE); // number表示は１個しか出来ないので重複排除
							_iSelectPosNum = 0;
							while((p2 = *(ap3 + _iSelectPosNum)))
							{
								if(imb_cmppi(p2, "number"))
								{
									break;
								}
								++_iSelectPosNum;
							}
							if(!p2)
							{
								_iSelectPosNum = -1;
							}
							_sSelect = iary_toA(ap3, ", ");
						ifree(ap3);
					ifree(ap2);
				}
				else
				{
					_sSelect = OP_SELECT_1;
				}
			}
			else
			{
				_sSelect = OP_SELECT_0;
			}
		ifree(ap1);
		// -sort
		ap1 = iargs_option(args, "-sort", "-st");
			_sSort = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		ifree(ap1);
	}
	/*
		_sSort => SORT関係を一括変換
	*/
	if(_iExec >= I_MV)
	{
		// Dir削除時必要なものは "order by desc"
		ifree(_sSort);
		_sSort = "path desc";
	}
	else
	{
		if(!_sSort || !*_sSort)
		{
			ifree(_sSort);
			_sSort = "dir, name";
		}
		/*
			path, dir, name, ext
			ソートは、大文字・小文字を区別しない
		*/
		p1 = ims_clone(_sSort);
			p2 = ijs_replace(p1, "path", "lower(path)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "dir", "lower(dir)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "name", "lower(name)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "ext", "lower(ext)");
				ifree(p1);
				p1 = p2;
			_sSort = ims_clone(p1);
		ifree(p1);
	}
	// -where | -w
	snprintf($sTmp, IGET_ARGS_LEN_MAX, "depth>=%d AND depth<=%d", _iMinDepth, _iMaxDepth);
		_sWhere2 = ims_clone($sTmp);
	ap1 = iargs_option(args, "-where", "-w");
		if($IWM_uAryUsed)
		{
			_sWhere0 = sql_escape(*(ap1 + 0));
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "WHERE %s AND", _sWhere0);
			_sWhere1 = ims_clone($sTmp);
		}
		else
		{
			_sWhere0 = "";
			_sWhere1 = "WHERE";
		}
	ifree(ap1);
	// -group | -g
	ap1 = iargs_option(args, "-group", "-g");
		if($IWM_uAryUsed)
		{
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "GROUP BY %s", *(ap1 + 0));
			_sGroup = ims_clone($sTmp);
		}
		else
		{
			_sGroup = "";
		}
	ifree(ap1);
	// -noguide | -ng
	ap1 = iargs_option(args, "-noguide", "-ng");
		_bNoGuide = $IWM_bSuccess;
	ifree(ap1);
	// -quote | -qt
	ap1 = iargs_option(args, "-quote", "-qt");
		_sQuote = ($IWM_uAryUsed ? str_encode(*(ap1 + 0)) : "");
	ifree(ap1);
	// -table
	ap1 = iargs_option(args, "-table", NULL);
		_sTable1 = ($IWM_uAryUsed > 0 ? str_encode(*(ap1 + 0)) : "");
		_sTable2 = ($IWM_uAryUsed > 1 ? str_encode(*(ap1 + 1)) : "");
	ifree(ap1);
	// -tr
	ap1 = iargs_option(args, "-tr", NULL);
		_sTr1 = ($IWM_uAryUsed > 0 ? str_encode(*(ap1 + 0)) : "");
		_sTr2 = ($IWM_uAryUsed > 1 ? str_encode(*(ap1 + 1)) : "");
	ifree(ap1);
	// -td
	ap1 = iargs_option(args, "-td", NULL);
		if($IWM_uAryUsed > 1)
		{
			_sTd1 = str_encode(*(ap1 + 0));
			_sTd2 = str_encode(*(ap1 + 1));
		}
		else if($IWM_uAryUsed == 1)
		{
			_sTd1 = "";
			_sTd2 = str_encode(*(ap1 + 0));
		}
		else
		{
			_sTd1 = "";
			_sTd2 = " | ";
		}
	ifree(ap1);
	// -html
	ap1 = iargs_option(args, "-html", NULL);
		_bHtml = $IWM_bSuccess;
		if(_bHtml)
		{
			// free
			ifree(_sTable1);
			ifree(_sTable2);
			ifree(_sTr1);
			ifree(_sTr2);
			ifree(_sTd1);
			ifree(_sTd2);
			// 再定義
			_sTable1 = "<table border=\"1\">";
			_sTable2 = "</table>";
			_sTr1    = "<tr>";
			_sTr2    = "</tr>";
			_sTd1    = "<td>";
			_sTd2    = "</td>";
		}
	ifree(ap1);
	// ---priority | ---pr
	ap1 = iargs_option(args, "---priority", "---pr");
		i1 = ($IWM_uAryUsed ? inum_atoi(*(ap1 + 0)) : 0);
		if(i1 < 1 || i1 > 5)
		{
			i1 = PRIORITY_DEFAULT; // default=3
		}
		_iPriority = i1;
	ifree(ap1);
	// 実行優先度設定
	iwin_set_priority(_iPriority);
	// SQL作成
	// SJIS で作成（DOSプロンプト対応）
	i1 = imi_len(SELECT_VIEW) + imi_len(_sSelect) + imi_len(_sWhere1) + imi_len(_sWhere2) + imi_len(_sGroup) + imi_len(_sSort) + 1;
	snprintf($sTmp, IGET_ARGS_LEN_MAX, SELECT_VIEW, _sSelect, _sWhere1, _sWhere2, _sGroup, _sSort);
	// SJIS を UTF-8 に変換（Sqlite3対応）
	$sqlU = M2U($sTmp);
	// -table "header"
	if(*_sTable1)
	{
		P2(_sTable1); // ""以外のとき
	}
	// -in DBを指定
	// UTF-8
	U8N *up1 = M2U(_sInDbn);
		if(sqlite3_open(up1, &$iDbs))
		{
			fprintf(stderr, "[Err] -in '%s' を開けない!\n", _sInDbn);
			sqlite3_close($iDbs); // ErrでもDB解放
			imain_end();
		}
		else
		{
			// DB構築
			if(!*_sIn)
			{
				$struct_iFinfoW *FI = iFinfo_allocW();
					/*
					【重要】UTF-8でDB構築
						データ入力(INSERT)は、
							・PRAGMA encoding = 'TTF-16le'
							・bind_text16()
						を使用して、"UTF-16LE"で登録可能だが、
						出力(SELECT)は、exec()が"UTF-8"しか扱わないため本時点では、
							＜入力＞ UTF-16 => UTF-8
							＜SQL＞  SJIS   => UTF-8
							＜出力＞ UTF-8  => SJIS
						の方法に辿り着いた。
					*/
					sql_exec($iDbs, "PRAGMA encoding='UTF-8';", NULL);
					// TABLE作成
					sql_exec($iDbs, CREATE_T_DIR, NULL);
					sql_exec($iDbs, CREATE_T_FILE, NULL);
					// VIEW作成
					sql_exec($iDbs, CREATE_VIEW, NULL);
					// 前処理
					sqlite3_prepare($iDbs, INSERT_T_DIR, imi_len(INSERT_T_DIR), &$stmt1, NULL);
					sqlite3_prepare($iDbs, INSERT_T_FILE, imi_len(INSERT_T_FILE), &$stmt2, NULL);
					// トランザクション開始
					sql_exec($iDbs, "BEGIN", NULL);
					// 検索データ DB書込
					WCS *wp1 = 0;
					for(i2 = 0; (p1 = *(_aDir + i2)); i2++)
					{
						wp1 = M2W(p1);
							$uStepCnt = iwi_len(wp1);
							ifind10(FI, wp1, $uStepCnt, 0); // 本処理
						ifree(wp1);
					}
					// トランザクション終了
					sql_exec($iDbs, "COMMIT", NULL);
					// 後処理
					sqlite3_finalize($stmt2);
					sqlite3_finalize($stmt1);
				iFinfo_freeW(FI);
			}
			// 結果
			if(*_sOut)
			{
				// 存在する場合、削除
				irm_file(_sOutDbn);
				// _sIn, _sOut両指定、別ファイル名のとき
				if(*_sIn)
				{
					CopyFile(_sInDbn, OLDDB, FALSE);
				}
				// WHERE STR のとき不要データ削除
				if(imi_len(_sWhere0))
				{
					sql_exec($iDbs, "BEGIN", NULL); // トランザクション開始
						p1 = ims_cat_clone("WHERE ", _sWhere0);
							snprintf($sTmp, IGET_ARGS_LEN_MAX, UPDATE_EXEC99_1, p1);
						ifree(p1);
						p1 = M2U($sTmp); // UTF-8で処理
							sql_exec($iDbs, p1, NULL);              // フラグを立てる
							sql_exec($iDbs, DELETE_EXEC99_1, NULL); // 不要データ削除
							sql_exec($iDbs, UPDATE_EXEC99_2, NULL); // フラグ初期化
						ifree(p1);
					sql_exec($iDbs, "COMMIT", NULL); // トランザクション終了
					sql_exec($iDbs, "VACUUM", NULL); // VACUUM
				}
				// _sIn, _sOut 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
				sql_saveOrLoadMemdb($iDbs, (*_sIn ? _sInDbn : _sOutDbn), TRUE);
				// outDb 出力数カウント
				_iExec = 99;
				sql_exec($iDbs, "SELECT number FROM V_INDEX;", print_result); // "SELECT *" は遅い
			}
			else
			{
				// 画面表示
				sql_exec($iDbs, $sqlU, print_result);
			}
			// DB解放
			sqlite3_close($iDbs);
			// _sIn, _sOut ファイル名をswap
			if(*_sIn)
			{
				MoveFile(_sInDbn, _sOutDbn);
				MoveFile(OLDDB, _sInDbn);
			}
			// 作業用ファイル削除
			DeleteFile(OLDDB);
		}
	ifree(up1);
	//
	// -table "footer"
	//
	if(*_sTable2)
	{
		P2(_sTable2); // ""以外のとき
	}
	//
	// フッタ部
	//
	if(_bNoGuide || _bHtml)
	{
		// footerを表示しない
	}
	else
	{
		print_footer();
	}
	//
	// デバッグ用
	//
	///icalloc_mapPrint();ifree_all();icalloc_mapPrint();
	//
	imain_end();
}

MBS
*str_encode(
	MBS *pM
)
{
	MBS *p1 = ims_clone(pM);
	MBS *p2 = 0;
	p2 = ijs_replace(p1, "\\a", "\a"); // "\a"
		ifree(p1);
		p1 = p2;
	p2 = ijs_replace(p1, "\\t", "\t"); // "\t"
		ifree(p1);
		p1 = p2;
	p2 = ijs_replace(p1, "\\n", "\n"); // "\n"
		ifree(p1);
		p1 = p2;
	return p1;
}

MBS
*sql_escape(
	MBS *pM
)
{
	MBS *p1 = ims_clone(pM);
	MBS *p2 = 0;
		p2 = ijs_replace(p1, ";", " "); // ";" => " " SQLインジェクション回避
	ifree(p1);
		p1 = p2;
		p2 = ijs_replace(p1, "*", "%"); // "*" => "%"
	ifree(p1);
		p1 = p2;
		p2 = ijs_replace(p1, "?", "_"); // "?" => "_"
	ifree(p1);
		p1 = p2;
		p2 = idate_replace_format_ymdhns(
			p1,
			"[", "]",
			"'",
			_aiNow[0],
			_aiNow[1],
			_aiNow[2],
			_aiNow[3],
			_aiNow[4],
			_aiNow[5]
		); // [] を日付に変換
	ifree(p1);
		p1 = p2;
	return p1;
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
	※DirとFileを別テーブルで管理／joinして使用するため、
	　このような仕様にならざるを得ない。
*/
VOID
ifind10(
	$struct_iFinfoW *FI,
	WCS *dir,
	UINT dirLenJ,
	INT depth
)
{
	if(depth > _iMaxDepth)
	{
		return;
	}
	WIN32_FIND_DATAW F;
	WCS *p1 = iwp_cpy(FI->fullnameW, dir);
		iwp_cpy(p1, L"*");
	HANDLE hfind = FindFirstFileW(FI->fullnameW, &F);
		// 読み飛ばす Depth
		BOOL bMinDepthFlg = (depth >= _iMinDepth ? TRUE : FALSE);
		// dirIdを逐次生成
		UINT dirId = (++$uDirId);
		// Dir
		if(bMinDepthFlg)
		{
			ifind21(dir, dirId, depth);
			ifind22(FI, dirId, L"");
		}
		// File
		do
		{
			if(iFinfo_initW(FI, &F, dir, dirLenJ, F.cFileName))
			{
				if((FI->iFtype) == 1)
				{
					p1 = iws_clone(FI->fullnameW);
						// 下位Dirへ
						ifind10(FI, p1, FI->iEnd, depth + 1);
					ifree(p1);
				}
				else if(bMinDepthFlg)
				{
					ifind22(FI, dirId, F.cFileName);
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
}

VOID
ifind21(
	WCS *dir,
	UINT dirId,
	INT depth
)
{
	sqlite3_reset($stmt1);
		sqlite3_bind_int64($stmt1, 1, dirId);
		sqlite3_bind_text16($stmt1, 2, dir, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt1, 3, depth);
		sqlite3_bind_int($stmt1, 4, $uStepCnt);
	sqlite3_step($stmt1);
}

VOID
ifind22(
	$struct_iFinfoW *FI,
	UINT dirId,
	WCS *fname
)
{
	sqlite3_reset($stmt2);
		sqlite3_bind_int64($stmt2, 1, ++$uAllCnt);
		sqlite3_bind_int64($stmt2, 2, dirId);
		sqlite3_bind_text16($stmt2, 3, fname, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt2, 4, (FI->iExt) - (FI->iFname));
		sqlite3_bind_int($stmt2, 5, FI->iAttr);
		sqlite3_bind_double($stmt2, 6, FI->cjdCtime);
		sqlite3_bind_double($stmt2, 7, FI->cjdMtime);
		sqlite3_bind_double($stmt2, 8, FI->cjdAtime);
		sqlite3_bind_int64 ($stmt2, 9, FI->iFsize);
	sqlite3_step($stmt2);
}

/*
	SQLの実行
*/
VOID
sql_exec(
	sqlite3 *db,
	const MBS *sql,
	sqlite3_callback cb
)
{
	MBS *p_err = 0; // SQLiteが使用
	$uRowCnt = 0;
	if(sqlite3_exec(db, sql, cb, NULL, &p_err))
	{
		fprintf(stderr, "[Err] 構\文エラー\n    %s\n    %s\n", p_err, sql);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}
}

/*
	検索結果を１行取得する度に呼ばれるコールバック関数
*/
INT
print_result(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	INT i1 = 0;
	MBS *p1 = 0, *p1e = 0, *p2 = 0, *p3 = 0, *p4 = 0, *p5 = 0;
	INT iColumnCount2 = iColumnCount-1;
	switch(_iExec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// _sOpMd\以下には、_aDir\以下の dir を作成
			i1 = atoi(*(sColumnValues + 0)); // step_cnt
			p1 = U2M(*(sColumnValues + 1));  // dir "\"付
			p1e = ijp_forwardN(p1, i1);      // p1のEOD
			p2 = U2M(*(sColumnValues + 2));  // name
			p3 = U2M(*(sColumnValues + 3));  // path
			// mkdir
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s", _sOpMd, p1e);
				if(imk_dir($sTmp))
				{
					P("md   => %s\n", $sTmp); // 新規dirを表示
					++$uRowCnt;
				}
			// 先
			p4 = ims_clone(p1e);
				snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s%s", _sOpMd, p4, p2);
			p5 = ims_clone($sTmp);
			if(_iExec == I_CP)
			{
				if(CopyFile(p3, p5, FALSE))
				{
					P("cp   <= %s\n", p3); // fileを表示
					++$uRowCnt;
				}
			}
			else if(_iExec >= I_MV)
			{
				// ReadOnly属性(1)を解除
				if((1 & GetFileAttributes(p5)))
				{
					SetFileAttributes(p5, FALSE);
				}
				// 既存file削除
				if(iFchk_typePathA(p5))
				{
					irm_file(p5);
				}
				if(MoveFile(p3, p5))
				{
					P("mv   <= %s\n", p3); // fileを表示
					++$uRowCnt;
				}
				// rmdir
				if(_iExec == I_MV2)
				{
					if(irm_dir(p1))
					{
						P("rd   => %s\n", p1); // dirを表示
						++$uRowCnt;
					}
				}
			}
			ifree(p5);
			ifree(p4);
			ifree(p3);
			ifree(p2);
			ifree(p1);
		break;
		case(I_EXT1): // --extract1
		case(I_EXT2): // --extract2
			p1 = U2M(*(sColumnValues + 0)); // path
			p2 = U2M(*(sColumnValues + 1)); // name
				// mkdir
				if(imk_dir(_sOpMd))
				{
					P("md   => %s\n", _sOpMd); // 新規dirを表示
					++$uRowCnt;
				}
				// 先
				snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s", _sOpMd, p2);
				p3 = ims_clone($sTmp);
					// I_EXT1, I_EXT2共、同名fileは上書き
					if(_iExec == I_EXT1)
					{
						if(CopyFile(p1, p3, FALSE))
						{
							P("ext1 <= %s\n", p3); // fileを表示
							++$uRowCnt;
						}
					}
					else if(_iExec == I_EXT2)
					{
						// ReadOnly属性(1)を解除
						if((1 & GetFileAttributes(p3)))
						{
							SetFileAttributes(p3, FALSE);
						}
						// dir存在していれば削除しておく
						if(iFchk_typePathA(p3))
						{
							irm_file(p3);
						}
						if(MoveFile(p1, p3))
						{
							P("mv   <= %s\n", p3); // fileを表示
							++$uRowCnt;
						}
					}
				ifree(p3);
			ifree(p2);
			ifree(p1);
		break;
		case(I_RP): // --replace
			p1 = U2M(*(sColumnValues + 0)); // type
			p2 = U2M(*(sColumnValues + 1)); // path
				if(*p1 == 'f' && CopyFile(_sOpRep, p2, FALSE))
				{
					P("rep  => %s\n", p2); // fileを表示
					++$uRowCnt;
				}
			ifree(p2);
			ifree(p1);
		break;
		case(I_RM):  // --remove
		case(I_RM2): // --remove2
			p1 = U2M(*(sColumnValues + 0)); // path
			p2 = U2M(*(sColumnValues + 1)); // dir
			// ReadOnly属性(1)を解除
			if((1 & atoi(*(sColumnValues + 2))))
			{
				SetFileAttributes(p1, FALSE);
			}
			// file 削除
			if(irm_file(p1))
			{
				// fileのみ
				P("rm   => %s\n", p1); // fileを表示
				++$uRowCnt;
			}
			// 空dir 削除
			if(_iExec == I_RM2)
			{
				// 空dirである
				if(irm_dir(p2))
				{
					P("rd   => %s\n", p2); // dirを表示
					++$uRowCnt;
				}
			}
			ifree(p2);
			ifree(p1);
		break;
		case(99): // Count Only
			++$uRowCnt;
		break;
		default:
			// タイトル表示
			if(!$uRowCnt)
			{
				iConsole_setTextColor(ColorExp3);
					P20(_sTr1); // 行先頭の付加文字列
					for(i1 = 0; i1 < iColumnCount; i1++)
					{
						p1 = *(sColumnNames + i1);
						p2 = U2M(p1);
							P(
								"%s%s[%s]%s%s",
								_sTd1,
								_sQuote,
								p2,
								_sQuote,
								(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
							);
						ifree(p2);
					}
					P20(_sTr2); // 行末尾の付加文字列
				iConsole_setTextColor(_getColorDefault);
				NL();
			}
			// データ表示
			P20(_sTr1); // 行先頭の付加文字列
			for(i1 = 0; i1 < iColumnCount; i1++)
			{
				// [number]
				if(i1 == _iSelectPosNum)
				{
					P(
						"%s%s%u%s%s",
						_sTd1,
						_sQuote,
						$uRowCnt + 1,
						_sQuote,
						(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
					);
				}
				else
				{
					p2 = U2M(*(sColumnValues + i1));
						P(
							"%s%s%s%s%s",
							_sTd1,
							_sQuote,
							p2,
							_sQuote,
							(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
						);
					ifree(p2);
				}
			}
			P2(_sTr2); // 行末尾の付加文字列
			++$uRowCnt;
		break;
	}
	return SQLITE_OK; // return 0
}

BOOL
sql_saveOrLoadMemdb(
	sqlite3 *mem_db, // ":memory:"
	MBS *ofn,        // filename
	BOOL save_load   // TRUE=save, FALSE=load
)
{
	INT err = 0;
	sqlite3 *pFile;
	sqlite3_backup *pBackup;
	sqlite3 *pTo;
	sqlite3 *pFrom;
	// UTF-8で処理
	U8N *up1 = M2U(ofn);
		if(!(err = sqlite3_open(up1, &pFile)))
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
	ifree(up1);
	return (!err ? TRUE : FALSE);
}

VOID
print_footer()
{
	MBS *p1 = 0;
	iConsole_setTextColor(ColorExp3);
		LN();
		P(
			"-- %u row%s in set ( %.3f sec)\n",
			$uRowCnt,
			($uRowCnt > 1 ? "s" : ""), // 複数形
			iExecSec_next($execMS)
		);
	iConsole_setTextColor(ColorExp1);
		P2("--");
			UINT u1 = 0;
			while(u1 < _aDirNum)
			{
				P("--  _aDir(%u) \"%s\"\n", u1 + 1, *(_aDir + u1));
				++u1;
			}
		P2("--");
			p1 = U2M($sqlU);
				P("--  $sqlU  \"%s\"\n", p1);
			ifree(p1);
		P2("--");
		if(*_sInDbn)
		{
			P("--  -in    \"%s\"\n", _sInDbn);
		}
		if(*_sOutDbn)
		{
			P("--  -out   \"%s\"\n", _sOutDbn);
		}
		if(_bNoGuide)
		{
			P("--  -noguide\n");
		}
		if(*_sQuote)
		{
			P("--  -quote \"%s\"\n", _sQuote);
		}
		if(*_sTable1 || *_sTable2)
		{
			P("--  -table \"%s\" \"%s\"\n", _sTable1, _sTable2);
		}
		if(*_sTr1 || *_sTr2)
		{
			P("--  -tr    \"%s\" \"%s\"\n", _sTr1, _sTr2);
		}
		if(*_sTd1 || *_sTd2)
		{
			P("--  -td    \"%s\" \"%s\"\n", _sTd1, _sTd2);
		}
		if(_bHtml)
		{
			P("--  -html\n");
		}
			P("--  ---priority \"%d\"\n", _iPriority);
		P2("--");
	iConsole_setTextColor(_getColorDefault);
}

VOID
print_version()
{
	LN();
	P("  %s\n",
		IWMFIND_COPYRIGHT
	);
	P("    Ver.%s+%s+SQLite%s\n",
		IWMFIND_VERSION,
		LIB_IWMUTIL_VERSION,
		SQLITE_VERSION
	);
}

VOID
print_help()
{
	iConsole_setTextColor(ColorHeaderFooter);
		print_version();
		LN();
	iConsole_setTextColor(ColorBgText1);
		P (" %s [Dir] [Option] ", $program);
	iConsole_setTextColor(ColorText1);
		P9(2);
	iConsole_setTextColor(ColorText2);
		P2(" (使用例1) 検索 ");
		P ("   %s DIR -r -s \"number, path, size\" -w \"ext like 'exe'\" ", $program);
		P9(2);
		P2(" (使用例2) 検索結果をファイルへ保存 ");
		P ("   %s DIR1 DIR2 ... -r -o FILE [Option] ", $program);
		P9(2);
		P2(" (使用例3) 検索対象をファイルから読込 ");
		P ("   %s -i FILE [Option] ", $program);
	iConsole_setTextColor(ColorText1);
		P9(2);
	iConsole_setTextColor(ColorExp2);
		P2("[Dir]");
	iConsole_setTextColor(ColorText1);
		P2("    検索対象 dir");
		P2("    (注) 複数指定の場合、上位Dirに集約する");
		P ("    (例) \"c:\\\" \"c:\\windows\\\" => \"c:\\\"\n");
		NL();
	iConsole_setTextColor(ColorExp2);
		P2("[Option]");
	iConsole_setTextColor(ColorExp3);
		P2("-out | -o FILE");
	iConsole_setTextColor(ColorText1);
		P2("    出力ファイル");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-in | -i FILE");
	iConsole_setTextColor(ColorText1);
		P2("    入力ファイル");
		P2("    (注) 検索対象 dir と併用できない");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-select | -s \"COLUMN1, ...\"");
	iConsole_setTextColor(ColorText1);
		P2("    number    (NUM) 表\示順");
		P2("    path      (STR) dir\\name");
		P2("    dir       (STR) ディレクトリ名");
		P2("    name      (STR) ファイル名");
		P2("    ext       (STR) 拡張子");
		P2("    depth     (NUM) ディレクトリ階層 = 0..");
		P2("    type      (STR) ディレクトリ = d／ファイル = f");
		P2("    attr_num  (NUM) 属性");
		P2("    attr      (STR) 属性 \"[d|f][r|-][h|-][s|-][a|-]\"");
		P2("                      [dir|file][read-only][hidden][system][archive]");
		P2("    size      (NUM) ファイルサイズ = byte");
		P2("    ctime_cjd (NUM) 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5");
		P2("    ctime     (STR) 作成日時     \"yyyy-mm-dd hh:nn:ss\"");
		P2("    mtime_cjd (NUM) 更新日時     ctime_cjd参照");
		P2("    mtime     (STR) 更新日時     ctime参照");
		P2("    atime_cjd (NUM) アクセス日時 ctime_cjd参照");
		P2("    atime     (STR) アクセス日時 ctime参照");
		NL();
		P2("    ※1 COLUMN指定なしの場合");
		P ("       %s 順で表\示\n", OP_SELECT_1);
		NL();
		P2("    ※2 SQLite演算子／関数を利用可能\");
		P2("       (例)");
		P2("            abs(X)  changes()  char(X1, X2, ..., XN)  coalesce(X, Y, ...)");
		P2("            glob(X, Y)  ifnull(X, Y)  instr(X, Y)  hex(X)");
		P2("            last_insert_rowid()  length(X)");
		P2("            like(X, Y)  like(X, Y, Z)  likelihood(X, Y)  likely(X)");
		P2("            load_extension(X)  load_extension(X, Y)  lower(X)");
		P2("            ltrim(X)  ltrim(X, Y)  max(X, Y, ...)  min(X, Y, ...)");
		P2("            nullif(X, Y)  printf(FORMAT, ...)  quote(X)");
		P2("            random()  randomblob(N)  replace(X, Y, Z)");
		P2("            round(X)  round(X, Y)  rtrim(X)  rtrim(X, Y)");
		P2("            soundex(X)  sqlite_compileoption_get(N)");
		P2("            sqlite_compileoption_used(X)  sqlite_source_id()");
		P2("            sqlite_version()  substr(X, Y, Z) substr(X, Y)");
		P2("            total_changes()  trim(X) trim(X, Y)  typeof(X)  unlikely(X)");
		P2("            unicode(X)  upper(X)  zeroblob(N)");
		P2("       (注) マルチバイト文字を１文字として処理.");
		P2("       (参考) http://www.sqlite.org/lang_corefunc.html");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-where | -w STR");
	iConsole_setTextColor(ColorText1);
		P2("    (例1) \"size<=100 or size>1000000\"");
		P2("    (例2) \"type like 'f' and name like 'abc??.*'\"");
		P2("          '?' '_' は任意の1文字");
		P2("          '*' '%' は任意の0文字以上");
		P2("    (例3) 基準日 \"2010-12-10 12:00:00\"のとき");
		P2("          \"ctime>[-10D]\"   => \"ctime>'2010-11-30 00:00:00'\"");
		P2("          \"ctime>[-10d]\"   => \"ctime>'2010-11-30 12:00:00'\"");
		P2("          \"ctime>[-10d*]\"  => \"ctime>'2010-11-30 %%'\"");
		P2("          \"ctime>[-10d%%]\" => \"ctime>'2010-11-30 %%'\"");
		P2("          (年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-group | -g STR");
	iConsole_setTextColor(ColorText1);
		P2("    (例) -g \"STR1, STR2\"");
		P2("         STR1とSTR2をグループ毎にまとめる");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-sort | -st STR [ASC|DESC]");
	iConsole_setTextColor(ColorText1);
		P2("    (例) -st \"STR1 ASC, STR2 DESC\"");
		P2("         STR1を順ソ\ート, STR2を逆順ソ\ート");
		NL();
	iConsole_setTextColor(ColorExp3);
		P2("-recursive | -r *NUM");
	iConsole_setTextColor(ColorText1);
		P2("    下階層を全検索");
		P2("    オプション *NUM 指定のときは、*NUM 階層まで検索");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-depth | -d NUM1 NUM2");
	iConsole_setTextColor(ColorText1);
		P2("    検索する下階層を指定");
		P2("    (例1) -d \"1\"");
		P2("          1階層のみ検索");
		P2("    (例2) -d \"3\" \"5\"");
		P2("          3～5階層を検索");
		NL();
		P2("    ※1 CurrentDir は \"0\"");
		P2("    ※2 「-depth」と「-where内でのdepth」の挙動の違い");
		P2("      ◇(速い) -depth は指定された階層のみ検索を行う");
		P2("      ◇(遅い) -where内でのdepthによる検索は、全階層のdir／fileに対して行う");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-noguide | -ng");
	iConsole_setTextColor(ColorText1);
		P2("    フッタ情報を表\示しない");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-quote | -qt STR");
	iConsole_setTextColor(ColorText1);
		P2("    囲み文字");
		P2("    (例) -qt \"'\"");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-table STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    ヘッダ(STR1)、フッタ(STR2)を付加");
		P2("    (例1) -table \"<table>\" \"</table>\"");
		P2("    (例2) -table \"\" \"footer\"");
		P2("      文中の改行は\"\\n\"を使用");
		P2("      ※ 先頭の\"-\"は、\"\\-\"と表\記");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-tr STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    行前後に文字を付加");
		P2("    (例1) -tr \"<tr>\" \"</tr>\"");
		P2("    (例2) -tr \",\"");
		P2("    ※ 先頭の\"-\"は、\"\\-\"と表\記");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-td STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    区切り文字／STR2はオプション");
		P2("    (例1) -td \"<td>\" \"</td>\"");
		P2("    (例2) -td \",\"");
		P2("    ※ 先頭の\"-\"は、\"\\-\"と表\記");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-html");
	iConsole_setTextColor(ColorText1);
		P2("    HTML Table出力");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--mkdir | --md DIR");
	iConsole_setTextColor(ColorText1);
		P2("    検索結果からdirを抽出しDIRに作成する(階層維持)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--copy | --cp DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + 検索結果をDIRにコピーする(階層維持)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--move | --mv DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + 検索結果をDIRに移動する(階層維持)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--move2 | --mv2 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + --move + 移動元の空dirを削除する(階層維持)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--extract1 | --ext1 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + 検索結果ファイルのみ抽出しDIRにコピーする");
		P2("    (注) 階層を維持しない／同名ファイルは上書き");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--extract2 | --ext2 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + 検索結果ファイルのみ抽出しDIRに移動する");
		P2("    (注) 階層を維持しない／同名ファイルは上書き");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--remove | --rm");
	iConsole_setTextColor(ColorText1);
		P2("    検索結果の file のみ削除する");
		P2("    ※ dirは削除しない");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--remove2 | --rm2");
	iConsole_setTextColor(ColorText1);
		P2("    --remove + 空dir(subDir除く) を削除する");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--replace | --rep FILE");
	iConsole_setTextColor(ColorText1);
		P2("    検索結果(複数) を FILE(単一名) の内容で置換／ファイル名は変更しない");
		P2("    (例) -w \"name like 'foo.txt'\" --rep \".\\foo.txt\"");
		P2("    ※ 検索結果は上書きされるので注意");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("---priority | ---pr NUM");
	iConsole_setTextColor(ColorText1);
		P2("    実行優先度を設定");
		P2("    (例)NUM 1=優先／2=通常以上／3=通常(初期値)／4=通常以下／5=アイドル時");
	iConsole_setTextColor(ColorHeaderFooter);
		NL();
		LN();
	iConsole_setTextColor(_getColorDefault);
}
