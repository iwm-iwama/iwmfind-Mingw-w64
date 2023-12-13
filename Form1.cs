// プロジェクト～参照の追加
using Microsoft.VisualBasic;

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web;
using System.Windows.Forms;

namespace iwm_Commandliner
{
	public partial class Form1 : Form
	{
		//--------------------------------------------------------------------------------
		// 大域定数
		//--------------------------------------------------------------------------------
		private const string COPYRIGHT = "(C)2018-2023 iwm-iwama";
		private const string VERSION = "iwm_Commandliner4_20231210 'A-29'";

		// タイトル表示の初期値
		private const string TextDefault = "[F4] 説明画面を表示／非表示";

		// 最初に読み込まれる設定ファイル
		private const string ConfigFn = "config.iwmcmd";

		// TextBox 内のテキスト処理(複数行)に使用 ※改行コード長 NL.Length = 2
		private const string NL = "\r\n";

		// "\r\n" "\n" の２パターンに対応
		private const string RgxNL = "\r?\n";

		private readonly object[] AryDgvMacro = {
		//	[マクロ],      [説明],                                     [引数],                                        [使用例],                                   [引数個]
			"#stream",     "出力行毎に処理",                           "(1)コマンド (2)出力 1..5",                    "#stream \"echo #{}\" \"2\"",               2,
			"#streamDL",   "出力行毎にダウンロード",                   "(1)ファイル名 ※拡張子は自動付与",            "#streamDL \"#{line,3}\"",                  1,
			"#stdout",     "直前に出力された stdout",                  "",                                            "#stdout",                                  0,
			"#stderr",     "直前に出力された stderr",                  "",                                            "#stderr",                                  0,
			"#set",        "一時変数 #{:キー} で参照",                 "(1)キー (2)正規表現",                         "#set \"japan\" \"日本\"",                  2,
			"#bd",         "開始時のフォルダに戻る",                   "",                                            "#bd",                                      0,
			"#cd",         "フォルダ変更（存在しないときは新規作成）", "(1)フォルダ名",                               "#cd \"フォルダ名\"",                       1,
			"#cls",        "出力クリア",                               "(1)出力 1..5",                                "#cls \"2\"",                               1,
			"#clear",      "全出力クリア",                             "",                                            "#clear",                                   0,
			"#print",      "印字（改行なし）",                         "(1)正規表現 (2)回数",                         "#print \"#{line,4}\\tDATA\\n\" \"10\"",    2,
			"#puts",       "印字（改行あり）",                         "(1)正規表現 (2)回数",                         "#puts \"#{line,4}\\tDATA\" \"10\"",        2,
			"#copyTo",     "別の出力画面へコピー",                     "(1)出力 n",                                   "#copyTo \"2\"",                            1,
			"#copyFrom",   "別の出力画面からコピー",                   "(1)出力 n",                                   "#copyFrom \"2\"",                          1,
			"#move",       "別の出力画面へ表示切替",                   "(1)出力 n",                                   "#move \"2\"",                              1,
			"#rowBgn",     "この出力画面の行始へ移動",                 "",                                            "#rowBgn",                                  0,
			"#rowEnd",     "この出力画面の行末へ移動",                 "",                                            "#rowEnd",                                  0,
			"#row+",       "別の出力画面から行結合",                   "(1)From 出力 n1,n2,...",                      "#row+ \"2,3\"",                            1,
			"#column+",    "別の出力画面から列結合",                   "(1)From 出力 n1,n2,... (2)結合文字",          "#column+ \"2,3\" \"\\t\"",                 2,
			"#grep",       "検索",                                     "(1)正規表現",                                 "#grep \"\\d{4}\"",                         1,
			"#grep2",      "検索（大小区別）",                         "(1)正規表現",                                 "#grep2 \"\\d{4}\"",                        1,
			"#except",     "不一致検索",                               "(1)正規表現",                                 "#except \"\\d{4}\"",                       1,
			"#except2",    "不一致検索（大小区別）",                   "(1)正規表現",                                 "#except2 \"\\d{4}\"",                      1,
			"#greps",      "合致数検索",                               "(1)正規表現 (2)一致数 n1,n2（以上,以下）",    "#greps \"\\\\\" \"1,4\"",                  2,
			"#greps2",     "合致数検索（大小区別）",                   "(1)正規表現 (2)一致数 n1,n2（以上,以下）",    "#greps2 \"\\\\\" \"1,4\"",                 2,
			"#extract",    "抽出",                                     "(1)正規表現",                                 "#extract \"http\\S+?\\.(jpg|png)\"",       1,
			"#extract2",   "抽出（大小区別）",                         "(1)正規表現",                                 "#extract2 \"http\\S+?\\.(jpg|png)\"",      1,
			"#getRow",     "指定行を抽出",                             "(1)行番号 (2)抽出行数",                       "#getRow \"-5\" \"5\"",                     2,
			"#replace",    "置換",                                     "(1)正規表現 (2)置換文字 $1..$9",              "#replace \"(\\d+)\" \"\\\"$1\\\"\"",       2,
			"#replace2",   "置換（大小区別）",                         "(1)正規表現 (2)置換文字 $1..$9",              "#replace2 \"(\\d+)\" \"\\\"$1\\\"\"",      2,
			"#split",      "分割",                                     "(1)正規表現 (2)分割列 [0]..[n]",              "#split \"\\t\" \"[0]\\t[1]\"",             2,
			"#split2",     "分割（大小区別）",                         "(1)正規表現 (2)分割列 [0]..[n]",              "#split2 \"\\t\" \"[0]\\t[1]\"",            2,
			"#sort",       "ソート（昇順）",                           "",                                            "#sort",                                    0,
			"#reverse",    "ソート（降順）",                           "",                                            "#reverse",                                 0,
			"#uniq",       "重複行をクリア（#sort と併用）",           "",                                            "#uniq",                                    0,
			"#trim",       "文頭文末の空白クリア",                     "",                                            "#trim",                                    0,
			"#rmNL",       "改行をクリア",                             "",                                            "#rmNL",                                    0,
			"#toUpper",    "大文字に変換",                             "",                                            "#toUpper",                                 0,
			"#toLower",    "小文字に変換",                             "",                                            "#toLower",                                 0,
			"#toWide",     "全角に変換",                               "",                                            "#toWide",                                  0,
			"#toZenNum",   "全角に変換(数字のみ)",                     "",                                            "#toZenNum",                                0,
			"#toZenKana",  "全角に変換(カナのみ)",                     "",                                            "#toZenKana",                               0,
			"#toNarrow",   "半角に変換",                               "",                                            "#toNarrow",                                0,
			"#toHanNum",   "半角に変換(数字のみ)",                     "",                                            "#toHanNum",                                0,
			"#toHanKana",  "半角に変換(カナのみ)",                     "",                                            "#toHanKana",                               0,
			"#dfList",     "フォルダ・ファイル一覧",                   "(1)フォルダ名",                               "#dfList \".\"",                            1,
			"#dList",      "フォルダ一覧",                             "(1)フォルダ名",                               "#dList \".\"",                             1,
			"#fList",      "ファイル一覧",                             "(1)フォルダ名",                               "#fList \".\"",                             1,
			"#wread",      "URLからソース取得",                        "(1)URL",                                      "#wread \"https://.../index.html\"",        1,
			"#fread",      "テキストファイル読込",                     "(1)ファイル名",                               "#fread \"ファイル名\"",                    1,
			"#fwrite",     "テキストファイル書込(Shift_JIS)",          "(1)ファイル名",                               "#fwrite \"ファイル名\"",                   1,
			"#fwriteU8",   "テキストファイル書込(UTF-8 BOMあり)",      "(1)ファイル名",                               "#fwriteU8 \"ファイル名\"",                 1,
			"#fwriteU8N",  "テキストファイル書込(UTF-8 BOMなし)",      "(1)ファイル名",                               "#fwriteU8N \"ファイル名\"",                1,
			"#frename",    "出力行毎のファイル名を置換",               "(1)正規表現 (2)置換文字 $1..$9",              "#frename \"(.+)(\\..+)\" \"#{line,3}$2\"", 2,
			"#fgrep",      "出力行毎のファイル内を検索",               "(1)正規表現",                                 "#fgrep \"(dir|file)\"",                    1,
			"#fgrep2",     "出力行毎のファイル内を検索（大小区別）",   "(1)正規表現",                                 "#fgrep2 \"(dir|file)\"",                   1,
			"#fsetCtime",  "ファイル作成日時変更",                     "(1)日時 \"Now\"=現在時 (2)遅延秒",            "#fsetCtime \"2023/09/15 10:00:00\" \"0\"", 2,
			"#fsetMtime",  "ファイル更新日時変更",                     "(1)日時 \"Now\"=現在時 (2)遅延秒",            "#fsetMtime \"2023/09/15 20:00:00\" \"1\"", 2,
			"#fsetAtime",  "ファイルアクセス日時変更",                 "(1)日時 \"Now\"=現在時 (2)遅延秒",            "#fsetAtime \"Now\"",                       2,
			"#fgetRow",    "ファイルから指定行を抽出",                 "(1)行番号 (2)抽出行数 (3)出力 1..5",          "#fgetRow \"-5\" \"5\" \"2\"",              3,
			"#pos",        "フォーム位置",                             "(1)横位置 X (2)縦位置 Y",                     "#pos \"50%\" \"100\"",                     2,
			"#posCenter",  "フォーム位置（中央）",                     "",                                            "#posCenter",                               0,
			"#size",       "フォームサイズ",                           "(1)幅 Width (2)高さ Height",                  "#size \"50%\" \"600\"",                    2,
			"#sizeMax",    "フォーム最大化",                           "",                                            "#sizeMax",                                 0,
			"#sizeMin",    "フォーム最小化",                           "",                                            "#sizeMin",                                 0,
			"#sizeNormal", "元のフォームサイズ",                       "",                                            "#sizeNormal",                              0,
			"#tabSize",    "タブサイズを変更",                         "(1)数字",                                     "#tabSize \"8\"",                           1,
			"#fontSize",   "フォントサイズを変更",                     "(1)数字",                                     "#fontSize \"10\"",                         1,
			"#bgColor",    "背景色を変更",                             "(1)色名",                                     "#bgColor \"BLACK\"",                       1,
			"#color?",     "背景色",                                   "",                                            "#color?",                                  0,
			"#macro?",     "マクロ／マクロ変数",                       "",                                            "#macro?",                                  0,
			"#help",       "ヘルプ（操作説明）",                       "",                                            "#help",                                    0,
			"#version",    "バージョン",                               "",                                            "#version",                                 0,
			"#exit",       "終了",                                     "",                                            "#exit",                                    0
		};

		private readonly object[] AryDgvMacroVar = {
			"[#stream専用]",     "",
			"#{}",               "出力を１行ずつ処理",
			"#{1..5}",           "↑ 出力1..5 を明示",
			"",                    "",
			"[汎用]",              "",
			"#{line,NUM1,NUM2}", "行番号を表示 NUM1=桁数 NUM2=加算数",
			"#{&NUM}",           "ASCIIコードを文字に変換 (例) #{&9} => \\t",
			"#{:STR}",           "一時変数 #setマクロ参照",
			"#{:now}",           "yyyyMMdd_HHmmss_fff",
			"#{:ymd}",           "yyyyMMdd",
			"#{:hns}",           "HHmmss",
			"#{:msec}",          "fff",
			"#{:y}",             "yyyy",
			"#{:m}",             "MM",
			"#{:d}",             "dd",
			"#{:h}",             "HH",
			"#{:n}",             "mm",
			"#{:s}",             "ss",
			"#{%STR}",           "環境変数 (例) #{%PATH}"
		};

		//--------------------------------------------------------------------------------
		// 大域変数
		//--------------------------------------------------------------------------------
		// エラーが発生したとき
		private bool ExecStopOn = false;

		// Stdout, Stderr
		private readonly StringBuilder SbStdOut = new StringBuilder();
		private readonly StringBuilder SbStdErr = new StringBuilder();

		// BaseDir
		private string BaseDir = "";

		// Start Control Name
		private Control TmpCtrlName = null;

		// Object
		private Process PS = null;

		// HttpClient
		private HttpClient HttpClient = null;

		// 一時変数
		private readonly SortedDictionary<string, int> DictMacroOptCnt = new SortedDictionary<string, int>();
		private readonly SortedDictionary<string, string> DictTmpVar = new SortedDictionary<string, string>();

		// SendMessage メソッド
		[DllImport("User32.dll")]
		private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, int wParam, int[] lParam);

		// タブストップ定数
		private const int EM_SETTABSTOPS = 0x00CB;

		//--------------------------------------------------------------------------------
		// TbCmdHelp
		//--------------------------------------------------------------------------------
		private const string TbCmdHelp =
			"--------------------------" + NL +
			"> 特殊ショートカットキー <" + NL +
			"--------------------------" + NL +
			"[F4]で説明画面を表示／非表示します。" + NL +
			 NL +
			"--------------------------" + NL +
			"> 標準ショートカットキー <" + NL +
			"--------------------------" + NL +
			"[Ctrl]+[A]    全て選択" + NL +
			"[Ctrl]+[C]    コピー" + NL +
			"[Ctrl]+[V]    ペースト" + NL +
			"[Ctrl]+[X]    カット" + NL +
			"[Ctrl]+[Z]    アンドゥ" + NL +
			NL +
			"-----------------------------------" + NL +
			"> 正規表現における \\ 表記について <" + NL +
			"-----------------------------------" + NL +
			"  \\t  =>  \\t" + NL +
			"  \\\"  =>  \"" + NL +
			"  \\;  =>  ;" + NL +
			"  \\\\  =>  \\" + NL +
			NL +
			"--------------" + NL +
			"> マクロ入力 <" + NL +
			"--------------" + NL +
			"(例１) #cd \"..\"" + NL +
			"           ↑引数はダブルクォーテーションで囲む。" + NL +
			NL +
			"(例２) #puts \"\\\"ダブルクォーテーション\\\"\" \"5\"" + NL +
			"             ↑引数内にダブルクォーテーションを記述するときは \\\" を使用する。" + NL +
			NL +
			"----------------" + NL +
			"> コマンド入力 <" + NL +
			"----------------" + NL +
			"(例１) dir /b .." + NL +
			NL +
			"(例２) dir \"C:/Program Files\"" + NL +
			"           ↑引数内に空白があるときはダブルクォーテーションで囲む。" + NL +
			NL +
			"--------------------------------" + NL +
			"> 複数のマクロ／コマンドを入力 <" + NL +
			"--------------------------------" + NL +
			"(例) #cls; dir; #grep \"^20\"; #replace \"^20(\\d+)\" \"'$1\"" + NL +
			"         ↑セミコロンで区切る。" + NL +
			NL +
			"----------------" + NL +
			"> 設定ファイル <" + NL +
			"----------------" + NL +
			"設定ファイル名は " + ConfigFn + " とします。" + NL +
			"実行ファイルと同じフォルダにある設定ファイルを起動時に読み込みます。" + NL +
			NL +
			"コメント行" + NL +
			"  ・（単一行コメント）文頭が // の行" + NL +
			"  ・（複数行コメント）文頭 /* から 文頭 */ で囲まれた行" + NL +
			NL +
			"(例)" + NL +
			"  // 単一行コメント" + NL +
			"  /*" + NL +
			"     複数行コメント" + NL +
			"  */" + NL +
			"  #cls" + NL +
			"  dir" + NL +
			"  #grep \"^20\"" + NL +
			"  #replace \"^20(\\d+)\" \"'$1\"" + NL
		;

		//--------------------------------------------------------------------------------
		// LblTooltip
		//--------------------------------------------------------------------------------
		private readonly string LblTooltip_TbCmd =
			"[Shift]+[↑／↓]     実行履歴（過去／最近）" + NL +
			NL +
			"[Ctrl]+[U]           クリア" + NL +
			"[Ctrl]+[Back]        カーソルより前をクリア" + NL +
			"[Ctrl]+[Delete]      カーソルより後をクリア" + NL +
			NL +
			"[Ctrl]+[↑／↓]      コンテキストメニューを表示" + NL +
			"[Esc]                コンテキストメニューを閉じる" + NL +
			NL +
			"[Ait]+[←／→]       スペース直後にカーソル移動" + NL +
			NL +
			"[F1]                 実行履歴" + NL +
			"[F2]                 マクロ選択" + NL +
			"[F3]                 コマンド選択" + NL +
			"[F4]                 説明画面を表示／非表示" + NL +
			"[F5]                 実行" + NL +
			"[F6]                 出力を実行前に戻す" + NL +
			"[F7]                 出力をクリア" + NL +
			"[F8]                 出力履歴" + NL +
			"[F9]                 フォーカス移動" + NL +
			"[F10]                （メニュー）" + NL +
			"[F11]                出力変更（前へ）" + NL +
			"[F12]                出力変更（次へ）" + NL +
			NL +
			"[PgUp／PgDn]         出力をスクロール" + NL
		;

		private readonly string LblTooltip_TbDgvSearch =
			"[Shift]+[↓／↑]     マクロ選択／コマンド選択へ移動／戻る" + NL +
			"[Esc]                マクロ選択／コマンド選択を閉じる" + NL +
			NL +
			"[Ctrl]+[U]           クリア" + NL +
			"[Ctrl]+[Back]        カーソルより前をクリア" + NL +
			"[Ctrl]+[Delete]      カーソルより後をクリア" + NL +
			NL +
			"[F4]                 説明画面を表示／非表示" + NL +
			NL +
			"[PgUp／PgDn]         マクロ選択／コマンド選択をスクロール" + NL
		;

		private readonly string LblTooltip_TbResult =
			"[Ctrl]+[Back]        カーソルより前をクリア" + NL +
			"[Ctrl]+[Delete]      カーソルより後をクリア" + NL +
			NL +
			"[F4]                 説明画面を表示／非表示" + NL +
			"[F8]                 出力履歴" + NL +
			"[F9]                 フォーカス移動" + NL +
			"[F11]                出力変更（前へ）" + NL +
			"[F12]                出力変更（次へ）" + NL
		;

		private void SubLblTooltip(string str = "")
		{
			LblTooltip.Text = str;
			LblTooltip.AutoSize = true;
			LblTooltip.Location = new Point((Width - LblTooltip.Width) / 2, (Height - LblTooltip.Height) / 2);
			LblTooltip.BringToFront();
			GblLblTooltipVisible = LblTooltip.Visible = true;
		}

		private void LblTooltip_Click(object sender, EventArgs e)
		{
			GblLblTooltipVisible = LblTooltip.Visible = false;
		}

		//--------------------------------------------------------------------------------
		// Form
		//--------------------------------------------------------------------------------
		public Form1()
		{
			InitializeComponent();

			// MouseWhell イベント追加
			TbCmd.MouseWheel += new MouseEventHandler(TbCmd_MouseWheel);
			NudTabSize.MouseWheel += new MouseEventHandler(NudTabSize_MouseWheel);
			NudFontSize.MouseWheel += new MouseEventHandler(NudFontSize_MouseWheel);
		}

		private void Form1_Load(object sender, EventArgs e)
		{
			Opacity = 0.0;

			// タイトル表示
			Text = TextDefault;

			// 表示位置
			StartPosition = FormStartPosition.Manual;
			Location = new Point(Cursor.Position.X - (Width / 2), Cursor.Position.Y - (SystemInformation.CaptionHeight / 2));

			// TbCurDir
			BaseDir = TbCurDir.Text = Directory.GetCurrentDirectory();
			Directory.SetCurrentDirectory(BaseDir);

			// TbCmd
			TbCmd.Text = "";

			// DgvMacro
			// DictMacroOptCnt
			for (int _i1 = 0; _i1 < AryDgvMacro.Length; _i1 += 5)
			{
				_ = DgvMacro.Rows.Add(AryDgvMacro[_i1].ToString(), AryDgvMacro[_i1 + 1].ToString(), AryDgvMacro[_i1 + 2].ToString(), AryDgvMacro[_i1 + 3].ToString());
				DictMacroOptCnt[AryDgvMacro[_i1].ToString().ToLower()] = (int)AryDgvMacro[_i1 + 4];
			}

			// DgvCmd
			SubDgvCmdLoad();

			// TbResult
			SubTbResultChange(1, TbCmd);

			// ScrTbResult
			ScrTbResult.Visible = false;

			// フォントサイズ
			NudFontSize.Value = (int)Math.Round(TbResult.Font.Size);

			// 設定ファイルが存在するとき
			string s1 = "";

			// 作業フォルダにある？
			if (File.Exists(ConfigFn))
			{
				s1 = ConfigFn;
			}
			// 実行ファイルと同じフォルダにある？
			else
			{
				string _s1 = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + "\\" + ConfigFn;
				if (File.Exists(_s1))
				{
					s1 = _s1;
				}
			}

			if (s1.Length > 0)
			{
				TbCmd.Text = RtnTbCmdFreadToFormat(s1);
				BtnCmdExec_Click(sender, e);
				TbCmd.Text = "";
			}

			// コマンドライン引数によるバッチ処理
			if (ARGS.Length > 0)
			{
				TbCmd.Text = "";
				foreach (string _s1 in ARGS)
				{
					if (File.Exists(_s1))
					{
						TbCmd.Text += RtnTbCmdFreadToFormat(_s1);
					}
				}
				BtnCmdExec_Click(sender, e);
				TbCmd.Text = "";
			}

			// 開始時フォーカス
			SubTbCmdFocus(-1);

			// TopMost
			ChkTopMost.Checked = TopMost = true;

			// ちらつき防止
			Type CtrlType;
			PropertyInfo CtrPropertyInfo;
			CtrlType = typeof(DataGridView);
			CtrPropertyInfo = CtrlType.GetProperty("DoubleBuffered", BindingFlags.Instance | BindingFlags.NonPublic);
			CtrPropertyInfo.SetValue(DgvMacro, true, null);
			CtrPropertyInfo.SetValue(DgvCmd, true, null);

			Opacity = 1.0;
		}

		private void Form1_Resize(object sender, EventArgs e)
		{
			if (GblDgvMacroOpen)
			{
				GblDgvMacroOpen = false;
				BtnDgvMacro_Click(sender, e);
			}
			else if (GblDgvCmdOpen)
			{
				GblDgvCmdOpen = false;
				BtnDgvCmd_Click(sender, e);
			}

			if (GblLblTooltipVisible)
			{
				SubLblTooltip(null);
			}
		}

		//--------------------------------------------------------------------------------
		// TbCurDir
		//--------------------------------------------------------------------------------
		private void TbCurDir_Click(object sender, EventArgs e)
		{
			LblCurDir.Visible = true;

			FolderBrowserDialog fbd = new FolderBrowserDialog
			{
				Description = "フォルダを指定してください。",
				RootFolder = Environment.SpecialFolder.MyComputer,
				SelectedPath = Environment.CurrentDirectory,
				ShowNewFolderButton = true
			};

			if (fbd.ShowDialog(this) == DialogResult.OK)
			{
				TbCurDir.Text = fbd.SelectedPath;
				Directory.SetCurrentDirectory(TbCurDir.Text);
			}

			LblCurDir.Visible = false;
			SubTbCmdFocus(-1);
		}

		private void TbCurDir_TextChanged(object sender, EventArgs e)
		{
			ToolTip.SetToolTip(TbCurDir, TbCurDir.Text.Replace("\\", NL));
		}

		private void TbCurDir_DragEnter(object sender, DragEventArgs e)
		{
			e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
		}

		private void TbCurDir_DragDrop(object sender, DragEventArgs e)
		{
			string s1 = ((string[])e.Data.GetData(DataFormats.FileDrop))[0];
			if (Directory.Exists(s1))
			{
				Directory.SetCurrentDirectory(s1);
				TbCurDir.Text = s1;
			}
		}

		//--------------------------------------------------------------------------------
		// CmsTbCurDir
		//--------------------------------------------------------------------------------
		private void CmsTbCurDir_全コピー_Click(object sender, EventArgs e)
		{
			TbCurDir.SelectAll();
			TbCurDir.Copy();
		}

		//--------------------------------------------------------------------------------
		// ChkTopMost
		//--------------------------------------------------------------------------------
		private void ChkTopMost_Click(object sender, EventArgs e)
		{
			TopMost = ChkTopMost.Checked;
		}

		//--------------------------------------------------------------------------------
		// TbCmd
		//--------------------------------------------------------------------------------
		//   RichTextBox化すると操作のたび警告音が発生し、やかましくてしょうがない!!
		//   正攻法での解決策を見出せなかったので、TextBoxでの実装にとどめることにした。
		//--------------------------------------------------------------------------------
		private int GblTbCmdPos = 0;
		private bool GblLblTooltipVisible = false;

		private void TbCmd_Enter(object sender, EventArgs e)
		{
			TmpCtrlName = TbCmd;
			Text = TextDefault;

			LblCmd.Visible = true;
			Lbl_F1.ForeColor = Lbl_F2.ForeColor = Lbl_F3.ForeColor = Lbl_F5.ForeColor = Lbl_F6.ForeColor = Lbl_F7.ForeColor = Lbl_F8.ForeColor = Color.WhiteSmoke;
			if (GblLblTooltipVisible)
			{
				SubLblTooltip(LblTooltip_TbCmd);
			}
		}

		private void TbCmd_Leave(object sender, EventArgs e)
		{
			LblCmd.Visible = false;
			Lbl_F1.ForeColor = Lbl_F2.ForeColor = Lbl_F3.ForeColor = Lbl_F5.ForeColor = Lbl_F6.ForeColor = Lbl_F7.ForeColor = Lbl_F8.ForeColor = Color.Gray;
			GblTbCmdPos = TbCmd.SelectionStart;

			GblLblTooltipVisible = LblTooltip.Visible;
			LblTooltip.Visible = false;
		}

		private void TbCmd_TextChanged(object sender, EventArgs e)
		{
			// IME確定 [Enter] で本イベントは発生しない(＝改行されない)ので "\n" の有無で入力モードの判定可能
			if (TbCmd.Text.IndexOf('\n') >= 0)
			{
				TbCmd.Text = Regex.Replace(TbCmd.Text, RgxNL, "");
			}
		}

		private void TbCmd_KeyDown(object sender, KeyEventArgs e)
		{
			MatchCollection MC;
			int iPos;

			// [→]
			if (e.KeyData == Keys.Right)
			{
				if (TbCmd.SelectionStart == TbCmd.TextLength)
				{
					TbCmd.AppendText(" ");
				}
				return;
			}

			// [Alt]+[←]
			if (e.KeyData == (Keys.Alt | Keys.Left))
			{
				MC = Regex.Matches(TbCmd.Text.Substring(0, TbCmd.SelectionStart), "\\S+\\s*$");
				TbCmd.SelectionStart = MC.Count > 0 ? MC[0].Index : 0;
				TbCmd.Select(TbCmd.SelectionStart, 1);
				return;
			}

			// [Alt]+[→]
			if (e.KeyData == (Keys.Alt | Keys.Right))
			{
				iPos = TbCmd.SelectionStart;
				MC = Regex.Matches(TbCmd.Text.Substring(iPos), "\\s\\S+");
				TbCmd.SelectionStart = MC.Count > 0 ? iPos + 1 + MC[0].Index : TbCmd.TextLength;
				TbCmd.Select(TbCmd.SelectionStart, 1);
				return;
			}

			// [↑] [↓] [PgUp] [PgDn] は日本語入力時にバッティングするので使用不可
			// [Shift]+[↑]
			if (e.KeyData == (Keys.Shift | Keys.Up))
			{
				SubTbCmdHistory_Get(true);
				return;
			}

			// [Shift]+[↓]
			if (e.KeyData == (Keys.Shift | Keys.Down))
			{
				SubTbCmdHistory_Get(false);
				return;
			}

			// [Ctrl]+[Back]
			if (e.KeyData == (Keys.Control | Keys.Back))
			{
				TbCmd.Select(0, TbCmd.SelectionStart);
				TbCmd.Cut();
				return;
			}

			// [Ctrl]+[Delete]
			if (e.KeyData == (Keys.Control | Keys.Delete))
			{
				TbCmd.Select(TbCmd.SelectionStart, TbCmd.TextLength);
				TbCmd.Cut();
				return;
			}

			// [Ctrl]+[A]
			if (e.KeyData == (Keys.Control | Keys.A))
			{
				TbCmd.SelectAll();
				return;
			}

			// [Ctrl]+[C]
			if (e.KeyData == (Keys.Control | Keys.C))
			{
				TbCmd.Copy();
				return;
			}

			// [Ctrl]+[S]
			if (e.KeyData == (Keys.Control | Keys.S))
			{
				Cursor.Position = new Point(Left + ((Width - CmsCmd.Width) / 2), Top + SystemInformation.CaptionHeight + TbCmd.Bottom - 20);
				CmsCmd.Show(Cursor.Position);
				CmsCmd_履歴を保存.Select();
				return;
			}

			// [Ctrl]+[U]
			if (e.KeyData == (Keys.Control | Keys.U))
			{
				TbCmd.Text = "";
				return;
			}

			// [Ctrl]+[V]
			if (e.KeyData == (Keys.Control | Keys.V))
			{
				TbCmd.Paste(Regex.Replace(Clipboard.GetText().TrimEnd(), RgxNL, ";"));
				return;
			}

			// [Ctrl]+[X]
			if (e.KeyData == (Keys.Control | Keys.X))
			{
				TbCmd.Cut();
				return;
			}

			// [Ctrl]+[Z]
			if (e.KeyData == (Keys.Control | Keys.Z))
			{
				if (TbCmd.CanUndo)
				{
					TbCmd.Undo();
					TbCmd.ClearUndo();
				}
				return;
			}

			// [Ctrl]+[↑／↓]
			if (e.KeyData == (Keys.Control | Keys.Up) || e.KeyData == (Keys.Control | Keys.Down))
			{
				Cursor.Position = new Point(Left + ((Width - CmsCmd.Width) / 2), Top + SystemInformation.CaptionHeight + TbCmd.Bottom - 20);
				CmsCmd.Show(Cursor.Position);
				CmsCmd_マクロ変数.Select();
				return;
			}
		}

		private void TbCmd_KeyPress(object sender, KeyPressEventArgs e)
		{
			// ビープ音抑制
			//   [Ctrl] で縛りをかけると [Ctrl]+[キー] を自前で追記する必要がある
			if ((ModifierKeys & Keys.Control) == Keys.Control)
			{
				e.Handled = true;
			}

			// IME入力対策
			//   TextChanged と処理を分担しないとIME操作時に不具合が発生する
			if (e.KeyChar == (char)Keys.Enter)
			{
				BtnCmdExec_Click(sender, null);
			}
		}

		private void TbCmd_KeyUp(object sender, KeyEventArgs e)
		{
			GblTbCmdPos = TbCmd.SelectionStart;

			switch (e.KeyCode)
			{
				case Keys.F1:
					CbCmdHistory.DroppedDown = true;
					_ = CbCmdHistory.Focus();
					return;

				case Keys.F2:
					BtnDgvMacro_Click(sender, e);
					return;

				case Keys.F3:
					BtnDgvCmd_Click(sender, e);
					return;

				case Keys.F4:
					if (GblLblTooltipVisible)
					{
						LblTooltip.Visible = GblLblTooltipVisible = false;
					}
					else
					{
						SubLblTooltip(LblTooltip_TbCmd);
					}
					return;

				case Keys.F5:
					BtnCmdExec_Click(sender, e);
					return;

				case Keys.F6:
					BtnCmdExecUndo_Click(sender, e);
					return;

				case Keys.F7:
					BtnClear_Click(sender, e);
					return;

				case Keys.F8:
					CbResultHistory.DroppedDown = true;
					_ = CbResultHistory.Focus();
					return;

				case Keys.F9:
					_ = TbResult.Focus();
					return;

				case Keys.F10: // システムメニュー表示
					SendKeys.Send("{UP}");
					return;

				case Keys.F11:
					SubTbResultForward();
					return;

				case Keys.F12:
					SubTbResultNext();
					return;

				case Keys.Enter:
					// 実行は KeyPress
					// ここでは何もしない
					return;

				// [Ctrl], [Shift], [Alt] と方向キーの組み合わせは弊害が多いので使用しない方向で
				case Keys.PageUp:
					SendKeys.Send("{F9}{PGUP}{F9}");
					return;

				case Keys.PageDown:
					SendKeys.Send("{F9}{PGDN}{F9}");
					return;
			}

			// 補完(*)
			if (TbCmd.TextLength == TbCmd.SelectionStart && e.KeyData != Keys.Delete && e.KeyData != Keys.Back)
			{
				// 引数分の "" を付与
				foreach (Match _m1 in new Regex(".*(?<v1>#\\S+?)\\s+$", RegexOptions.IgnoreCase).Matches(TbCmd.Text))
				{
					string s1 = _m1.Groups["v1"].Value.ToLower();
					if (DictMacroOptCnt.ContainsKey(s1))
					{
						TbCmd.Text = TbCmd.Text.Trim();
						int iSpace = DictMacroOptCnt[s1];
						for (int _i2 = 0; _i2 < iSpace; _i2++)
						{
							TbCmd.Text += " \"\"";
						}
						// カーソル位置
						TbCmd.SelectionStart = TbCmd.TextLength - (iSpace * 3) + 2;
					}
				}
				TbCmd.ForeColor = Color.Black;
			}
			else
			{
				TbCmd.ForeColor = Color.Red;
			}
			TbCmd.ScrollToCaret();
		}

		private void TbCmd_MouseDown(object sender, MouseEventArgs e)
		{
			_ = TbCmd.Focus();

			switch (e.Button)
			{
				case MouseButtons.Right:
					TbCmd.ContextMenuStrip = TbCmd.SelectedText.Length > 0 ? CmsTextSelect : CmsCmd;
					return;
			}
		}

		private void TbCmd_MouseUp(object sender, MouseEventArgs e)
		{
			switch (e.Button)
			{
				case MouseButtons.Left:
					SubCmsTextSelect_Opened();
					return;

				case MouseButtons.Right:
					SubCmsTextSelect_Opened();
					return;
			}
		}

		private void TbCmd_MouseWheel(object sender, MouseEventArgs e)
		{
			// 上回転 +120
			if (e.Delta > 0)
			{
				SubTbCmdHistory_Get(true);
			}
			// 下回転 -120
			else if (e.Delta < 0)
			{
				SubTbCmdHistory_Get(false);
			}
		}

		private void TbCmd_DragEnter(object sender, DragEventArgs e)
		{
			e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
		}

		private void TbCmd_DragDrop(object sender, DragEventArgs e)
		{
			string s1 = "";
			foreach (string _s1 in (string[])e.Data.GetData(DataFormats.FileDrop))
			{
				s1 += $"{_s1.TrimEnd('\\')} ";
			}
			TbCmd.Paste(s1.TrimEnd());
		}

		//--------------------------------------------------------------------------------
		// CmsCmd
		//--------------------------------------------------------------------------------
		private void CmsCmd_実行_Click(object sender, EventArgs e)
		{
			BtnCmdExec_Click(sender, e);
		}

		private void CmsCmd_全クリア_Click(object sender, EventArgs e)
		{
			TbCmd.Text = "";
		}

		private void CmsCmd_全コピー_Click(object sender, EventArgs e)
		{
			TbCmd.SelectAll();
			TbCmd.Copy();
		}

		private void CmsCmd_全カット_Click(object sender, EventArgs e)
		{
			TbCmd.SelectAll();
			TbCmd.Cut();
		}

		private void CmsCmd_ペースト_Click(object sender, EventArgs e)
		{
			TbCmd.Paste(Regex.Replace(Clipboard.GetText().TrimEnd(), RgxNL, ";"));
		}

		private void CmsCmd_ペースト_MouseHover(object sender, EventArgs e)
		{
			ToolTip.SetToolTip(CmsCmd, RtnClipboardSubstring());
		}

		private string RtnClipboardSubstring(int iStrMax = 500)
		{
			string rtn = Clipboard.GetText();
			int i1 = rtn.Length;
			if (i1 > iStrMax)
			{
				return rtn.Substring(0, iStrMax) + NL + "...";
			}
			return rtn;
		}

		private void CmsCmd_カーソルの前方をクリア_Click(object sender, EventArgs e)
		{
			TbCmd.Select(0, TbCmd.SelectionStart);
			TbCmd.SelectedText = "";
		}

		private void CmsCmd_カーソルの後方をクリア_Click(object sender, EventArgs e)
		{
			TbCmd.Select(TbCmd.SelectionStart, TbCmd.TextLength - TbCmd.SelectionStart);
			TbCmd.SelectedText = "";
		}

		private void CmsCmd_マクロ変数_Click(object sender, EventArgs e)
		{
			Cursor.Position = new Point(Left + ((Width - CmsCmd.Width) / 2), Top + SystemInformation.CaptionHeight + TbCmd.Bottom - 20);
			CmsCmd2.Show(Cursor.Position);
		}

		private void CmsCmd_フォルダ選択_Click(object sender, EventArgs e)
		{
			FolderBrowserDialog fbd = new FolderBrowserDialog
			{
				Description = "フォルダを指定してください。",
				RootFolder = Environment.SpecialFolder.MyComputer,
				SelectedPath = Environment.CurrentDirectory,
				ShowNewFolderButton = true
			};

			if (fbd.ShowDialog(this) == DialogResult.OK)
			{
				TbCmd.Paste($"{fbd.SelectedPath.TrimEnd('\\')}");
			}
		}

		private void CmsCmd_ファイル選択_Click(object sender, EventArgs e)
		{
			OpenFileDialog ofd = new OpenFileDialog
			{
				InitialDirectory = Environment.CurrentDirectory,
				Multiselect = true
			};

			if (ofd.ShowDialog() == DialogResult.OK)
			{
				string s1 = "";
				foreach (string _s1 in ofd.FileNames)
				{
					s1 += $"{_s1} ";
				}
				TbCmd.Paste(s1.TrimEnd());
			}
		}

		private string GblCmsCmdFn = "";

		private void CmsCmd_履歴を保存_Click(object sender, EventArgs e)
		{
			// 読込時フォーマット RtnTbCmdFreadToFormat() に対応
			string s1 = string.Join(NL, GblLstTbCmdHistory).Replace("\\;", "\a");
			s1 = Regex.Replace(s1, "(/\\*|\\*/)+\\s*", "$1" + NL);
			s1 = Regex.Replace(s1, "[;]+\\s*", NL);
			s1 = s1.Replace("\a", "\\;").Trim() + NL;
			string fn = (GblCmsCmdFn.Length == 0 ? "" : Path.GetDirectoryName(GblCmsCmdFn) + "\\") + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".iwmcmd";
			if (RtnTextFileWrite(s1, "65001nobom", fn, true, CMD_FILTER))
			{
				GblCmsCmdFn = fn;
			}
		}

		private void CmsCmd_履歴を読込_Click(object sender, EventArgs e)
		{
			CmsCmd.Close();

			(string fn, _) = RtnTextFread(GblCmsCmdFn, true, CMD_FILTER);
			if (fn.Length > 0)
			{
				GblCmsCmdFn = fn;
				CmsCmd_履歴を読込_再読込_Click(sender, e);
				CmsCmd_履歴を読込_再読込.ToolTipText = GblCmsCmdFn.Replace("\\", NL);
			}
		}

		private void CmsCmd_履歴を読込_再読込_Click(object sender, EventArgs e)
		{
			if (GblCmsCmdFn.Length > 0)
			{
				TbCmd.Text = RtnTbCmdFreadToFormat(GblCmsCmdFn);
				SubTbCmdFocus(-1);
			}
			else
			{
				CmsCmd_履歴を読込_Click(sender, e);
			}
		}

		private void CmsCmd2_Opened(object sender, EventArgs e)
		{
			_ = CmsCmd2.Focus();
			CmsCmd2_一時変数.Enabled = false;

			// #{:[キー]} のメニュー作成
			if (DictTmpVar.Count > 0)
			{
				CmsCmd2_一時変数.DropDownItems.Clear();

				foreach (KeyValuePair<string, string> _kv1 in DictTmpVar)
				{
					ToolStripMenuItem _tsi = new ToolStripMenuItem
					{
						Text = $"#{{:{_kv1.Key}}} {_kv1.Value}"
					};
					_tsi.Click += CmsCmd2_一時変数_SubMenuClick;
					_ = CmsCmd2_一時変数.DropDownItems.Add(_tsi);
				}

				CmsCmd2_一時変数.Enabled = true;
			}

			// #stream のときだけ使える変数
			CmsCmd2_出力タブの行データ.Enabled = TbCmd.Text.ToLower().Contains("#stream");

			CmsCmd2_閉じる.Select();
		}

		private void CmsCmd2_一時変数_SubMenuClick(object sender, EventArgs e)
		{
			ToolStripMenuItem tsmi = (ToolStripMenuItem)sender;
			TbCmd.Paste(Regex.Replace(tsmi.Text, "^(#\\{:.+?\\}).+", "$1"));
			_ = CmsCmd2.Focus();
			CmsCmd2_一時変数.Select();
		}

		private bool GblCmsCmd2CancelOn = true;

		private void CmsCmd2_Opening(object sender, System.ComponentModel.CancelEventArgs e)
		{
			GblCmsCmd2CancelOn = true;
		}

		private void CmsCmd2_Closing(object sender, ToolStripDropDownClosingEventArgs e)
		{
			e.Cancel = GblCmsCmd2CancelOn;
		}

		private void CmsCmd2_PreviewKeyDown(object sender, PreviewKeyDownEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.Escape:
					GblCmsCmd2CancelOn = false;
					CmsCmd2.Close();
					return;
			}
		}

		private void CmsCmd2_閉じる_Click(object sender, EventArgs e)
		{
			GblCmsCmd2CancelOn = false;
			CmsCmd2.Close();
		}

		private void CmsCmd2_現時間_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:now}");
		}

		private void CmsCmd2_日付_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:ymd}");
		}

		private void CmsCmd2_年_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:y}");
		}

		private void CmsCmd2_月_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:m}");
		}

		private void CmsCmd2_日_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:d}");
		}

		private void CmsCmd2_時間_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:hns}");
		}

		private void CmsCmd2_時_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:h}");
		}

		private void CmsCmd2_分_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:n}");
		}

		private void CmsCmd2_秒_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:s}");
		}

		private void CmsCmd2_ミリ秒_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{:msec}");
		}

		private void CmsCmd2_出力タブの行データ_カレント_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{}");
		}

		private void CmsCmd2_出力タブの行データ_1_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{1}");
		}

		private void CmsCmd2_出力タブの行データ_2_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{2}");
		}

		private void CmsCmd2_出力タブの行データ_3_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{3}");
		}

		private void CmsCmd2_出力タブの行データ_4_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{4}");
		}

		private void CmsCmd2_出力タブの行データ_5_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{5}");
		}

		private void CmsCmd2_出力の行番号_Click(object sender, EventArgs e)
		{
			TbCmd.Paste("#{line,,}");
		}

		//--------------------------------------------------------------------------------
		// LblDropScript
		//--------------------------------------------------------------------------------
		private void LblDropScript_DragDrop(object sender, DragEventArgs e)
		{
			GblCmsCmdFn = ((string[])e.Data.GetData(DataFormats.FileDrop))[0];
			TbCmd.Text = RtnTbCmdFreadToFormat(GblCmsCmdFn);
			SubTbCmdFocus(-1);
		}

		private void LblDropScript_DragEnter(object sender, DragEventArgs e)
		{
			e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
		}

		private string RtnTbCmdFreadToFormat(string sFn)
		{
			if (!File.Exists(sFn))
			{
				return "";
			}
			string rtn = "";
			(string _fn, string _data) = RtnTextFread(sFn, false, "");
			if (_fn.Length > 0)
			{
				rtn += _data;
			}
			// 引数中の \\; は一時置換で対応
			rtn = rtn.Replace("\\;", "\a");
			rtn = Regex.Replace(rtn, RgxNL + "\\s*", "; ");
			rtn = Regex.Replace(rtn, "(/\\*|\\*/)+;", "$1");
			rtn = rtn.Replace("\a", "\\;");
			return rtn;
		}

		//--------------------------------------------------------------------------------
		// DgvMacro
		//--------------------------------------------------------------------------------
		private int GblDgvMacroRow = 0;
		private bool GblDgvMacroOpen = false; // DgvMacro.enabled より速い

		private void BtnDgvMacro_Click(object sender, EventArgs e)
		{
			DgvMacro.Visible = false;

			if (GblDgvCmdOpen)
			{
				BtnDgvCmd_Click(sender, e);
			}

			if (GblDgvMacroOpen)
			{
				GblDgvMacroOpen = false;
				DgvMacro.Enabled = false;
				BtnDgvMacro.BackColor = Color.RoyalBlue;
				DgvMacro.ScrollBars = ScrollBars.None;

				DgvMacro.Width = 68;
				DgvMacro.Height = 23;

				TbDgvSearch.Visible = BtnDgvSearch.Visible = BtnDgvSearchClear.Visible = false;

				SubTbCmdFocus(GblTbCmdPos);
			}
			else
			{
				GblDgvMacroOpen = true;
				DgvMacro.Enabled = true;
				BtnDgvMacro.BackColor = Color.Crimson;
				DgvMacro.ScrollBars = ScrollBars.Both;
				DgvMacro.Width = Width - 107;

				int i1 = DgvTb11.Width + DgvTb12.Width + DgvTb13.Width + DgvTb14.Width + 20;
				if (DgvMacro.Width > i1)
				{
					DgvMacro.Width = i1;
				}

				DgvMacro.Height = Height - 170;

				TbDgvSearch.Left = DgvMacro.Left + 60;
				BtnDgvSearch.Left = TbDgvSearch.Left + TbDgvSearch.Width - 1;
				BtnDgvSearchClear.Left = BtnDgvSearch.Left + BtnDgvSearchClear.Width - 1;

				TbDgvSearch.BringToFront();
				BtnDgvSearch.BringToFront();
				BtnDgvSearchClear.BringToFront();

				TbDgvSearch.Visible = BtnDgvSearch.Visible = BtnDgvSearchClear.Visible = true;

				_ = TbDgvSearch.Focus();

				if (DgvMacro.RowCount > 0)
				{
					DataGridViewCell _o1 = DgvMacro[0, 0];
					_o1.Selected = true;
					_o1.Style.SelectionBackColor = DgvMacro.DefaultCellStyle.BackColor;
					_o1.Style.SelectionForeColor = DgvMacro.DefaultCellStyle.ForeColor;
					_o1.Dispose();
				}
			}

			DgvMacro.Visible = true;
		}

		private void DgvMacro_Enter(object sender, EventArgs e)
		{
			Lbl_F2.ForeColor = Color.Gold;
		}

		private void DgvMacro_Leave(object sender, EventArgs e)
		{
			Lbl_F2.ForeColor = Color.Gray;
		}

		private void DgvMacro_CellEnter(object sender, DataGridViewCellEventArgs e)
		{
			DgvCellColor(sender, e, Color.Empty, Color.Empty);
		}

		private void DgvMacro_CellLeave(object sender, DataGridViewCellEventArgs e)
		{
			DgvCellColor(sender, e, DgvMacro.DefaultCellStyle.BackColor, DgvMacro.DefaultCellStyle.ForeColor);
		}

		private void DgvMacro_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
		{
			DgvMacro.Rows[e.RowIndex].Cells[0].ToolTipText = "[Ctrl]を押しながら選択すると挿入モード";
		}

		private void DgvMacro_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
		{
			// 外部から操作されたとき e は発火しない
			if ((e != null && e.RowIndex == -1))
			{
				return;
			}

			int iPosForward = 0;
			string rtn;

			if (DgvMacro.CurrentCellAddress.X < 3)
			{
				rtn = DgvMacro[0, DgvMacro.CurrentCellAddress.Y].Value.ToString();
				int i1 = DictMacroOptCnt[rtn.ToLower()];
				if (i1 > 0)
				{
					for (int _i3 = 0; _i3 < i1; _i3++)
					{
						rtn += " \"\"";
					}
					iPosForward = ((i1 - 1) * 3) + 1;
				}
			}
			else
			{
				rtn = DgvMacro[3, DgvMacro.CurrentCellAddress.Y].Value.ToString();
			}

			// [Ctrl]+ のときは挿入モード／それ以外は上書き
			if ((ModifierKeys & Keys.Control) == Keys.Control)
			{
				// TbCmd.SelectionStart 取得が先
				GblTbCmdPos = TbCmd.SelectionStart + rtn.Length - iPosForward;
				TbCmd.Paste(rtn + " ");

				// 引数なしのとき
				if (GblTbCmdPos + 1 == TbCmd.TextLength)
				{
					++GblTbCmdPos;
				}
			}
			else
			{
				GblTbCmdPos = rtn.Length - iPosForward;
				TbCmd.Text = rtn;
			}

			GblDgvMacroOpen = true;
			BtnDgvMacro_Click(sender, e);
		}

		private void DgvMacro_KeyDown(object sender, KeyEventArgs e)
		{
			GblDgvMacroRow = DgvMacro.CurrentRow.Index;
		}

		private void DgvMacro_KeyUp(object sender, KeyEventArgs e)
		{
			// 列方向自動スクロール
			DgvMacro.FirstDisplayedScrollingColumnIndex = DgvMacro.CurrentCell.ColumnIndex;

			// [Ctrl]+[End] で最終セルへ移動した直後、カレントセルを行頭に変更
			if (e.KeyData == (Keys.Control | Keys.End))
			{
				DgvMacro.CurrentCell = DgvMacro[0, DgvMacro.CurrentRow.Index];
				return;
			}

			// [Shift]+[↑]
			if (e.KeyData == (Keys.Shift | Keys.Up))
			{
				DgvMacro.CurrentCell = DgvMacro[0, 0];
				_ = TbDgvSearch.Focus();
				return;
			}

			switch (e.KeyCode)
			{
				// SendKeys で遠隔操作するため
				case Keys.Escape:
					_ = TbDgvSearch.Focus();
					return;

				case Keys.F2:
					BtnDgvMacro_Click(sender, e);
					return;

				case Keys.F3:
					// [F3]はDGVのソートイベントになり干渉するため要注意
					BtnDgvCmd_Click(sender, e);
					return;

				case Keys.Enter:
					DgvMacro.CurrentCell = DgvMacro[DgvMacro.CurrentCell.ColumnIndex, DgvMacro.CurrentCell.RowIndex - (DgvMacro.CurrentRow.Index == GblDgvMacroRow ? 0 : 1)];
					GblTbCmdPos = DgvMacro.CurrentCell.Value.ToString().Length + 2;
					DgvMacro_CellMouseClick(sender, null);
					return;

				case Keys.Space:
					GblTbCmdPos = DgvMacro.CurrentCell.Value.ToString().Length + 2;
					DgvMacro_CellMouseClick(sender, null);
					return;
			}
		}

		//--------------------------------------------------------------------------------
		// DgvCmd
		//--------------------------------------------------------------------------------
		private int GblDgvCmdRow = 0;
		private bool GblDgvCmdOpen = false; // DgvCmd.enabled より速い

		private void BtnDgvCmd_Click(object sender, EventArgs e)
		{
			DgvCmd.Visible = false;

			if (GblDgvMacroOpen)
			{
				BtnDgvMacro_Click(sender, e);
			}

			if (GblDgvCmdOpen)
			{
				GblDgvCmdOpen = false;
				DgvCmd.Enabled = false;
				BtnDgvCmd.BackColor = Color.RoyalBlue;
				DgvCmd.ScrollBars = ScrollBars.None;

				DgvCmd.Width = 68;
				DgvCmd.Height = 23;

				TbDgvSearch.Visible = BtnDgvSearch.Visible = BtnDgvSearchClear.Visible = false;

				SubTbCmdFocus(GblTbCmdPos);
			}
			else
			{
				GblDgvCmdOpen = true;
				DgvCmd.Enabled = true;
				BtnDgvCmd.BackColor = Color.Crimson;
				DgvCmd.ScrollBars = ScrollBars.Both;
				DgvCmd.Width = Width - 194;

				int i1 = DgvTb21.Width + 20;
				if (DgvCmd.Width > i1)
				{
					DgvCmd.Width = i1;
				}

				DgvCmd.Height = Height - 170;

				TbDgvSearch.Left = DgvCmd.Left + 70;
				BtnDgvSearch.Left = TbDgvSearch.Left + TbDgvSearch.Width - 1;
				BtnDgvSearchClear.Left = BtnDgvSearch.Left + BtnDgvSearchClear.Width - 1;

				TbDgvSearch.BringToFront();
				BtnDgvSearch.BringToFront();
				BtnDgvSearchClear.BringToFront();

				TbDgvSearch.Visible = BtnDgvSearch.Visible = BtnDgvSearchClear.Visible = true;

				_ = TbDgvSearch.Focus();

				if (DgvCmd.RowCount > 0)
				{
					DataGridViewCell _o1 = DgvCmd[0, 0];
					_o1.Selected = true;
					_o1.Style.SelectionBackColor = DgvCmd.DefaultCellStyle.BackColor;
					_o1.Style.SelectionForeColor = DgvCmd.DefaultCellStyle.ForeColor;
					_o1.Dispose();
				}
			}

			DgvCmd.Visible = true;
		}

		private void DgvCmd_Enter(object sender, EventArgs e)
		{
			Lbl_F3.ForeColor = Color.Gold;
		}

		private void DgvCmd_Leave(object sender, EventArgs e)
		{
			Lbl_F3.ForeColor = Color.Gray;
		}

		private void DgvCmd_CellEnter(object sender, DataGridViewCellEventArgs e)
		{
			DgvCellColor(sender, e, Color.Empty, Color.Empty);
		}

		private void DgvCmd_CellLeave(object sender, DataGridViewCellEventArgs e)
		{
			DgvCellColor(sender, e, DgvCmd.DefaultCellStyle.BackColor, DgvCmd.DefaultCellStyle.ForeColor);
		}

		private void DgvCellColor(object sender, DataGridViewCellEventArgs e, Color BackColor, Color ForeColor)
		{
			if (e.ColumnIndex >= 0 && e.RowIndex >= 0)
			{
				DataGridViewCell _o1 = ((DataGridView)sender)[e.ColumnIndex, e.RowIndex];
				_o1.Style.SelectionBackColor = BackColor;
				_o1.Style.SelectionForeColor = ForeColor;
				_o1.Dispose();
			}
		}

		private void DgvCmd_CellFormatting(object sender, DataGridViewCellFormattingEventArgs e)
		{
			DgvCmd.Rows[e.RowIndex].Cells[e.ColumnIndex].ToolTipText = "[Ctrl]を押しながら選択すると挿入モード";
		}

		private void DgvCmd_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
		{
			// 外部から操作されたとき e は発火しない
			if (e != null && e.RowIndex == -1)
			{
				return;
			}

			string s1 = DgvCmd[0, DgvCmd.CurrentCell.RowIndex].Value.ToString();

			// [Ctrl]+ のときは挿入モード／それ以外は上書き
			if ((ModifierKeys & Keys.Control) == Keys.Control)
			{
				// TbCmd.SelectionStart 取得が先
				GblTbCmdPos = TbCmd.SelectionStart + s1.Length + 1;
				TbCmd.Paste(s1 + " ");
			}
			else
			{
				GblTbCmdPos = s1.Length;
				TbCmd.Text = s1;
			}

			GblDgvCmdOpen = true;
			BtnDgvCmd_Click(sender, e);
		}

		private void DgvCmd_KeyDown(object sender, KeyEventArgs e)
		{
			GblDgvCmdRow = DgvCmd.CurrentRow.Index;
		}

		private void DgvCmd_KeyUp(object sender, KeyEventArgs e)
		{
			// [Ctrl]+[End] で末尾セルへ移動した直後、カレントセルを行頭に変更
			if (e.KeyData == (Keys.Control | Keys.End))
			{
				DgvCmd.CurrentCell = DgvCmd[0, DgvCmd.CurrentRow.Index];
				return;
			}

			// [Shift]+[↑]
			if (e.KeyData == (Keys.Shift | Keys.Up))
			{
				DgvCmd.CurrentCell = DgvCmd[0, 0];
				_ = TbDgvSearch.Focus();
				return;
			}

			switch (e.KeyCode)
			{
				// SendKeys で遠隔操作するため
				case Keys.Escape:
					_ = TbDgvSearch.Focus();
					return;

				case Keys.F2:
					BtnDgvMacro_Click(sender, e);
					return;

				case Keys.F3:
					BtnDgvCmd_Click(sender, e);
					return;

				case Keys.Enter:
					DgvCmd.CurrentCell = DgvCmd[0, DgvCmd.CurrentCell.RowIndex - (DgvCmd.CurrentRow.Index == GblDgvCmdRow ? 0 : 1)];
					GblTbCmdPos = DgvCmd.CurrentCell.Value.ToString().Length;
					DgvCmd_CellMouseClick(sender, null);
					return;

				case Keys.Space:
					GblTbCmdPos = DgvCmd.CurrentCell.Value.ToString().Length;
					DgvCmd_CellMouseClick(sender, null);
					return;
			}
		}

		private string[] GblAryDgvCmd = { "" };

		private void SubDgvCmdLoad()
		{
			TbDgvSearch.Visible = BtnDgvSearch.Visible = BtnDgvSearchClear.Visible = false;
			List<string> lst1 = new List<string>(32);
			List<string> lst2 = new List<string>(512);
			// PATH 要素追加
			// Dir 取得
			foreach (string _s1 in Environment.GetEnvironmentVariable("Path").Replace("/", "\\").Split(';'))
			{
				if (Directory.Exists(_s1))
				{
					lst1.Add(_s1.TrimEnd('\\'));
				}
			}
			// File 取得
			lst1.Sort();
			foreach (string _s1 in lst1.Distinct())
			{
				DirectoryInfo DI = new DirectoryInfo(_s1);
				foreach (FileInfo _fi1 in DI.GetFiles("*", SearchOption.TopDirectoryOnly))
				{
					if (Regex.IsMatch(_fi1.FullName, "\\.(exe|bat)$", RegexOptions.IgnoreCase))
					{
						lst2.Add(Path.GetFileName(_fi1.FullName));
					}
				}
			}
			// File 重複排除
			lst2.Sort();
			GblAryDgvCmd = lst2.Distinct().ToArray();
			foreach (string _s1 in GblAryDgvCmd)
			{
				_ = DgvCmd.Rows.Add(_s1);
			}
		}

		//--------------------------------------------------------------------------------
		// TbDgvSearch
		//--------------------------------------------------------------------------------
		// 0=DgvMacro／1=DgvCmd
		private readonly string[] GblAryTbDgvSearch = { "", "" };

		private void TbDgvSearch_Enter(object sender, EventArgs e)
		{
			Text = TextDefault;

			if (GblLblTooltipVisible)
			{
				SubLblTooltip(LblTooltip_TbDgvSearch);
			}

			TbDgvSearch.BackColor = Color.LightYellow;
			if (GblDgvMacroOpen)
			{
				Lbl_F2.ForeColor = Color.Gold;
				Lbl_F3.ForeColor = Color.Gray;
				TbDgvSearch.Text = GblAryTbDgvSearch[0];
			}
			else
			{
				Lbl_F2.ForeColor = Color.Gray;
				Lbl_F3.ForeColor = Color.Gold;
				TbDgvSearch.Text = GblAryTbDgvSearch[1];
			}
			TbDgvSearch.SelectionStart = TbDgvSearch.TextLength;
		}

		private void TbDgvSearch_Leave(object sender, EventArgs e)
		{
			TbDgvSearch.BackColor = Color.Azure;
			Lbl_F2.ForeColor = Lbl_F3.ForeColor = Color.Gray;
		}

		private void TbDgvSearch_TextChanged(object sender, EventArgs e)
		{
			if (GblDgvMacroOpen)
			{
				GblAryTbDgvSearch[0] = TbDgvSearch.Text;
			}
			else
			{
				GblAryTbDgvSearch[1] = TbDgvSearch.Text;
			}
		}

		private void TbDgvSearch_KeyDown(object sender, KeyEventArgs e)
		{
			// [Ctrl]+[Back]
			if (e.KeyData == (Keys.Control | Keys.Back))
			{
				TbDgvSearch.Select(0, TbDgvSearch.SelectionStart);
				TbDgvSearch.Cut();
				return;
			}

			// [Ctrl]+[Delete]
			if (e.KeyData == (Keys.Control | Keys.Delete))
			{
				TbDgvSearch.Select(TbDgvSearch.SelectionStart, TbDgvSearch.TextLength);
				TbDgvSearch.Cut();
				return;
			}

			// [Ctrl]+[A]
			if (e.KeyData == (Keys.Control | Keys.A))
			{
				TbDgvSearch.SelectAll();
				return;
			}

			// [Ctrl]+[C]
			if (e.KeyData == (Keys.Control | Keys.C))
			{
				TbDgvSearch.Copy();
				return;
			}

			// [Ctrl]+[U]
			if (e.KeyData == (Keys.Control | Keys.U))
			{
				TbDgvSearch.Text = "";
				return;
			}

			// [Ctrl]+[V]
			if (e.KeyData == (Keys.Control | Keys.V))
			{
				TbDgvSearch.Paste(Regex.Replace(Clipboard.GetText(), RgxNL, ""));
				return;
			}

			// [Ctrl]+[X]
			if (e.KeyData == (Keys.Control | Keys.X))
			{
				TbDgvSearch.Cut();
				return;
			}

			// [Ctrl]+[Z]
			if (e.KeyData == (Keys.Control | Keys.Z))
			{
				if (TbDgvSearch.CanUndo)
				{
					TbDgvSearch.Undo();
					TbDgvSearch.ClearUndo();
				}
				return;
			}

			// [Shift]+[↓]
			//   [↓] のみは日本語入力時不具合
			if (e.KeyData == (Keys.Shift | Keys.Down))
			{
				_ = GblDgvMacroOpen ? DgvMacro.Focus() : DgvCmd.Focus();
				return;
			}
		}

		private void TbDgvSearch_KeyPress(object sender, KeyPressEventArgs e)
		{
			// ビープ音抑制
			//   [Ctrl] で縛りをかけると [Ctrl]+[キー] を自前で追記する必要がある
			//   MultiLine でないときは [ESC] [Enter] も含める
			if ((ModifierKeys & Keys.Control) == Keys.Control || e.KeyChar == (char)Keys.Escape || e.KeyChar == (char)Keys.Enter)
			{
				e.Handled = true;
			}
		}

		private void TbDgvSearch_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.Escape:
					if (GblDgvMacroOpen)
					{
						GblDgvMacroOpen = true;
						BtnDgvMacro_Click(sender, e);
					}
					else if (GblDgvCmdOpen)
					{
						GblDgvCmdOpen = true;
						BtnDgvCmd_Click(sender, e);
					}
					return;

				case Keys.F2:
					BtnDgvMacro_Click(sender, e);
					return;

				case Keys.F3:
					BtnDgvCmd_Click(sender, e);
					return;

				case Keys.F4:
					if (GblLblTooltipVisible)
					{
						LblTooltip.Visible = GblLblTooltipVisible = false;
					}
					else
					{
						SubLblTooltip(LblTooltip_TbDgvSearch);
					}
					return;

				case Keys.Enter:
					BtnDgvSearch_Click(sender, e);
					return;

				case Keys.PageUp:
					SendKeys.Send("+{DOWN}{PGUP}{ESC}");
					return;

				case Keys.PageDown:
					SendKeys.Send("+{DOWN}{PGDN}{ESC}");
					return;
			}
		}

		private void BtnDgvSearch_Click(object sender, EventArgs e)
		{
			if (GblDgvMacroOpen)
			{
				DgvMacro.Enabled = false;
				DgvMacro.Rows.Clear();
				for (int _i1 = 0; _i1 < AryDgvMacro.Length; _i1 += 5)
				{
					if (AryDgvMacro[_i1].ToString().IndexOf(TbDgvSearch.Text, StringComparison.OrdinalIgnoreCase) >= 0 || AryDgvMacro[_i1 + 1].ToString().IndexOf(TbDgvSearch.Text, StringComparison.OrdinalIgnoreCase) >= 0)
					{
						_ = DgvMacro.Rows.Add(AryDgvMacro[_i1].ToString(), AryDgvMacro[_i1 + 1].ToString(), AryDgvMacro[_i1 + 2].ToString(), AryDgvMacro[_i1 + 3].ToString());
					}
				}
				if (DgvMacro.RowCount > 0)
				{
					DataGridViewCell _o1 = DgvMacro[0, 0];
					_o1.Style.SelectionBackColor = DgvMacro.DefaultCellStyle.BackColor;
					_o1.Style.SelectionForeColor = DgvMacro.DefaultCellStyle.ForeColor;
					_o1.Dispose();
				}
				DgvMacro.Enabled = true;
			}
			else
			{
				DgvCmd.Enabled = false;
				DgvCmd.Rows.Clear();
				foreach (string _s1 in GblAryDgvCmd)
				{
					if (_s1.IndexOf(TbDgvSearch.Text, StringComparison.OrdinalIgnoreCase) >= 0)
					{
						_ = DgvCmd.Rows.Add(_s1);
					}
				}
				if (DgvCmd.RowCount > 0)
				{
					DataGridViewCell _o1 = DgvCmd[0, 0];
					_o1.Style.SelectionBackColor = DgvCmd.DefaultCellStyle.BackColor;
					_o1.Style.SelectionForeColor = DgvCmd.DefaultCellStyle.ForeColor;
					_o1.Dispose();
				}
				DgvCmd.Enabled = true;
			}
			_ = TbDgvSearch.Focus();
		}

		private void BtnDgvSearchClear_Click(object sender, EventArgs e)
		{
			TbDgvSearch.Text = "";
			_ = TbDgvSearch.Focus();
		}

		//--------------------------------------------------------------------------------
		// CmsTbDgvSearch
		//--------------------------------------------------------------------------------
		private void CmsTbDgvSearch_Opened(object sender, EventArgs e)
		{
			_ = TbDgvSearch.Focus();
			CmsTbDgvSearch_クリア.Text = TbDgvSearch.SelectedText.Length == 0 ? "全クリア" : "クリア";
		}

		private void CmsTbDgvSearch_クリア_Click(object sender, EventArgs e)
		{
			if (TbDgvSearch.SelectedText.Length == 0)
			{
				TbDgvSearch.Text = "";
			}
			else
			{
				TbDgvSearch.SelectedText = "";
			}
		}

		private void CmsTbDgvSearch_ペースト_Click(object sender, EventArgs e)
		{
			TbDgvSearch.Paste(Regex.Replace(Clipboard.GetText(), RgxNL, ""));
		}

		private void CmsTbDgvSearch_ペースト_MouseHover(object sender, EventArgs e)
		{
			ToolTip.SetToolTip(CmsTbDgvSearch, RtnClipboardSubstring());
		}

		//--------------------------------------------------------------------------------
		// 実行
		//--------------------------------------------------------------------------------
		// 設定ファイル中にコメント /* ～ */ があるとき
		private bool GblRemOn = false;

		private void BtnCmdExec_Click(object sender, EventArgs e)
		{
			// 背景色を表示させない
			TbCmd.SelectionStart = TbCmd.TextLength;

			GblRemOn = false;

			// 出力を記憶（実行前）
			GblCmdExecOld = TbResult.Text;

			// Trim() で置換すると GblTbCmdPos が変わるので不可
			if (TbCmd.Text.Trim().Length == 0)
			{
				return;
			}

			ExecStopOn = false;

			Cursor.Current = Cursors.WaitCursor;

			// 表示色を更新
			SubTbResultChange(GblAryResultIndex, TbCmd);

			// 実行
			// \\; を \a に置換
			string[] a1 = TbCmd.Text.Replace("\\;", "\a").Split(';');
			for (int _i1 = 0; _i1 < a1.Length; _i1++)
			{
				if (a1[_i1].Length > 0)
				{
					// \a を ; に置換
					SubTbCmdExec(a1[_i1].Replace("\a", ";"));
				}
			}

			// タイトル表示を戻す
			Text = TextDefault;

			// 出力を記憶（実行後）
			GblCmdExecNew = TbResult.Text;

			Cursor.Current = Cursors.Default;

			TbCmd.Text = "";
			GblTbCmdPos = 0;
			SubTbCmdFocus(GblTbCmdPos);
		}

		// 実行履歴を [↑][↓] で呼び出す
		private readonly List<string> GblLstTbCmdHistory = new List<string>();
		private int GblLstTbCmdHistoryPos = 0;

		private void SubTbCmdHistory_Set(string s1)
		{
			try
			{
				// 実行履歴を [↑][↓] で呼び出すとき、履歴末尾の重複はスルー
				if (GblLstTbCmdHistory[GblLstTbCmdHistory.Count - 1].ToLower() == s1.ToLower())
				{
					return;
				}
			}
			catch
			{
			}
			// 追加
			GblLstTbCmdHistory.Add(s1);
			// インデックス末尾
			GblLstTbCmdHistoryPos = GblLstTbCmdHistory.Count;
		}

		private void SubTbCmdHistory_Get(bool bKeyUp)
		{
			TbCmd.Text = "";
			if (bKeyUp)
			{
				--GblLstTbCmdHistoryPos;
				try
				{
					TbCmd.Text = GblLstTbCmdHistory[GblLstTbCmdHistoryPos];
				}
				catch
				{
					GblLstTbCmdHistoryPos = -1;
				}
			}
			else
			{
				++GblLstTbCmdHistoryPos;
				try
				{
					TbCmd.Text = GblLstTbCmdHistory[GblLstTbCmdHistoryPos];
				}
				catch
				{
					GblLstTbCmdHistoryPos = GblLstTbCmdHistory.Count;
				}
			}

			TbCmd.SelectionStart = TbCmd.TextLength;
		}

		private void BtnCmdExecStream_Click(object sender, EventArgs e)
		{
			BtnCmdExecStream.Visible = false;
			ExecStopOn = true;
		}

		private void SubTbCmdExec(string sCmd)
		{
			// エラーが発生しているとき
			if (ExecStopOn)
			{
				return;
			}

			// 文頭文末の空白と、末尾の ; を消除
			sCmd = sCmd.Trim().TrimEnd(';');

			// コメント行
			if (Regex.IsMatch(sCmd, "^//"))
			{
				return;
			}

			// コメント /* ～ */
			if (Regex.IsMatch(sCmd, "^/\\*"))
			{
				GblRemOn = true;
				return;
			}
			else if (Regex.IsMatch(sCmd, "^\\*/"))
			{
				GblRemOn = false;
				sCmd = Regex.Replace(sCmd, "^\\*/", "").TrimStart();
				if (sCmd.Length == 0)
				{
					return;
				}
			}
			else if (GblRemOn)
			{
				return;
			}

			// タイトルに表示
			Text = sCmd;

			// 変数
			int i1 = 0;
			int i2 = 0;
			int i3 = 0;
			string s1 = "";
			string s2 = "";
			StringBuilder sb = new StringBuilder();
			int iRead = 0;
			int iNL = NL.Length;
			int iLine = 0;

			if (sCmd.Length == 0)
			{
				return;
			}

			// 実行が成功のとき true
			bool bCmd = true;

			SubLblWaitOn(true);

			// マクロ実行
			if (sCmd[0] == '#')
			{
				//【重要】検索用フラグ " " 付与
				sCmd += " ";

				// 空の aOp[n] を作成
				const int aOpLen = 8;
				string[] aOp = Enumerable.Repeat<string>("", aOpLen).ToArray();

				// aOp[] 取得／大小区別しない
				aOp[0] = new Regex("^(?<v1>.+?) ").Match(sCmd).Groups["v1"].Value.ToLower();

				// 存在チェック
				try
				{
					_ = DictMacroOptCnt[aOp[0]];
				}
				catch
				{
					ExecStopOn = true;
					SubLblWaitOn(false);
					_ = MessageBox.Show(
						"[Err] マクロを確認してください。" + NL +
						NL +
						"・" + aOp[0] + "？" + NL +
						NL +
						"プログラムを停止します。",
						VERSION
					);
					return;
				}

				// \\\\ を \a1 に置換
				sCmd = sCmd.Replace("\\\\", "\a1");

				// \\\" を \a2 に置換
				sCmd = sCmd.Replace("\\\"", "\a2");

				if (DictMacroOptCnt[aOp[0]] > 0)
				{
					// \" が奇数個のとき引数エラー
					char[] cs = sCmd.ToCharArray();
					i1 = 0;

					foreach (char _cs1 in cs)
					{
						if (_cs1 == '\"')
						{
							++i1;
						}
					}

					if ((i1 % 2) != 0)
					{
						ExecStopOn = true;
						SubLblWaitOn(false);
						_ = MessageBox.Show(
							"[Err] 引数を確認してください。" + NL +
							NL +
							"・引数が \"...\" で囲まれていますか？" + NL +
							"・\" と \\\" を混同していませんか？" + NL +
							NL +
							"プログラムを停止します。",
							VERSION
						);
						return;
					}
				}

				// \t を \a3 に置換
				sCmd = sCmd.Replace("\\t", "\a3");

				// \a1 を \\\\ に戻す
				sCmd = sCmd.Replace("\a1", "\\\\");

				i1 = 1;
				foreach (Match _m1 in new Regex("\\s\"(?<v1>.*?)\"").Matches(sCmd))
				{
					// \a2... を元に戻す
					aOp[i1] = _m1.Groups["v1"].Value.Replace("\a2", "\\\"").Replace("\a3", "\\t");
					++i1;
					if (i1 >= aOpLen)
					{
						break;
					}
				}

				// \a2... を元に戻す
				sCmd = sCmd.Replace("\a2", "\\\"").Replace("\a3", "\\t");

				// "#(sCmd)2" ("#grep2" 等) のとき大小文字を区別して検索
				RegexOptions RgxOpt = aOp[0].EndsWith("2") ? RegexOptions.None : RegexOptions.IgnoreCase;

				switch (aOp[0])
				{
					// 出力行毎に処理
					case "#stream":
						if (aOp[1].Length == 0 || !Regex.IsMatch(aOp[1], "#{\\d{0,1}}"))
						{
							ExecStopOn = true;
							SubLblWaitOn(false);
							_ = MessageBox.Show(
								"[Err] 出力行 #{} が指定されていません。",
								VERSION
							);
							return;
						}

						BtnCmdExecStream.Visible = true;

						// カレント出力を反映
						GblAryResultBuf[GblAryResultIndex] = TbResult.Text;

						aOp[1] = aOp[1].Replace("#{}", $"#{{{GblAryResultIndex}}}");

						// 使用されている #{1} - #{5} を取得
						SortedSet<int> SortedSetTabNumber = new SortedSet<int>();
						for (i1 = GblAryResultMin; i1 <= GblAryResultMax; i1++)
						{
							if (aOp[1].Contains($"#{{{i1}}}"))
							{
								SortedSetTabNumber.Add(i1);
							}
						}

						// #{1} - #{5} の最大行数を取得
						iLine = 0;
						foreach (int _i1 in SortedSetTabNumber)
						{
							int _i2 = Regex.Split(GblAryResultBuf[_i1].TrimEnd(), RgxNL).Length;
							if (iLine < _i2)
							{
								iLine = _i2;
							}
						}

						string[] aCmd = new string[iLine];

						// 仮のコマンド配列を作成
						for (i1 = 0; i1 < iLine; i1++)
						{
							aCmd[i1] = RtnCnvMacroVar(aOp[1], i1 + 1);
						}

						// コマンド配列を #{1} - #{5} の内容で変換
						foreach (int _i1 in SortedSetTabNumber)
						{
							iLine = 0;
							foreach (string _s1 in Regex.Split(GblAryResultBuf[_i1].TrimEnd(), RgxNL))
							{
								aCmd[iLine] = aCmd[iLine].Replace($"#{{{_i1}}}", _s1);
								++iLine;
							}
						}

						// 該当なしの #{1} - #{5} を消去
						for (i1 = 0; i1 < aCmd.Length; i1++)
						{
							aCmd[i1] = Regex.Replace(aCmd[i1], "#{\\d}", "");
						}

						// TopMost
						bool bTopMost = TopMost;
						if (bTopMost)
						{
							ChkTopMost.Checked = TopMost = false;
						}

						PS = new Process();
						PS.StartInfo.UseShellExecute = false;
						PS.StartInfo.RedirectStandardInput = true;
						PS.StartInfo.RedirectStandardOutput = true;
						PS.StartInfo.RedirectStandardError = true;
						PS.StartInfo.CreateNoWindow = true;
						PS.StartInfo.FileName = "cmd.exe";

						string stdout932 = "";
						string stderr932 = "";
						string stdout65001 = "";
						string stderr65001 = "";

						_ = SbStdOut.Clear();
						_ = SbStdErr.Clear();

						iLine = 0;

						foreach (string _s1 in aCmd)
						{
							++iLine;
							Text = $"L{iLine}: {_s1}";

							if (_s1.Trim().Length > 0)
							{
								PS.StartInfo.Arguments = $"/c {_s1}";
								try
								{
									// CP932 で読込
									PS.StartInfo.StandardOutputEncoding = PS.StartInfo.StandardErrorEncoding = Encoding.GetEncoding(932);
									_ = PS.Start();
									PS.StandardInput.Close();
									stdout932 = RtnTbResultFormat(PS.StandardOutput.ReadToEnd());
									if (stdout932.Length == 0)
									{
										throw new Exception("実行プロセスから戻り値がない。");
									}
									stderr932 = RtnTbResultFormat(PS.StandardError.ReadToEnd());
									PS.Close();

									// CP65001 で読込
									PS.StartInfo.StandardOutputEncoding = PS.StartInfo.StandardErrorEncoding = Encoding.GetEncoding(65001);
									_ = PS.Start();
									PS.StandardInput.Close();
									stdout65001 = RtnTbResultFormat(PS.StandardOutput.ReadToEnd());
									stderr65001 = RtnTbResultFormat(PS.StandardError.ReadToEnd());
									PS.Close();
								}
								catch
								{
									PS.Close();
								}

								// 文字コードを推測
								if (stdout932.Length < stdout65001.Length)
								{
									_ = SbStdOut.Append(stdout932);
									_ = SbStdErr.Append(stderr932);
								}
								else
								{
									_ = SbStdOut.Append(stdout65001);
									_ = SbStdErr.Append(stderr65001);
								}
							}
						}

						// TopMost
						if (bTopMost)
						{
							ChkTopMost.Checked = TopMost = true;
						}

						// 出力[n]
						_ = int.TryParse(aOp[2], out i1);
						if (RtnAryResultBtnRangeChk(i1))
						{
							GblAryResultBuf[i1] += SbStdOut.ToString();
							if (i1 == GblAryResultIndex)
							{
								TbResult.Text = GblAryResultBuf[i1];
							}
							// 表示色を更新
							SubTbResultChange(GblAryResultIndex, TbCmd);
						}

						BtnCmdExecStream.Visible = false;
						_ = TbCmd.Focus();
						break;

					// 出力行毎にダウンロード
					case "#streamdl":
						aOp[1] = RtnErrFnToWide(aOp[1]);
						BtnCmdExecStream.Visible = true;
						_ = TbResult.Focus();
						TbResult.SelectionStart = 0;
						TbResult.ScrollToCaret();
						iRead = 0;
						iLine = 0;
						foreach (string _s1 in Regex.Split(TbResult.Text.TrimEnd(), RgxNL))
						{
							++iLine;
							Text = $"L{iLine}: {_s1}";
							// TbResult の進捗状況
							TbResult.Select(iRead, _s1.Length);
							iRead += _s1.Length + iNL;
							TbResult.ScrollToCaret();
							string _s2 = _s1.Trim();
							if (_s2.Length > 0)
							{
								// 処理中断
								Thread.Sleep(50);
								Application.DoEvents();
								if (ExecStopOn)
								{
									break;
								}
								// aOp[1] 本体は変更しない
								string _s3 = RtnCnvMacroVar(aOp[1], iLine);
								if (_s3.Length > 0)
								{
									if (Path.GetFileName(_s3).Length > 0)
									{
										// 拡張子付与
										_s3 += Path.GetExtension(_s2);
									}
									else
									{
										// ファイル名付与
										_s3 += Path.GetFileName(_s2);
									}
								}
								else
								{
									_s3 = Path.GetFileName(_s2);
								}
								HttpClient = new HttpClient();
								try
								{
									// URLはソノママ処理
									// ローカルの同一ファイルは処理しない
									if (Regex.IsMatch(_s2, "^[A-Za-z]\\:") && Path.GetFullPath(_s2) == Path.GetFullPath(_s3))
									{
									}
									else
									{
										File.WriteAllBytes(_s3, HttpClient.GetByteArrayAsync(_s2).Result);
									}
								}
								catch
								{
								}
								HttpClient.Dispose();
							}
						}
						BtnCmdExecStream.Visible = false;
						_ = TbCmd.Focus();
						break;

					// 直前に出力された Stdout
					case "#stdout":
						TbResult.AppendText(SbStdOut.ToString());
						break;

					// 直前に出力された Stderr
					case "#stderr":
						TbResult.AppendText(SbStdErr.ToString());
						break;

					// 一時変数
					case "#set":
						// 【リスト】#set
						// 【削除】  #set "Key" "" | #set "Key"
						// 【登録・変更】#set "Key" "Data" => #{:key} = "Data"
						if (aOp[1].Length == 0 && aOp[2].Length == 0)
						{
							// リスト
							foreach (KeyValuePair<string, string> _kv1 in DictTmpVar)
							{
								TbResult.AppendText($"#{{:{_kv1.Key}}}\t{_kv1.Value}{NL}");
							}
						}
						else
						{
							if (aOp[2].Length == 0)
							{
								// 削除
								_ = DictTmpVar.Remove(aOp[1]);
							}
							else
							{
								// 登録・変更
								DictTmpVar[aOp[1]] = RtnCnvEscVar(aOp[2]);
							}
						}
						break;

					// 最初のフォルダに戻る
					case "#bd":
						Directory.SetCurrentDirectory(BaseDir);
						TbCurDir.Text = BaseDir;
						break;

					// フォルダ変更
					case "#cd":
						if (aOp[1].Length == 0)
						{
							TbCurDir_Click(null, null);
							break;
						}

						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						aOp[1] = aOp[1].Replace("\"", "");
						string _sFullPath = Path.GetFullPath(aOp[1]);

						try
						{
							// フォルダが存在しないときは新規作成
							if (!Directory.Exists(_sFullPath))
							{
								_ = Directory.CreateDirectory(_sFullPath);
							}
							Directory.SetCurrentDirectory(_sFullPath);
							TbCurDir.Text = _sFullPath;
						}
						catch
						{
							ExecStopOn = true;
							SubLblWaitOn(false);
							_ = MessageBox.Show(
								"[Err] アクセス権限のないフォルダです。" + NL +
								NL +
								"・" + _sFullPath + NL +
								NL +
								"プログラムを停止します。",
								VERSION
							);
							return;
						}
						break;

					// クリア
					case "#cls":
						if (aOp[1].Length == 0)
						{
							i1 = GblAryResultIndex;
						}
						else
						{
							_ = int.TryParse(aOp[1], out i1);
						}

						if (RtnAryResultBtnRangeChk(i1))
						{
							if (i1 == GblAryResultIndex)
							{
								TbResult.Text = "";
							}
							else
							{
								GblAryResultBuf[i1] = "";
							}

							// 表示色を更新
							SubTbResultChange(GblAryResultIndex, TbCmd);

							GC.Collect();
						}
						break;

					// 全クリア
					case "#clear":
						TbResult.Text = "";
						for (int _i1 = GblAryResultMin; _i1 <= GblAryResultMax; _i1++)
						{
							GblAryResultBuf[_i1] = "";
						}
						SubTbResultChange(1, TbCmd);
						GC.Collect();
						break;

					// 印字
					case "#print":
					case "#puts":
						s1 = aOp[0] == "#puts" ? NL : "";
						if (!int.TryParse(aOp[2], out i1))
						{
							i1 = 1;
						}
						aOp[1] = RtnCnvEscVar(aOp[1]);
						_ = sb.Clear();
						for (int _i1 = 1; _i1 <= i1; _i1++)
						{
							_ = sb.Append(RtnCnvMacroVar(aOp[1], _i1) + s1);
						}
						TbResult.AppendText(sb.ToString());
						break;

					// 別の出力画面へコピー
					case "#copyto":
						_ = int.TryParse(aOp[1], out i1);
						if (RtnAryResultBtnRangeChk(i1))
						{
							GblAryResultBuf[i1] += TbResult.Text;
							SubTbResultChange(GblAryResultIndex, TbCmd);
						}
						break;

					// 別の出力画面からコピー
					case "#copyfrom":
						_ = int.TryParse(aOp[1], out i1);
						if (RtnAryResultBtnRangeChk(i1))
						{
							TbResult.AppendText(GblAryResultBuf[i1]);
						}
						break;

					// 別の出力画面へ表示切替
					case "#move":
						_ = int.TryParse(aOp[1], out i1);
						if (RtnAryResultBtnRangeChk(i1))
						{
							SubTbResultChange(i1, TbCmd);
						}
						break;

					// この出力画面の行始へ移動
					case "#rowbgn":
						TbResult.SelectionStart = 0;
						TbResult.ScrollToCaret();
						break;

					// この出力画面の行末へ移動
					case "#rowend":
						TbResult.SelectionStart = TbResult.TextLength;
						TbResult.ScrollToCaret();
						break;

					// 別の出力画面から行結合
					case "#row+":
						TbResult.Paste(RtnJoinCopy(aOp[1]));
						break;

					// 別の出力画面から列結合
					case "#column+":
						TbResult.Paste(RtnJoinColumn(aOp[1], aOp[2]));
						break;

					// 検索
					case "#grep":
					case "#grep2":
						TbResult.Text = RtnTextGrep(TbResult.Text, aOp[1], RgxOpt, true);
						break;

					// 不一致検索
					case "#except":
					case "#except2":
						TbResult.Text = RtnTextGrep(TbResult.Text, aOp[1], RgxOpt, false);
						break;

					// 検索／合致数指定
					case "#greps":
					case "#greps2":
						TbResult.Text = RtnTextGreps(TbResult.Text, aOp[1], aOp[2], RgxOpt);
						break;

					// 抽出
					case "#extract":
					case "#extract2":
						TbResult.Text = RtnTextExtract(TbResult.Text, aOp[1], RgxOpt);
						break;

					// 指定行を抽出
					case "#getrow":
						_ = int.TryParse(aOp[1], out i1);
						_ = int.TryParse(aOp[2], out i2);
						TbResult.Text = RtnTextGetRow(TbResult.Text, i1, i2);
						break;

					// 置換
					case "#replace":
					case "#replace2":
						TbResult.Text = RtnTextReplace(TbResult.Text, aOp[1], aOp[2], RgxOpt);
						break;

					// 分割変換
					case "#split":
					case "#split2":
						TbResult.Text = RtnTextSplit(TbResult.Text, aOp[1], aOp[2], RgxOpt);
						break;

					// ソート昇順
					case "#sort":
						TbResult.Text = RtnTextSort(TbResult.Text, true);
						break;

					// ソート降順
					case "#reverse":
						TbResult.Text = RtnTextSort(TbResult.Text, false);
						break;

					// 重複行をクリア
					case "#uniq":
						sb.Clear();
						s1 = null;
						foreach (string _s1 in Regex.Split(TbResult.Text.TrimEnd(), RgxNL))
						{
							if (_s1.Length > 0 && _s1 != s1)
							{
								s1 = _s1;
								_ = sb.Append(_s1);
								_ = sb.Append(NL);
							}
						}
						TbResult.Text = sb.ToString();
						break;

					// 文頭文末の空白クリア
					case "#trim":
						TbResult.Text = RtnTextTrim(TbResult.Text);
						break;

					// 改行をクリア
					case "#rmnl":
						TbResult.Text = Regex.Replace(TbResult.Text, NL, "");
						break;

					// 大文字変換
					case "#toupper":
						TbResult.Text = TbResult.Text.ToUpper();
						break;

					// 小文字変換
					case "#tolower":
						TbResult.Text = TbResult.Text.ToLower();
						break;

					// 全角変換
					case "#towide":
						TbResult.Text = Strings.StrConv(TbResult.Text, VbStrConv.Wide, 0x411);
						break;

					// 全角変換／数字のみ
					case "#tozennum":
						TbResult.Text = Regex.Replace(TbResult.Text, "\\d+", RtnReplacerWide);
						break;

					// 全角変換／Unicode半角カナのみ
					case "#tozenkana":
						TbResult.Text = Regex.Replace(TbResult.Text, "[\uff61-\uFF9f]+", RtnReplacerWide);
						break;

					// 半角変換
					case "#tonarrow":
						TbResult.Text = Strings.StrConv(TbResult.Text, VbStrConv.Narrow, 0x411);
						break;

					// 半角変換／Unicode全角０-９数字のみ
					case "#tohannum":
						TbResult.Text = Regex.Replace(TbResult.Text, "[\uff10-\uff19]+", RtnReplacerNarrow);
						break;

					// 半角変換／Unicode全角カナのみ
					case "#tohankana":
						TbResult.Text = Regex.Replace(TbResult.Text, "[\u30A1-\u30F6]+", RtnReplacerNarrow);
						break;

					// フォルダ・ファイル一覧
					case "#dflist":
					case "#dlist":
					case "#flist":
						switch (aOp[0])
						{
							case "#dlist":
								s1 = "d";
								break;

							case "#flist":
								s1 = "f";
								break;

							default:
								s1 = "df";
								break;
						}
						_ = sb.Clear();
						aOp[1] = aOp[1].Length == 0 ? "." : RtnCnvMacroVar(aOp[1], 0);
						if (Directory.Exists(aOp[1]))
						{
							_ = sb.Append(RtnDirFileList(aOp[1], s1, true));
							_ = sb.Append(NL);
						}
						TbResult.AppendText(sb.ToString());
						TbResult.ScrollToCaret();
						break;

					// URLからソース取得
					case "#wread":
						HttpClient = new HttpClient();
						try
						{
							TbResult.Paste(Regex.Replace(HttpClient.GetStringAsync(aOp[1]).Result, RgxNL, NL));
						}
						catch (Exception ex)
						{
							ExecStopOn = true;
							SubLblWaitOn(false);
							_ = MessageBox.Show(
								"[Err] " + ex.Message,
								VERSION
							);
							return;
						}
						HttpClient.Dispose();
						break;

					// テキストファイル読込
					case "#fread":
						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						(s1, s2) = RtnTextFread(aOp[1], false, "");
						if (s1.Length > 0)
						{
							TbResult.Paste(Regex.Replace(s2, RgxNL, NL));
						}
						break;

					// テキストファイル書込(Shift_JIS)
					case "#fwrite":
						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						_ = int.TryParse(aOp[2], out i1);
						_ = RtnTextFileWrite(TbResult.Text, "932", aOp[1], false, "");
						break;

					// テキストファイル書込(UTF-8 BOMあり)
					case "#fwriteu8":
						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						_ = int.TryParse(aOp[2], out i1);
						_ = RtnTextFileWrite(TbResult.Text, "65001bom", aOp[1], false, "");
						break;

					// テキストファイル書込(UTF-8 BOMなし)
					case "#fwriteu8n":
						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						_ = int.TryParse(aOp[2], out i1);
						_ = RtnTextFileWrite(TbResult.Text, "65001nobom", aOp[1], false, "");
						break;

					// 出力行毎のファイル名を置換
					case "#frename":
						if (aOp[1].Length == 0 || aOp[1].Length == 0)
						{
							break;
						}
						sb.Clear();
						i1 = 0;
						aOp[1] = RtnErrFnToWide(aOp[1]);
						foreach (string _s1 in Regex.Split(TbResult.Text.TrimEnd(), RgxNL))
						{
							// 文頭文末の " を消除
							string _sOldFn = Regex.Replace(_s1.Trim(), "^\"(.+)\"", "$1");
							if (File.Exists(_sOldFn))
							{
								_sOldFn = Path.GetFullPath(_sOldFn);
								string _dir = Path.GetDirectoryName(_sOldFn) + "\\";
								int _dirLen = _dir.Length;
								// 正規表現文法エラーはないか？
								// 使用中のファイルでないか？
								try
								{
									++i1;
									// ファイル名に大小区別しない
									string _sNewFn = _dir + Regex.Replace(_sOldFn.Substring(_dirLen), aOp[1], RtnCnvMacroVar(aOp[2], i1), RegexOptions.IgnoreCase);
									if (_sOldFn == _sNewFn)
									{
										_ = sb.Append(_sOldFn);
									}
									else
									{
										File.Move(_sOldFn, _sNewFn);
										_ = sb.Append(_sNewFn);
									}
								}
								catch
								{
									_ = sb.Append(_sOldFn);
								}
								_ = sb.Append(NL);
							}
						}
						TbResult.Text = sb.ToString();
						break;

					// 出力行毎のファイル内を検索
					case "#fgrep":
					case "#fgrep2":
						if (aOp[1].Length == 0)
						{
							break;
						}
						aOp[1] = RtnCnvMacroVar(aOp[1], 0);
						_ = sb.Clear();
						foreach (string _s1 in Regex.Split(TbResult.Text.TrimEnd(), RgxNL))
						{
							_ = sb.Append(RtnGrepFile(_s1, aOp[1], RgxOpt));
						}
						TbResult.Text = sb.ToString();
						TbResult.ScrollToCaret();
						break;

					// ファイル作成日時変更
					case "#fsetctime":
						_ = int.TryParse(aOp[2], out i1);
						SubSetFileTime(TbResult.Text, aOp[1], i1, "c");
						break;

					// ファイル更新日時変更
					case "#fsetmtime":
						_ = int.TryParse(aOp[2], out i1);
						SubSetFileTime(TbResult.Text, aOp[1], i1, "m");
						break;

					// ファイルアクセス日時変更
					case "#fsetatime":
						_ = int.TryParse(aOp[2], out i1);
						SubSetFileTime(TbResult.Text, aOp[1], i1, "a");
						break;

					// ファイルから指定行を抽出
					case "#fgetrow":
						_ = int.TryParse(aOp[1], out i1);
						_ = int.TryParse(aOp[2], out i2);
						_ = int.TryParse(aOp[3], out i3);
						SbStdOut.Clear();
						foreach (string _s1 in Regex.Split(TbResult.Text.TrimEnd(), RgxNL))
						{
							// 文頭文末の " を消除
							string _sFn = Regex.Replace(_s1, "^\"(.+)\"", "$1");
							if (File.Exists(_sFn))
							{
								(s1, s2) = RtnTextFread(_sFn, false, "");
								_ = SbStdOut.Append(RtnTextGetRow(s2, i1, i2));
							}
						}
						// 出力[n]
						if (RtnAryResultBtnRangeChk(i3))
						{
							GblAryResultBuf[i3] += SbStdOut.ToString();
							// 表示色を更新
							SubTbResultChange(GblAryResultIndex, TbCmd);
						}
						break;

					// フォーム位置
					case "#pos":
						WindowState = FormWindowState.Normal;
						// X
						s1 = RtnCnvMacroVar(aOp[1], 0);
						if (Regex.IsMatch(s1, "\\d+%"))
						{
							_ = int.TryParse(s1.Replace("%", ""), out i1);
							i1 = (int)(Screen.GetWorkingArea(this).Width / 100.0 * i1);
						}
						else if (s1.Length == 0)
						{
							i1 = Location.X;
						}
						else
						{
							_ = int.TryParse(s1, out i1);
						}
						// Y
						s1 = RtnCnvMacroVar(aOp[2], 0);
						if (Regex.IsMatch(s1, "\\d+%"))
						{
							_ = int.TryParse(s1.Replace("%", ""), out i2);
							i2 = (int)(Screen.GetWorkingArea(this).Height / 100.0 * i2);
						}
						else if (s1.Length == 0)
						{
							i2 = Location.Y;
						}
						else
						{
							_ = int.TryParse(s1, out i2);
						}
						SetDesktopLocation(i1, i2);
						break;

					// フォーム位置／中央
					case "#poscenter":
						WindowState = FormWindowState.Normal;
						Location = new Point((Screen.GetWorkingArea(this).Width - Width) / 2, (Screen.GetWorkingArea(this).Height - Height) / 2);
						break;

					// フォームサイズ
					case "#size":
						// Width
						s1 = RtnCnvMacroVar(aOp[1], 0);
						if (Regex.IsMatch(s1, "\\d+%"))
						{
							_ = int.TryParse(s1.Replace("%", ""), out i1);
							i1 = (int)(Screen.GetWorkingArea(this).Width / 100.0 * Math.Abs(i1));
						}
						else if (s1.Length > Screen.GetWorkingArea(this).Width)
						{
							i1 = Screen.GetWorkingArea(this).Width;
						}
						else if (s1.Length <= 0)
						{
							i1 = Width;
						}
						else
						{
							_ = int.TryParse(s1, out i1);
						}
						// Height
						s1 = RtnCnvMacroVar(aOp[2], 0);
						if (Regex.IsMatch(s1, "\\d+%"))
						{
							_ = int.TryParse(s1.Replace("%", ""), out i2);
							i2 = (int)(Screen.GetWorkingArea(this).Height / 100.0 * Math.Abs(i2));
						}
						else if (s1.Length > Screen.GetWorkingArea(this).Height)
						{
							i2 = Screen.GetWorkingArea(this).Height;
						}
						else if (s1.Length <= 0)
						{
							i2 = Height;
						}
						else
						{
							_ = int.TryParse(s1, out i2);
						}
						Width = i1;
						Height = i2;
						WindowState = FormWindowState.Normal;
						break;

					// フォームサイズ（最大化）
					case "#sizemax":
						WindowState = FormWindowState.Maximized;
						break;

					// フォームサイズ（最小化）
					case "#sizemin":
						WindowState = FormWindowState.Minimized;
						break;

					// フォームサイズ（普通）
					case "#sizenormal":
						WindowState = FormWindowState.Normal;
						break;

					// タブサイズを変更
					case "#tabsize":
						_ = decimal.TryParse(aOp[1], out GblNudTabSizeAfter);
						NudTabSize_ValueChanged(null, null);
						break;

					// フォントサイズを変更
					case "#fontsize":
						_ = decimal.TryParse(aOp[1], out GblNudFontSizeAfter);
						NudFontSize_ValueChanged(null, null);
						break;

					// 背景色を変更
					case "#bgcolor":
						SubBgColorChange(aOp[1]);
						break;

					// 背景色
					case "#color?":
						sb.Clear();
						foreach (PropertyInfo info in typeof(Color).GetProperties(BindingFlags.Public | BindingFlags.Static))
						{
							Color color = (Color)info.GetValue(null, null);
							_ = sb.Append($"#{color.R:X2}{color.G:X2}{color.B:X2}   {color.Name}{NL}");
						}
						TbResult.AppendText(sb.ToString());
						break;

					// マクロ
					case "#macro?":
						_ = sb.Clear();
						_ = sb.Append(
							"----------" + NL +
							"> マクロ <" + NL +
							"----------" + NL +
							"※大文字・小文字を区別しない。(例) #stream と #STREAM は同じ。" + NL +
							NL +
							"[マクロ]            [説明]"
						);
						for (int _i1 = 0; _i1 < AryDgvMacro.Length; _i1 += 5)
						{
							_ = sb.Append(NL);
							_ = sb.Append(string.Format(" {0,-20}{1}", AryDgvMacro[_i1], AryDgvMacro[_i1 + 1]));
						}
						_ = sb.Append(
							NL + NL +
							"--------------" + NL +
							"> マクロ変数 <" + NL +
							"--------------" + NL +
							"※大文字・小文字を区別しない。(例) #{:now} と #{:NOW} は同じ。" + NL
						);
						for (int _i1 = 0; _i1 < AryDgvMacroVar.Length; _i1 += 2)
						{
							_ = sb.Append(NL);
							_ = sb.Append(string.Format(" {0,-20}{1}", AryDgvMacroVar[_i1], AryDgvMacroVar[_i1 + 1]));
						}
						TbResult.Paste(sb.ToString() + NL);
						break;

					// 操作説明
					case "#help":
						TbResult.Paste(TbCmdHelp + NL);
						break;

					// バージョン
					case "#version":
						TbResult.AppendText(COPYRIGHT + NL + VERSION + NL);
						break;

					// 終了
					case "#exit":
						Application.Exit();
						break;
				}
			}
			// コマンド実行
			else
			{
				PS = new Process();
				PS.StartInfo.UseShellExecute = false;
				PS.StartInfo.RedirectStandardInput = true;
				PS.StartInfo.RedirectStandardOutput = true;
				PS.StartInfo.RedirectStandardError = true;
				PS.StartInfo.CreateNoWindow = true;
				PS.StartInfo.FileName = "cmd.exe";

				// ^ キャレット付与
				s1 = RtnCnvMacroVar(sCmd, 0);
				s1 = "/c \"" + Regex.Replace(s1, "([\"&\\^\\|<>])", "^$1") + "\"";
				PS.StartInfo.Arguments = s1;

				string stdout932 = "";
				string stderr932 = "";
				string stdout65001 = "";
				string stderr65001 = "";

				// TopMost
				bool bTopMost = TopMost;
				if (bTopMost)
				{
					ChkTopMost.Checked = TopMost = false;
				}

				try
				{
					// CP932 で読込
					PS.StartInfo.StandardOutputEncoding = PS.StartInfo.StandardErrorEncoding = Encoding.GetEncoding(932);
					_ = PS.Start();
					PS.StandardInput.Close();
					stdout932 = RtnTbResultFormat(PS.StandardOutput.ReadToEnd());
					stderr932 = RtnTbResultFormat(PS.StandardError.ReadToEnd());
					PS.Close();
					if (stdout932.Length == 0)
					{
						throw new Exception("実行プロセスから戻り値がない。");
					}

					// CP65001 で読込
					PS.StartInfo.StandardOutputEncoding = PS.StartInfo.StandardErrorEncoding = Encoding.GetEncoding(65001);
					_ = PS.Start();
					PS.StandardInput.Close();
					stdout65001 = RtnTbResultFormat(PS.StandardOutput.ReadToEnd());
					stderr65001 = RtnTbResultFormat(PS.StandardError.ReadToEnd());
					PS.Close();
				}
				catch
				{
					PS.Close();
					bCmd = false;
				}

				// TopMost
				if (bTopMost)
				{
					ChkTopMost.Checked = TopMost = true;
				}

				///MessageBox.Show(
				///	"stdout932   : " + stdout932.Length.ToString() + NL +
				///	"stdout65001 : " + stdout65001.Length.ToString()
				///);

				_ = SbStdOut.Clear();
				_ = SbStdErr.Clear();

				// 文字コードを推測
				if (stdout932.Length < stdout65001.Length)
				{
					_ = SbStdOut.Append(stdout932);
					_ = SbStdErr.Append(stderr932);
				}
				else
				{
					_ = SbStdOut.Append(stdout65001);
					_ = SbStdErr.Append(stderr65001);
				}

				TbResult.AppendText(SbStdOut.ToString());
				TbResult.AppendText(SbStdErr.ToString());
			}

			TbResult.ScrollToCaret();

			// 実行が成功したとき
			if (bCmd)
			{
				// コマンド履歴に追加／[↑][↓]
				SubTbCmdHistory_Set(sCmd);

				// マクロ／コマンド履歴に追加／[F1]
				GblListCmdHistory.Add(sCmd);

				// 出力履歴に追加
				if (TbResult.TextLength > 0)
				{
					SubDictResultHistory_Add();
				}
			}

			SubLblWaitOn(false);
		}

		//--------------------------------------------------------------------------------
		// BtnCmdExecUndo
		//--------------------------------------------------------------------------------
		private string GblCmdExecOld = "";
		private string GblCmdExecNew = "";

		private void BtnCmdExecUndo_Click(object sender, EventArgs e)
		{
			TbResult.Text = TbResult.Text == GblCmdExecNew ? GblCmdExecOld : GblCmdExecNew;
			TbResult.SelectionStart = TbResult.TextLength;
			TbResult.ScrollToCaret();
			SubTbCmdFocus(GblTbCmdPos);
		}

		//--------------------------------------------------------------------------------
		// 出力クリア
		//--------------------------------------------------------------------------------
		private void BtnClear_Click(object sender, EventArgs e)
		{
			TbResult.Text = "";
			SubTbCmdFocus(GblTbCmdPos);
			GC.Collect();
		}

		//--------------------------------------------------------------------------------
		// TbResult
		//--------------------------------------------------------------------------------
		private void CmsResult_Closed(object sender, ToolStripDropDownClosedEventArgs e)
		{
			// 一度カーソルを外さないと表示が消えてしまう
			_ = TbCurDir.Focus();
			// ちらつきを防止
			TbCurDir.Select(0, 0);
			// 再フォーカス
			_ = TbResult.Focus();
		}

		private void TbResult_Enter(object sender, EventArgs e)
		{
			TmpCtrlName = TbResult;

			LblResult.Visible = true;
			Lbl_F8.ForeColor = Color.WhiteSmoke;

			TbResult.Select(TbResult.SelectionStart, 0);
			if (GblLblTooltipVisible)
			{
				SubLblTooltip(LblTooltip_TbResult);
			}
		}

		private void TbResult_Leave(object sender, EventArgs e)
		{
			Text = "";

			LblResult.Visible = false;
			Lbl_F8.ForeColor = Color.Gray;

			GblLblTooltipVisible = LblTooltip.Visible;
			LblTooltip.Visible = false;

			if (TbResult.TextLength > 0)
			{
				SubDictResultHistory_Add();
			}
		}

		private void SubDictResultHistory_Add()
		{
			bool bDiff = true;
			// 異なるデータのみ追加
			foreach (KeyValuePair<string, string> _dict in GblDictResultHistory)
			{
				if (_dict.Value == TbResult.Text)
				{
					bDiff = false;
					break;
				}
			}
			if (bDiff)
			{
				// Key重複回避のため一応遅延
				Thread.Sleep(10);
				GblDictResultHistory.Add(DateTime.Now.ToString("HH:mm:ss.ff"), TbResult.Text);
				// 履歴は10件
				if (GblDictResultHistory.Count > 10)
				{
					_ = GblDictResultHistory.Remove(GblDictResultHistory.First().Key);
				}
			}
		}

		private void TbResult_KeyDown(object sender, KeyEventArgs e)
		{
			// タイトル表示
			Text = TextDefault;

			// [Ctrl]+[Back]
			if (e.KeyData == (Keys.Control | Keys.Back))
			{
				TbResult.Select(0, TbResult.SelectionStart);
				TbResult.Cut();
				return;
			}

			// [Ctrl]+[Delete]
			if (e.KeyData == (Keys.Control | Keys.Delete))
			{
				TbResult.Select(TbResult.SelectionStart, TbResult.TextLength);
				TbResult.Cut();
				return;
			}

			// [Ctrl]+[A]
			if (e.KeyData == (Keys.Control | Keys.A))
			{
				TbResult.SelectAll();
				return;
			}

			// [Ctrl]+[C]
			if (e.KeyData == (Keys.Control | Keys.C))
			{
				TbResult.Copy();
				return;
			}

			// [Ctrl]+[S]
			if (e.KeyData == (Keys.Control | Keys.S))
			{
				Cursor.Position = new Point(Left + ((Width - CmsResult.Width) / 2), Top + SystemInformation.CaptionHeight + TbResult.Top + 20);
				CmsResult.Show(Cursor.Position);
				CmsResult_名前を付けて保存.Select();
				SendKeys.Send("{RIGHT}");
				return;
			}

			// [Ctrl]+[V]
			if (e.KeyData == (Keys.Control | Keys.V))
			{
				TbResult.Paste(Regex.Replace(Clipboard.GetText(), RgxNL, NL));
				return;
			}

			// [Ctrl]+[X]
			if (e.KeyData == (Keys.Control | Keys.X))
			{
				TbResult.Cut();
				return;
			}

			// [Ctrl]+[Z]
			if (e.KeyData == (Keys.Control | Keys.Z))
			{
				if (TbResult.CanUndo)
				{
					TbResult.Undo();
					TbResult.ClearUndo();
				}
				return;
			}
		}

		private void TbResult_KeyPress(object sender, KeyPressEventArgs e)
		{
			// ビープ音抑制
			//   [Ctrl] で縛りをかけると [Ctrl]+[キー] を自前で追記する必要がある
			if ((ModifierKeys & Keys.Control) == Keys.Control)
			{
				e.Handled = true;
			}
		}

		private void TbResult_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.Escape:
					SubTbCmdFocus(GblTbCmdPos);
					return;

				case Keys.F4:
					if (GblLblTooltipVisible)
					{
						LblTooltip.Visible = GblLblTooltipVisible = false;
					}
					else
					{
						SubLblTooltip(LblTooltip_TbResult);
					}
					return;

				case Keys.F8:
					TmpCtrlName = TbResult;
					CbResultHistory.DroppedDown = true;
					_ = CbResultHistory.Focus();
					return;

				case Keys.F9:
					SubTbCmdFocus(GblTbCmdPos);
					return;

				case Keys.F11:
					SubTbResultForward();
					return;

				case Keys.F12:
					SubTbResultNext();
					return;
			}
		}

		private void TbResult_MouseDown(object sender, MouseEventArgs e)
		{
			_ = TbResult.Focus();

			switch (e.Button)
			{
				case MouseButtons.Right:
					TbResult.ContextMenuStrip = TbResult.SelectedText.Length > 0 ? CmsTextSelect : CmsResult;
					return;
			}
		}

		private void TbResult_MouseUp(object sender, MouseEventArgs e)
		{
			switch (e.Button)
			{
				case MouseButtons.Left:
					SubCmsTextSelect_Opened();
					SubTbResultCnt();
					return;

				case MouseButtons.Middle:
					return;

				case MouseButtons.Right:
					SubCmsTextSelect_Opened();
					return;
			}
		}

		private void TbResult_DragEnter(object sender, DragEventArgs e)
		{
			// ドロップ不可
			e.Effect = DragDropEffects.None;
			ScrTbResult.Visible = true;
		}

		private string RtnTbResultFormat(string str)
		{
			//   "\n" => "\r\n"
			str = Regex.Replace(str, RgxNL, NL);
			//   ESC（\u001B, \033, \x1b）は除外
			return Regex.Replace(str, "\u001B\\[(.+?)[A-Za-z]", "");
		}

		private void SubTbResultCnt()
		{
			if (TbResult.SelectionLength > 0)
			{
				int iY = 1;
				int i1 = 0;
				char cs1 = '\0';
				foreach (char _cs1 in TbResult.SelectedText.ToCharArray())
				{
					if (_cs1 == '\n')
					{
						++iY;
					}
					// "\n\r\n" のとき空白行
					else if (cs1 == '\n' && _cs1 == '\r')
					{
						++i1;
					}
					cs1 = _cs1;
				}
				// 改行は "\r\n" なので ('\n' * 2) を引いた数が改行数
				int iX = TbResult.SelectionLength - ((iY - 1) * NL.Length);
				Text = $"全{iY}行  有効{iY - i1}行  {iX}文字";
			}
			else
			{
				int iY = 1;
				int i1 = 0;
				int i2 = 0;
				foreach (char _cs1 in TbResult.Text.Substring(0, TbResult.SelectionStart).ToCharArray())
				{
					++i1;
					// '\n' で改行
					if (_cs1 == '\n')
					{
						++iY;
						i2 = i1;
					}
				}
				Text = $"{iY}行 {TbResult.SelectionStart - i2 + 1}文字目";
			}
		}

		//--------------------------------------------------------------------------------
		// CmsResult
		//--------------------------------------------------------------------------------
		private void CmsResult_Opened(object sender, EventArgs e)
		{
			CmsResult_出力へコピー_1.Visible = CmsResult_出力へコピー_2.Visible = CmsResult_出力へコピー_3.Visible = CmsResult_出力へコピー_4.Visible = CmsResult_出力へコピー_5.Visible = true;

			switch (GblAryResultIndex)
			{
				case 1:
					CmsResult_出力へコピー_1.Visible = false;
					break;

				case 2:
					CmsResult_出力へコピー_2.Visible = false;
					break;

				case 3:
					CmsResult_出力へコピー_3.Visible = false;
					break;

				case 4:
					CmsResult_出力へコピー_4.Visible = false;
					break;

				case 5:
					CmsResult_出力へコピー_5.Visible = false;
					break;
			}

			_ = TbResult.Focus();
		}

		private void CmsResult_全選択_Click(object sender, EventArgs e)
		{
			_ = TbResult.Focus();
			TbResult.SelectAll();
			SubTbResultCnt();
		}

		private void CmsResult_全クリア_Click(object sender, EventArgs e)
		{
			TbResult.Text = "";
		}

		private void CmsResult_全コピー_Click(object sender, EventArgs e)
		{
			TbResult.SelectAll();
			TbResult.Copy();
		}

		private void CmsResult_全カット_Click(object sender, EventArgs e)
		{
			TbResult.SelectAll();
			TbResult.Cut();
		}

		private void CmsResult_ペースト_Click(object sender, EventArgs e)
		{
			TbResult.Paste(Regex.Replace(Clipboard.GetText(), RgxNL, NL));
		}

		private void CmsResult_ペースト_MouseHover(object sender, EventArgs e)
		{
			ToolTip.SetToolTip(CmsResult, RtnClipboardSubstring());
		}

		private void CmsResult_カーソルの前方をクリア_Click(object sender, EventArgs e)
		{
			TbResult.Select(0, TbResult.SelectionStart);
			TbResult.SelectedText = "";
		}

		private void CcmsResult_カーソルの後方をクリア_Click(object sender, EventArgs e)
		{
			TbResult.Select(TbResult.SelectionStart, TbResult.TextLength - TbResult.SelectionStart);
			TbResult.SelectedText = "";
		}


		private void CmsResult_出力へコピー_1_Click(object sender, EventArgs e)
		{
			SubCmsResultCopyTo(1);
		}

		private void CmsResult_出力へコピー_2_Click(object sender, EventArgs e)
		{
			SubCmsResultCopyTo(2);
		}

		private void CmsResult_出力へコピー_3_Click(object sender, EventArgs e)
		{
			SubCmsResultCopyTo(3);
		}

		private void CmsResult_出力へコピー_4_Click(object sender, EventArgs e)
		{
			SubCmsResultCopyTo(4);
		}

		private void CmsResult_出力へコピー_5_Click(object sender, EventArgs e)
		{
			SubCmsResultCopyTo(5);
		}

		private void SubCmsResultCopyTo(int iIndex)
		{
			if (RtnAryResultBtnRangeChk(iIndex))
			{
				GblAryResultBuf[iIndex] = TbResult.Text;
				// 表示色を更新
				SubTbResultChange(GblAryResultIndex, TbResult);
			}
		}

		private void CmsResult_名前を付けて保存_SJIS_Click(object sender, EventArgs e)
		{
			SubCmsResult_名前を付けて保存("932");
		}

		private void CmsResult_名前を付けて保存_UTF8_Click(object sender, EventArgs e)
		{
			SubCmsResult_名前を付けて保存("65001bom");
		}

		private void CmsResult_名前を付けて保存_UTF8N_Click(object sender, EventArgs e)
		{
			SubCmsResult_名前を付けて保存("65001nobom");
		}

		private string GblCmsResultFn = "";

		private void SubCmsResult_名前を付けて保存(string encode)
		{
			string fn = (GblCmsResultFn.Length == 0 ? "" : Path.GetDirectoryName(GblCmsResultFn) + "\\") + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".txt";
			if (RtnTextFileWrite(TbResult.Text, encode, fn, true, TEXT_FILTER))
			{
				GblCmsResultFn = fn;
			}
		}

		//--------------------------------------------------------------------------------
		// BtnResult
		//--------------------------------------------------------------------------------
		private Color BtnResultBackColor;
		private readonly Color BtnResultBackColorFocus = Color.Crimson;

		private void BtnResult1_MouseEnter(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 1)
			{
				BtnResultBackColor = BtnResult1.BackColor;
				BtnResult1.BackColor = BtnResultBackColorFocus;
			}
		}

		private void BtnResult1_MouseLeave(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 1)
			{
				BtnResult1.BackColor = BtnResultBackColor;
			}
		}
		private void BtnResult1_Click(object sender, EventArgs e)
		{
			SubTbResultChange(1, TbCmd);
		}

		private void BtnResult2_MouseEnter(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 2)
			{
				BtnResultBackColor = BtnResult2.BackColor;
				BtnResult2.BackColor = BtnResultBackColorFocus;
			}
		}

		private void BtnResult2_MouseLeave(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 2)
			{
				BtnResult2.BackColor = BtnResultBackColor;
			}
		}

		private void BtnResult2_Click(object sender, EventArgs e)
		{
			SubTbResultChange(2, TbCmd);
		}

		private void BtnResult3_MouseEnter(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 3)
			{
				BtnResultBackColor = BtnResult3.BackColor;
				BtnResult3.BackColor = BtnResultBackColorFocus;
			}
		}

		private void BtnResult3_MouseLeave(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 3)
			{
				BtnResult3.BackColor = BtnResultBackColor;
			}
		}

		private void BtnResult3_Click(object sender, EventArgs e)
		{
			SubTbResultChange(3, TbCmd);
		}

		private void BtnResult4_MouseEnter(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 4)
			{
				BtnResultBackColor = BtnResult4.BackColor;
				BtnResult4.BackColor = BtnResultBackColorFocus;
			}
		}

		private void BtnResult4_MouseLeave(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 4)
			{
				BtnResult4.BackColor = BtnResultBackColor;
			}
		}

		private void BtnResult4_Click(object sender, EventArgs e)
		{
			SubTbResultChange(4, TbCmd);
		}

		private void BtnResult5_MouseEnter(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 5)
			{
				BtnResultBackColor = BtnResult5.BackColor;
				BtnResult5.BackColor = BtnResultBackColorFocus;
			}
		}

		private void BtnResult5_MouseLeave(object sender, EventArgs e)
		{
			if (GblAryResultIndex != 5)
			{
				BtnResult5.BackColor = BtnResultBackColor;
			}
		}

		private void BtnResult5_Click(object sender, EventArgs e)
		{
			SubTbResultChange(5, TbCmd);
		}

		// [0] = null, [1..5] = BtnResult..
		private const int GblAryResultMin = 1;
		private const int GblAryResultMax = 5;
		private int GblAryResultIndex = GblAryResultMin;
		private readonly string[] GblAryResultBuf = { "", "", "", "", "", "" };
		private readonly int[] GblAryResultStartIndex = { 0, 0, 0, 0, 0, 0 };

		private bool RtnAryResultBtnRangeChk(int index)
		{
			return index >= GblAryResultMin && index <= GblAryResultMax;
		}

		private void SubTbResultChange(int iIndex, Control ctrlName)
		{
			// 選択されたタブへ移動
			if (RtnAryResultBtnRangeChk(iIndex))
			{
				Controls[$"BtnResult{iIndex}"].BackColor = Color.Crimson;
				// 旧タブのデータ保存
				GblAryResultBuf[GblAryResultIndex] = TbResult.Text;
				GblAryResultStartIndex[GblAryResultIndex] = TbResult.SelectionStart;
				// 新タブのデータ読込
				TbResult.Text = GblAryResultBuf[iIndex];
				TbResult.Select(GblAryResultStartIndex[iIndex], 0);
				// 新タブ位置を記憶
				GblAryResultIndex = iIndex;
			}

			// 非選択タブのうちデータのあるタブは色を変える
			for (int _i1 = GblAryResultMin; _i1 <= GblAryResultMax; _i1++)
			{
				if (_i1 != iIndex)
				{
					Controls[$"BtnResult{_i1}"].BackColor = GblAryResultBuf[_i1].Length > 0 ? Color.Maroon : Color.Transparent;
				}
			}

			// 元の場所へカーソル移動
			_ = ctrlName.Focus();
		}

		private void SubTbResultForward()
		{
			int i1 = GblAryResultIndex - 1;
			SubTbResultChange(RtnAryResultBtnRangeChk(i1) ? i1 : GblAryResultMax, TmpCtrlName);
		}

		private void SubTbResultNext()
		{
			int i1 = GblAryResultIndex + 1;
			SubTbResultChange(RtnAryResultBtnRangeChk(i1) ? i1 : GblAryResultMin, TmpCtrlName);
		}

		private void ScrTbResult_Panel1_MouseLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void ScrTbResult_Panel1_Click(object sender, EventArgs e)
		{
			// 誤操作で表示されたままになったとき使用
			ScrTbResult.Visible = false;
		}

		private void ScrTbResult_Panel1_DragLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void ScrTbResult_Panel2_MouseLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void ScrTbResult_Panel2_Click(object sender, EventArgs e)
		{
			// 誤操作で表示されたままになったとき使用
			ScrTbResult.Visible = false;
		}

		private void ScrTbResult_Panel2_DragLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void BtnPasteFilename_MouseLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void BtnPasteFilename_DragEnter(object sender, DragEventArgs e)
		{
			e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
		}

		private void BtnPasteFilename_DragDrop(object sender, DragEventArgs e)
		{
			StringBuilder sb = new StringBuilder();

			foreach (string _s1 in (string[])e.Data.GetData(DataFormats.FileDrop))
			{
				_ = sb.Append(_s1);
				if (Directory.Exists(_s1))
				{
					_ = sb.Append('\\');
				}
				_ = sb.Append(NL);
			}
			TbResult.Paste(sb.ToString());

			if (sb.Length > 0)
			{
				TbResult_Leave(sender, e);
			}

			ScrTbResult.Visible = false;
		}

		private void BtnPasteTextfile_MouseLeave(object sender, EventArgs e)
		{
			ScrTbResult.Visible = false;
		}

		private void BtnPasteTextfile_DragEnter(object sender, DragEventArgs e)
		{
			e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
		}

		private void BtnPasteTextfile_DragDrop(object sender, DragEventArgs e)
		{
			SubLblWaitOn(true);

			StringBuilder sb = new StringBuilder();
			string s1 = "";

			foreach (string _s1 in (string[])e.Data.GetData(DataFormats.FileDrop))
			{
				(string _fn, string _data) = RtnTextFread(_s1, false, "");
				if (_fn.Length > 0)
				{
					_ = sb.Append(_data);
				}
				else
				{
					if (File.Exists(_s1))
					{
						s1 += "・" + Path.GetFileName(_s1) + NL;
					}
				}
			}

			TbResult.Paste(Regex.Replace(sb.ToString(), RgxNL, NL));

			if (sb.Length > 0)
			{
				TbResult_Leave(sender, e);
			}

			SubLblWaitOn(false);
			ScrTbResult.Visible = false;

			if (s1.Length > 0)
			{
				_ = MessageBox.Show(
					"[Err] テキストファイルでないか、他のプロセスで使用中のファイルです。" + NL +
					NL +
					s1,
					VERSION
				);
			}
		}

		//--------------------------------------------------------------------------------
		// 履歴
		//--------------------------------------------------------------------------------
		private readonly List<string> GblListCmdHistory = new List<string>();

		private void CbCmdHistory_Enter(object sender, EventArgs e)
		{
			CbCmdHistory.BackColor = Color.WhiteSmoke;
			Lbl_F1.ForeColor = Color.Gold;

			// GblListCmdHistory 空白削除
			_ = GblListCmdHistory.RemoveAll(s => s.Length == 0);

			// CbCmdHistory 再編成
			CbCmdHistory.Items.Clear();

			// CbCmdHistory １行目は空白
			_ = CbCmdHistory.Items.Add("");

			foreach (string _s1 in GblListCmdHistory)
			{
				_ = CbCmdHistory.Items.Add(_s1);
			}

			CbCmdHistory.SelectedIndex = 0;

			int i1 = 0;

			foreach (string _s1 in GblListCmdHistory)
			{
				++i1;

				if (_s1 == TbCmd.Text)
				{
					CbCmdHistory.SelectedIndex = i1;
					break;
				}
			}
		}

		private void CbCmdHistory_Leave(object sender, EventArgs e)
		{
			CbCmdHistory.BackColor = BackColor;
			Lbl_F1.ForeColor = Color.Gray;
		}

		private void CbCmdHistory_DropDownClosed(object sender, EventArgs e)
		{
			// "" 選択のときは何もしない
			if (CbCmdHistory.Text.Length > 0)
			{
				string s1 = CbCmdHistory.Text;

				// [Ctrl]+ のときは挿入モード／それ以外は上書き
				if ((ModifierKeys & Keys.Control) == Keys.Control)
				{
					TbCmd.Paste(s1 + ";");
					GblTbCmdPos = TbCmd.SelectionStart + s1.Length + 1;
				}
				else
				{
					TbCmd.Text = s1;
					GblTbCmdPos = s1.Length;
				}
			}

			CbCmdHistory.Text = "";
			SubTbCmdFocus(GblTbCmdPos);
		}

		private void CbCmdHistory_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.F1:
				case Keys.Space:
					// 自身を閉じる
					CbCmdHistory.DroppedDown = false;
					return;
			}
		}

		private readonly SortedDictionary<string, string> GblDictResultHistory = new SortedDictionary<string, string>();

		private void CbResultHistory_Enter(object sender, EventArgs e)
		{
			CbResultHistory.BackColor = Color.WhiteSmoke;
			Lbl_F8.ForeColor = Color.Gold;

			// CbResultHistory を再構成
			CbResultHistory.Items.Clear();
			_ = CbResultHistory.Items.Add("");

			foreach (KeyValuePair<string, string> _dict in GblDictResultHistory)
			{
				string _s1 = _dict.Value.Substring(0, _dict.Value.Length < 80 ? _dict.Value.Length : 80).TrimStart();
				_ = CbResultHistory.Items.Add($"{_dict.Key}  {_s1.Replace(NL, "　")}");
			}

			CbResultHistory.SelectedIndex = 0;

			int i1 = 0;

			foreach (KeyValuePair<string, string> _dict in GblDictResultHistory)
			{
				++i1;

				if (_dict.Value == TbResult.Text)
				{
					CbResultHistory.SelectedIndex = i1;
					break;
				}
			}
		}

		private void CbResultHistory_Leave(object sender, EventArgs e)
		{
			CbResultHistory.BackColor = BackColor;
			Lbl_F8.ForeColor = Color.Gray;
		}

		private void CbResultHistory_DropDownClosed(object sender, EventArgs e)
		{
			// "" 選択のときは何もしない
			if (CbResultHistory.Text.Length > 0)
			{
				// "HH:mm:ss.ff".Length => 11
				TbResult.Text = GblDictResultHistory[CbResultHistory.Text.Substring(0, 11)];
				// TbResult.SelectionStart = NUM 不可
				TbResult.ScrollToCaret();
			}

			CbResultHistory.Text = "";

			switch (TmpCtrlName.Name)
			{
				case "TbCmd":
					SubTbCmdFocus(GblTbCmdPos);
					return;

				case "TbResult":
					TbResult.Focus();
					return;
			}
		}

		private void CbResultHistory_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.F8:
				case Keys.Space:
					// 自身を閉じる
					CbResultHistory.DroppedDown = false;
					return;
			}
		}

		//--------------------------------------------------------------------------------
		// タブサイズ
		//--------------------------------------------------------------------------------
		private void NudTabSize_ValueChanged(object sender, EventArgs e)
		{
			// SystemInformation.MouseWheelScrollLines 対策のため独自実装
			if (GblNudTabSizeAfter == 0)
			{
				GblNudTabSizeAfter = NudTabSize.Value;
			}
			else if (GblNudTabSizeAfter > NudTabSize.Value || GblNudTabSizeAfter < NudTabSize.Value)
			{
				if (GblNudTabSizeAfter > NudTabSize.Maximum)
				{
					GblNudTabSizeAfter = NudTabSize.Maximum;
				}
				else if (GblNudTabSizeAfter < NudTabSize.Minimum)
				{
					GblNudTabSizeAfter = NudTabSize.Minimum;
				}
			}

			_ = SendMessage(TbResult.Handle, EM_SETTABSTOPS, 1, new int[] { (int)(GblNudTabSizeAfter * 4) });

			NudTabSize.Value = GblNudTabSizeAfter;
		}

		private void NudTabSize_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.Escape:
					SubTbCmdFocus(GblTbCmdPos);
					return;
			}
		}

		private decimal GblNudTabSizeAfter = 0;

		private void NudTabSize_MouseDown(object sender, MouseEventArgs e)
		{
			NudTabSize.Select(0, NudTabSize.Value.ToString().Length);

			GblNudTabSizeAfter = 0;
		}

		private void NudTabSize_MouseWheel(object sender, MouseEventArgs e)
		{
			// 上回転 +120
			if (e.Delta > 0)
			{
				GblNudTabSizeAfter = NudTabSize.Value + 4;
			}
			// 下回転 -120
			else if (e.Delta < 0)
			{
				GblNudTabSizeAfter = NudTabSize.Value - 4;
			}
		}

		//--------------------------------------------------------------------------------
		// フォントサイズ
		//--------------------------------------------------------------------------------
		private void NudFontSize_ValueChanged(object sender, EventArgs e)
		{
			// SystemInformation.MouseWheelScrollLines 対策のため独自実装
			if (GblNudFontSizeAfter == 0)
			{
				GblNudFontSizeAfter = NudFontSize.Value;
			}
			else if (GblNudFontSizeAfter > NudFontSize.Value || GblNudFontSizeAfter < NudFontSize.Value)
			{
				if (GblNudFontSizeAfter > NudFontSize.Maximum)
				{
					GblNudFontSizeAfter = NudFontSize.Maximum;
				}
				else if (GblNudFontSizeAfter < NudFontSize.Minimum)
				{
					GblNudFontSizeAfter = NudFontSize.Minimum;
				}
			}

			TbResult.Font = new Font(TbResult.Font.Name.ToString(), (float)GblNudFontSizeAfter);

			NudFontSize.Value = GblNudFontSizeAfter;

			// タブサイズ再調整
			NudTabSize_ValueChanged(sender, e);
		}

		private void NudFontSize_KeyUp(object sender, KeyEventArgs e)
		{
			switch (e.KeyCode)
			{
				case Keys.Escape:
					SubTbCmdFocus(GblTbCmdPos);
					return;
			}
		}

		private decimal GblNudFontSizeAfter = 0;

		private void NudFontSize_MouseDown(object sender, MouseEventArgs e)
		{
			NudFontSize.Select(0, NudFontSize.Value.ToString().Length);

			GblNudFontSizeAfter = 0;
		}

		private void NudFontSize_MouseWheel(object sender, MouseEventArgs e)
		{
			// 上回転 +120
			if (e.Delta > 0)
			{
				GblNudFontSizeAfter = NudFontSize.Value + 5;
			}
			// 下回転 -120
			else if (e.Delta < 0)
			{
				GblNudFontSizeAfter = NudFontSize.Value - 5;
			}
		}

		//--------------------------------------------------------------------------------
		// CmsTextSelect
		//--------------------------------------------------------------------------------
		private void CmsTextSelect_Opened(object sender, EventArgs e)
		{
			Cursor.Position = new Point(Cursor.Position.X + 10, Cursor.Position.Y + 10);
		}

		private void SubCmsTextSelect_Opened()
		{
			switch (ActiveControl)
			{
				case TextBox tb when tb.SelectionLength > 0:
					CmsTextSelect.Show(Cursor.Position);
					return;
			}
		}

		private void CmsTextSelect_PreviewKeyDown(object sender, PreviewKeyDownEventArgs e)
		{
			// 選択キー [Enter] [↑] [↓]
			if (e.KeyCode == Keys.Enter || e.KeyCode == Keys.Up || e.KeyCode == Keys.Down)
			{
				return;
			}

			CmsTextSelect.Close();

			bool bCapsLock = Control.IsKeyLocked(Keys.CapsLock);

			// [A..Z] [a..z]
			if (e.KeyValue >= 65 && e.KeyValue <= 90)
			{
				switch (ActiveControl)
				{
					case TextBox tb:
						tb.SelectedText = bCapsLock ? e.KeyCode.ToString().ToUpper() : e.KeyCode.ToString().ToLower();
						return;
				}
			}

			switch (e.KeyCode)
			{
				// 削除
				case Keys.Delete:
				case Keys.Back:
					switch (ActiveControl)
					{
						case TextBox tb:
							tb.SelectedText = "";
							break;
					}
					return;
			}
		}

		private void CmsTextSelect_クリア_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = "";
					return;
			}
		}

		private void CmsTextSelect_コピー_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.Copy();
					return;
			}
		}

		private void CmsTextSelect_カット_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.Cut();
					return;
			}
		}

		private void CmsTextSelect_ペースト_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.Paste();
					return;
			}
		}

		private void CmsTextSelect_ペースト_MouseHover(object sender, EventArgs e)
		{
			ToolTip.SetToolTip(CmsTextSelect, RtnClipboardSubstring());
		}

		private void CmsTextSelect_DQで囲む_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = $"\"{RtnEraseDQ(tb.SelectedText)}\"";
					return;
			}
		}

		private void CmsTextSelect_YenDQで囲む_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = $"\\\"{RtnEraseDQ(tb.SelectedText)}\\\"";
					return;
			}
		}

		private void CmsTextSelect_DQを消去_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = RtnEraseDQ(tb.SelectedText);
					return;
			}
		}

		private void CmsTextSelect_YenYenに置換_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = tb.SelectedText.Replace("\\", "\\\\");
					return;
			}
		}

		private void CmsTextSelect_Yenに置換_Click(object sender, EventArgs e)
		{
			switch (ActiveControl)
			{
				case TextBox tb:
					tb.SelectedText = tb.SelectedText.Replace("\\\\", "\\");
					return;
			}
		}

		private string RtnEraseDQ(string str)
		{
			// ", \" を消去
			return Regex.Replace(str, "^(\"|\\\\\")+(.*?)(\"|\\\\\")+$", "$2");
		}

		private void CmsTextSelect_ネット検索_URLを開く_Click(object sender, EventArgs e)
		{
			CmsTextSelect_関連付けられたアプリケーションで開く_Click(sender, e);
		}

		private void CmsTextSelect_ネット検索_Google_Click(object sender, EventArgs e)
		{
			SubNetSearch("https://www.google.co.jp/search?q=");
		}

		private void CmsTextSelect_ネット検索_Google翻訳_Click(object sender, EventArgs e)
		{
			SubNetSearch("https://translate.google.com/?hl=ja&sl=auto&tl=ja&op=translate&text=");
		}

		private void CmsTextSelect_ネット検索_Googleマップ_Click(object sender, EventArgs e)
		{
			SubNetSearch("https://www.google.co.jp/maps/place/");
		}

		private void CmsTextSelect_ネット検索_YouTube_Click(object sender, EventArgs e)
		{
			SubNetSearch("https://www.youtube.com/results?search_query=");
		}

		private void CmsTextSelect_ネット検索_Wikipedia_Click(object sender, EventArgs e)
		{
			SubNetSearch("https://ja.wikipedia.org/wiki/");
		}

		private void SubNetSearch(string url)
		{
			string s1 = "";
			switch (ActiveControl)
			{
				case TextBox tb:
					s1 = tb.SelectedText;
					break;
			}
			_ = Process.Start(url + HttpUtility.UrlEncode(s1.Replace("\n", " ")));
		}

		private void CmsTextSelect_関連付けられたアプリケーションで開く_Click(object sender, EventArgs e)
		{
			string s1 = "";
			switch (ActiveControl)
			{
				case TextBox tb:
					s1 = tb.SelectedText;
					break;
			}
			foreach (string _s1 in Regex.Split(s1.TrimEnd(), RgxNL))
			{
				try
				{
					_ = Process.Start(_s1.Trim());
				}
				catch
				{
				}
			}
		}

		//--------------------------------------------------------------------------------
		// TbCmd へフォーカス
		//--------------------------------------------------------------------------------
		private void SubTbCmdFocus(int iPos)
		{
			if (iPos < 0 || iPos > TbCmd.TextLength)
			{
				iPos = TbCmd.TextLength;
			}
			TbCmd.Select(iPos, 0);
			_ = TbCmd.Focus();
			LblCmd.Visible = true;
			LblCurDir.Visible = LblResult.Visible = false;
		}

		//--------------------------------------------------------------------------------
		// LblWait
		//--------------------------------------------------------------------------------
		private void SubLblWaitOn(bool bOn)
		{
			if (bOn)
			{
				Cursor.Current = Cursors.WaitCursor;
				LblWait.Visible = true;
				Refresh();
			}
			else
			{
				LblWait.Visible = false;
				Cursor.Current = Cursors.Default;
			}
		}

		//--------------------------------------------------------------------------------
		// エスケープ文字に置換
		//--------------------------------------------------------------------------------
		private string RtnCnvEscVar(string str)
		{
			try
			{
				// 以下、順序を違わぬように!!
				// エスケープ文字に置換 (例)"\\" => '\\'
				// Dir/File 使用時には要注意
				str = Regex.Unescape(str);
				// 改行リフォーマット
				str = Regex.Replace(str, RgxNL, NL);
			}
			catch
			{
			}
			return str;
		}

		//--------------------------------------------------------------------------------
		// マクロ変数の置換
		//--------------------------------------------------------------------------------
		private string RtnCnvMacroVar(string cmd, int iLine)
		{
			if (cmd == null)
			{
				cmd = "";
			}
			string s1, s2, s3;
			// 行番号を取得
			if (iLine > 0)
			{
				foreach (Match _m1 in new Regex("(?<v1>#{+?)(?<v2>line.*?)}", RegexOptions.IgnoreCase).Matches(cmd))
				{
					s1 = _m1.Groups["v1"].Value;
					s2 = _m1.Groups["v2"].Value;
					string[] _a1 = s2.Split(',');
					int _iZero = 0;
					if (_a1.Length > 1)
					{
						_ = int.TryParse(_a1[1], out _iZero);
					}
					if (_a1.Length > 2)
					{
						_ = int.TryParse(_a1[2], out int _i1);
						iLine += _i1;
					}
					cmd = cmd.Replace($"{s1}{s2}}}", string.Format($"{{0:D{_iZero}}}", iLine));
				}
			}
			// 各種変換
			if (cmd.IndexOf("#{") >= 0)
			{
				// ASCIIコードを文字に変換
				// (例) #{&65} => 'A'
				foreach (Match _m1 in new Regex("(?<v1>#{&+?)(?<v2>\\d+?)}").Matches(cmd))
				{
					s1 = _m1.Groups["v1"].Value;
					s2 = _m1.Groups["v2"].Value;
					cmd = cmd.Replace($"{s1}{s2}}}", ((char)Convert.ToInt32(s2)).ToString());
				}
				// 日時変数
				DateTime dt = DateTime.Now;
				cmd = Regex.Replace(cmd, "#{:now}", dt.ToString("yyyyMMdd_HHmmss_fff"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:ymd}", dt.ToString("yyyyMMdd"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:hns}", dt.ToString("HHmmss"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:msec}", dt.ToString("fff"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:y}", dt.ToString("yyyy"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:m}", dt.ToString("MM"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:d}", dt.ToString("dd"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:h}", dt.ToString("HH"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:n}", dt.ToString("mm"), RegexOptions.IgnoreCase);
				cmd = Regex.Replace(cmd, "#{:s}", dt.ToString("ss"), RegexOptions.IgnoreCase);
				// 一時変数
				// (例) #set "JAPAN" "日本" => #{:JAPAN} => "日本"
				foreach (Match _m1 in new Regex("(?<v1>#{:+?)(?<v2>.+?)}").Matches(cmd))
				{
					s1 = _m1.Groups["v1"].Value;
					s2 = _m1.Groups["v2"].Value;
					if (DictTmpVar.TryGetValue(s2, out string value))
					{
						cmd = cmd.Replace($"{s1}{s2}}}", value);
					}
				}
				// 環境変数
				// (例) #{%PATH}
				foreach (Match _m1 in new Regex("(?<v1>#{%+?)(?<v2>\\S+?)}").Matches(cmd))
				{
					s1 = _m1.Groups["v1"].Value;
					s2 = _m1.Groups["v2"].Value;
					s3 = Environment.GetEnvironmentVariable(s2);
					if (s3 != null)
					{
						cmd = cmd.Replace($"{s1}{s2}}}", s3);
					}
				}
			}
			return cmd;
		}

		//--------------------------------------------------------------------------------
		// 正規表現による検索
		//--------------------------------------------------------------------------------
		private string RtnTextGrep(string str, string sSearch, RegexOptions RgxOpt, bool bGrep)
		{
			if (sSearch.Length == 0)
			{
				sSearch = "^$";
			}
			sSearch = RtnCnvMacroVar(sSearch, 0);
			StringBuilder sb = new StringBuilder();
			foreach (string _s1 in Regex.Split(str.TrimEnd(), RgxNL))
			{
				// 正規表現文法エラーはないか？
				try
				{
					if (Regex.IsMatch(_s1, sSearch, RgxOpt) == bGrep)
					{
						_ = sb.Append(_s1);
						_ = sb.Append(NL);
					}
				}
				catch
				{
					return str;
				}
			}
			return sb.ToString();
		}

		//--------------------------------------------------------------------------------
		// 正規表現による検索（出現回数指定）
		//--------------------------------------------------------------------------------
		private string RtnTextGreps(string str, string sSearch, string sTimes, RegexOptions RgxOpt)
		{
			if (sSearch.Length == 0)
			{
				return str;
			}
			sSearch = RtnCnvMacroVar(sSearch, 0);
			StringBuilder sb = new StringBuilder();
			string[] aTimes = sTimes.Split(',');
			int iTimesBgn;
			int iTimesEnd;
			// iTimesBgn 以上、iTimesEnd 以下
			if (aTimes.Length > 1)
			{
				_ = int.TryParse(aTimes[0], out iTimesBgn);
				_ = int.TryParse(aTimes[1], out iTimesEnd);
				// iTimesBgn 以上（Max以下）
				if (iTimesEnd == 0)
				{
					iTimesEnd = str.Length;
				}
			}
			// 一致
			else
			{
				_ = int.TryParse(aTimes[0], out iTimesBgn);
				iTimesEnd = iTimesBgn;
			}
			foreach (string _s1 in Regex.Split(str.TrimEnd(), RgxNL))
			{
				// 正規表現文法エラーはないか？
				try
				{
					int _i1 = Regex.Split(_s1.TrimEnd(), sSearch, RgxOpt).Length - 1;
					if (_s1.Trim().Length > 0 && _i1 >= iTimesBgn && _i1 <= iTimesEnd)
					{
						_ = sb.Append(_s1);
						_ = sb.Append(NL);
					}
				}
				catch
				{
					return str;
				}
			}
			return sb.ToString();
		}

		//--------------------------------------------------------------------------------
		// 正規表現による抽出
		//--------------------------------------------------------------------------------
		private string RtnTextExtract(string str, string sSearch, RegexOptions RgxOpt)
		{
			// 正規表現文法エラーはないか？
			try
			{
				StringBuilder sb = new StringBuilder();
				foreach (Match _m1 in new Regex($"(?<v1>{sSearch.Trim()})", RgxOpt).Matches(str))
				{
					_ = sb.Append(_m1.Groups["v1"].Value.Trim());
					_ = sb.Append(NL);
				}
				return sb.ToString();
			}
			catch
			{
				return str;
			}
		}

		//--------------------------------------------------------------------------------
		// head／tail コマンドライク
		//--------------------------------------------------------------------------------
		private string RtnTextGetRow(string str, int iBgn = 1, int iLine = 0)
		{
			if (str.Length == 0)
			{
				return "";
			}
			string[] a1 = Regex.Split(str.TrimEnd(), RgxNL);
			// [0..]
			if (iBgn > 0)
			{
				--iBgn;
			}
			// [a1.Length]
			else if (iBgn == 0)
			{
				return "";
			}
			// [..(a1.Length-1)]
			else
			{
				iBgn += a1.Length;
			}
			// iLine <= 0 のときは末尾まで読込
			int iEnd = (iLine > 0 ? iBgn + iLine : a1.Length) - 1;
			StringBuilder sb = new StringBuilder();
			for (int _i1 = iBgn; _i1 <= iEnd && _i1 < a1.Length; _i1++)
			{
				if (_i1 >= 0)
				{
					_ = sb.Append(a1[_i1]);
					_ = sb.Append(NL);
				}
			}
			return sb.ToString();
		}

		//--------------------------------------------------------------------------------
		// 正規表現による置換
		//--------------------------------------------------------------------------------
		private string RtnTextReplace(string str, string sOld, string sNew, RegexOptions RgxOpt)
		{
			if (str.Length == 0 || sOld.Length == 0)
			{
				return str;
			}
			// Regex.Replace("12345", "(123)(45)", "$1999$2") のとき
			//   => OK "12399945"
			//   => NG "$199945"
			// "\a" で区切って上記を回避
			sNew = Regex.Replace(sNew, "(\\$[1-9])([0-9])", "$1\a$2");
			sNew = RtnCnvEscVar(sNew);
			StringBuilder sb = new StringBuilder();
			int iLine = 0;
			foreach (string _s1 in Regex.Split(str.TrimEnd(), RgxNL))
			{
				++iLine;
				// SNew 本体は変更しない
				string _s2 = RtnCnvMacroVar(sNew, iLine);
				// 正規表現文法エラーはないか？
				try
				{
					_s2 = Regex.Replace(_s1, sOld, _s2, RgxOpt).Replace("\a", "");
				}
				catch
				{
					return str;
				}
				_ = sb.Append(_s2);
				_ = sb.Append(NL);
			}
			return sb.ToString().TrimEnd() + NL;
		}

		//--------------------------------------------------------------------------------
		// ファイル名に使用できない文字を全角化
		//--------------------------------------------------------------------------------
		private string RtnErrFnToWide(string path)
		{
			return path.Replace("\\", "￥").Replace("/", "／").Replace(":", "：").Replace("*", "＊").Replace("?", "？").Replace("\"", "”").Replace("<", "＜").Replace(">", "＞").Replace("|", "｜");
		}

		//--------------------------------------------------------------------------------
		// ファイル作成・更新・アクセス日時変更
		//--------------------------------------------------------------------------------
		private void SubSetFileTime(string path, string sDateTime, int iDelaySec, string sMenu)
		{
			if (path.Length == 0 || sDateTime.Length == 0)
			{
				return;
			}
			DateTime DT;
			TimeSpan TS;
			try
			{
				// "Now" or "y/m/d h:n:s"
				DT = sDateTime.ToLower() == "now" ? DateTime.Now : DateTime.Parse(sDateTime);
				TS = new TimeSpan(0, 0, 0, iDelaySec);
				sMenu = sMenu.ToLower();
			}
			catch
			{
				return;
			}
			foreach (string _s1 in Regex.Split(path.TrimEnd(), RgxNL))
			{
				if (File.Exists(_s1))
				{
					try
					{
						switch (sMenu)
						{
							// Ctime
							case "c":
								File.SetCreationTime(_s1, DT);
								break;
							// Mtime
							case "m":
								File.SetLastWriteTime(_s1, DT);
								break;
							// Atime
							case "a":
								File.SetLastAccessTime(_s1, DT);
								break;
						}
					}
					catch
					{
					}
					DT += TS;
				}
			}
		}

		//--------------------------------------------------------------------------------
		// 正規表現による分割
		//--------------------------------------------------------------------------------
		private string RtnTextSplit(string str, string sSplit, string sReplace, RegexOptions RgxOpt)
		{
			if (str.Length == 0 || sSplit.Length == 0)
			{
				return str;
			}
			sSplit = RtnCnvMacroVar(sSplit, 0);
			sReplace = RtnCnvEscVar(sReplace);
			StringBuilder sb = new StringBuilder();
			int iLine = 0;
			// 行末の空白行は対象外
			foreach (string _s1 in Regex.Split(str.TrimEnd(), RgxNL))
			{
				++iLine;
				// sReplace 本体は変更しない
				string _sReplace = RtnCnvMacroVar(sReplace, iLine);
				// 正規表現文法エラーはないか？
				try
				{
					if (Regex.IsMatch(_s1, sSplit, RgxOpt))
					{
						string[] a1 = Regex.Split(_s1.TrimEnd(), sSplit, RgxOpt);
						for (int _i1 = 0; _i1 < a1.Length; _i1++)
						{
							_sReplace = _sReplace.Replace($"[{_i1}]", a1[_i1]);
						}
						_ = sb.Append(_sReplace);
						_ = sb.Append(NL);
					}
				}
				catch
				{
					return str;
				}
			}
			// 該当なしの変換子 [n] を削除
			return Regex.Replace(sb.ToString(), "\\[\\d+\\]", "");
		}

		//--------------------------------------------------------------------------------
		// TbResult を結合
		//--------------------------------------------------------------------------------
		private string RtnJoinCopy(string sLine)
		{
			StringBuilder sb = new StringBuilder();
			foreach (string _s1 in sLine.Split(','))
			{
				_ = int.TryParse(_s1, out int _i1);
				if (RtnAryResultBtnRangeChk(_i1))
				{
					_ = sb.Append(GblAryResultBuf[_i1]);
				}
			}
			return sb.ToString();
		}

		//--------------------------------------------------------------------------------
		// TbResult を列結合
		//--------------------------------------------------------------------------------
		private string RtnJoinColumn(string sColumn, string sSeparater)
		{
			List<int> lColumnNumber = new List<int>();
			int iLineMax = 0;
			// #{1} - #{5} のタブ・最大行数を取得
			foreach (string _s1 in sColumn.Split(','))
			{
				_ = int.TryParse(_s1, out int _i1);
				if (RtnAryResultBtnRangeChk(_i1))
				{
					lColumnNumber.Add(_i1);
					int _i2 = Regex.Split(GblAryResultBuf[_i1].TrimEnd(), RgxNL).Length;
					if (_i2 > iLineMax)
					{
						iLineMax = _i2;
					}
				}
			}
			List<string> lColumn = new List<string>();
			// 空リスト作成
			for (int _i1 = 0; _i1 < iLineMax; _i1++)
			{
				lColumn.Add("");
			}
			sSeparater = RtnCnvEscVar(sSeparater);
			sSeparater = RtnCnvMacroVar(sSeparater, 0);
			// 使用されている #{1} - #{5} を取得
			foreach (int _i1 in lColumnNumber)
			{
				string[] _a1 = Regex.Split(GblAryResultBuf[_i1].TrimEnd(), RgxNL);
				for (int _i2 = 0; _i2 < _a1.Length; _i2++)
				{
					lColumn[_i2] += _a1[_i2] + sSeparater;
				}
			}
			// 末尾の sSeparater を消去
			for (int _i1 = 0; _i1 < lColumn.Count; _i1++)
			{
				lColumn[_i1] = lColumn[_i1].Substring(0, lColumn[_i1].Length - sSeparater.Length);
			}
			return string.Join(NL, lColumn) + NL;
		}

		//--------------------------------------------------------------------------------
		// Trim
		//--------------------------------------------------------------------------------
		private string RtnTextTrim(string str)
		{
			StringBuilder sb = new StringBuilder();
			foreach (string _s1 in Regex.Split(str.TrimEnd(), RgxNL))
			{
				_ = sb.Append(_s1.Trim());
				_ = sb.Append(NL);
			}
			return sb.ToString().TrimEnd() + NL;
		}

		//--------------------------------------------------------------------------------
		// 全角 <=> 半角
		//--------------------------------------------------------------------------------
		private string RtnReplacerWide(Match m)
		{
			return Strings.StrConv(m.Value, VbStrConv.Wide, 0x411);
		}

		private string RtnReplacerNarrow(Match m)
		{
			return Strings.StrConv(m.Value, VbStrConv.Narrow, 0x411);
		}

		//--------------------------------------------------------------------------------
		// Sort／Reverse
		//--------------------------------------------------------------------------------
		private string RtnTextSort(string str, bool bAsc)
		{
			List<string> l1 = Regex.Split(str.TrimEnd(), RgxNL).ToList();
			if (bAsc)
			{
				l1.Sort();
			}
			else
			{
				l1.Reverse();
			}
			_ = l1.RemoveAll(s => s.Length == 0);
			return string.Join(NL, l1) + NL;
		}

		//--------------------------------------------------------------------------------
		// Text File ?
		//--------------------------------------------------------------------------------
		private bool RtnIsTextFile(string fn)
		{
			if (!File.Exists(fn))
			{
				return false;
			}
			// 使用中のファイルでないか？
			try
			{
				byte[] bs = File.ReadAllBytes(fn);
				int iNull = 0;
				for (int _iCnt = bs.Length, _i1 = 0; _i1 < _iCnt; _i1++)
				{
					if (bs[_i1] == 0x00)
					{
						++iNull;
						// UTF-16 の 1byte 文字に 0x00 が含まれるので誤検知対策
						if (iNull >= 2)
						{
							return false;
						}
					}
					else
					{
						iNull = 0;
					}
				}
				return true;
			}
			catch
			{
				return false;
			}
		}

		//--------------------------------------------------------------------------------
		// UTF-16 LE
		// UTF-16 BE
		// UTF-8 BOM
		//   BOM判定
		//--------------------------------------------------------------------------------
		// UTF-8 NoBOM
		//   1byte:  [0]0x00..0x7F
		//   2byte:  [0]0xC2..0xDF  [1]0x80..0xBF
		//   3byte:  [0]0xE0..0xEF  [1]0x80..0xBF  [2]0x80..0xBF
		//   4byte:  [0]0xF0..0xF7  [1]0x80..0xBF  [2]0x80..0xBF  [3]0x80..0xBF
		//--------------------------------------------------------------------------------
		// Shift_JIS
		//   2byte:  [0]0x81..0x9F or [0]0xE0..0xEC
		//--------------------------------------------------------------------------------
		private bool RtnIsFileEncCp65001(string fn)
		{
			byte[] bs = File.ReadAllBytes(fn);
			if (bs.Length == 0)
			{
				return true;
			}
			// UTF-16 LE
			// UTF-16 BE
			if (bs.Length >= 2)
			{
				if ((bs[0] == 0xFF && bs[1] == 0xFE) || (bs[0] == 0xFE && bs[1] == 0xFF))
				{
					return false;
				}
			}
			// UTF-8 BOM
			if (bs.Length >= 3 && bs[0] == 0xEF && bs[1] == 0xBB && bs[2] == 0xBF)
			{
				return true;
			}
			// UTF-8 NoBOM
			for (int _iCnt = bs.Length, _i1 = 0; _i1 < _iCnt; _i1++)
			{
				// 1byte
				if (bs[_i1] >= 0x00 && bs[_i1] <= 0x7F)
				{
				}
				// 2byte
				else if (bs[_i1] >= 0xC2 && bs[_i1] <= 0xDF)
				{
					++_i1;
					if (_i1 >= _iCnt || bs[_i1] < 0x80 || bs[_i1] > 0xBF)
					{
						return false;
					}
				}
				// 3byte
				else if (bs[_i1] >= 0xE0 && bs[_i1] <= 0xEF)
				{
					for (int _i2 = 2; _i2 > 0; _i2--)
					{
						++_i1;
						if (_i1 >= _iCnt || bs[_i1] < 0x80 || bs[_i1] > 0xBF)
						{
							return false;
						}
					}
				}
				// 4byte
				else if (bs[_i1] >= 0xF0 && bs[_i1] <= 0xF7)
				{
					for (int _i2 = 3; _i2 > 0; _i2--)
					{
						++_i1;
						if (_i1 >= _iCnt || bs[_i1] < 0x80 || bs[_i1] > 0xBF)
						{
							return false;
						}
					}
				}
				// Shift_JIS
				else if ((bs[_i1] & 0xE0) == 0x80 || (bs[_i1] & 0xE0) == 0xE0)
				{
					return false;
				}
				// 上記以外
				else
				{
					return false;
				}
			}
			return true;
		}

		//--------------------------------------------------------------------------------
		// File Read/Write
		//--------------------------------------------------------------------------------
		private const string CMD_FILTER = "All files (*.*)|*.*|Command (*.iwmcmd)|*.iwmcmd";
		private const string TEXT_FILTER = "All files (*.*)|*.*|Text (*.txt)|*.txt|TSV (*.tsv)|*.tsv|CSV (*.csv)|*.csv|HTML (*.html,*.htm)|*.html,*.htm";

		private string RtnFileReadAllText(string path)
		{
			return File.ReadAllText(path, Encoding.GetEncoding(RtnIsFileEncCp65001(path) ? 65001 : 932));
		}

		private string RtnGrepFile(string path, string search, RegexOptions RgxOpt)
		{
			if (!RtnIsTextFile(path))
			{
				return "";
			}
			StringBuilder sb = new StringBuilder();
			string s1 = RtnFileReadAllText(path);
			int iLine = 0;
			foreach (string _s1 in Regex.Split(s1.TrimEnd(), RgxNL))
			{
				++iLine;
				// 正規表現文法エラーはないか？
				try
				{
					if (Regex.IsMatch(_s1, search, RgxOpt))
					{
						_ = sb.Append(path);
						_ = sb.Append("\tL");
						_ = sb.Append(iLine);
						_ = sb.Append('\t');
						_ = sb.Append(_s1.Trim());
						_ = sb.Append(NL);
					}
				}
				catch
				{
					break;
				}
			}
			return sb.ToString();
		}

		private (string, string) RtnTextFread(string path, bool bGuiOn, string filter) // return(ファイル名, データ)
		{
			if (bGuiOn || path.Length == 0)
			{
				OpenFileDialog ofd = new OpenFileDialog
				{
					Filter = filter,
					InitialDirectory = path.Length == 0 ? Environment.CurrentDirectory : Path.GetDirectoryName(path)
				};
				if (ofd.ShowDialog() == DialogResult.OK)
				{
					path = ofd.FileName;
				}
				else
				{
					return ("", "");
				}
			}
			if (RtnIsTextFile(path))
			{
				// UTF-8(CP65001) でないときは Shift_JIS(CP932) で読込
				return (path, RtnFileReadAllText(path));
			}
			// Err
			return ("", "");
		}

		private bool RtnTextFileWrite(string str, string encode, string path, bool bGuiOn, string filter)
		{
			if (bGuiOn || path.Length == 0)
			{
				SaveFileDialog sfd = new SaveFileDialog
				{
					FileName = Path.GetFileName(path),
					Filter = filter,
					FilterIndex = 1,
					InitialDirectory = path.Length == 0 ? Environment.CurrentDirectory : Path.GetDirectoryName(path)
				};
				if (sfd.ShowDialog() == DialogResult.OK)
				{
					path = sfd.FileName;
				}
				else
				{
					return false;
				}
			}
			// 使用中のファイルでないか？
			try
			{
				switch (encode.ToLower())
				{
					// UTF-8 BOMなし
					case "65001nobom":
						File.WriteAllText(path, str, new UTF8Encoding(false));
						break;
					// UTF-8 BOMあり
					case "65001bom":
						File.WriteAllText(path, str, Encoding.GetEncoding(65001));
						break;
					// Shift_JIS
					case "932":
					default:
						File.WriteAllText(path, str, Encoding.GetEncoding(932));
						break;
				}
			}
			catch (Exception e)
			{
				_ = MessageBox.Show(
					e.Message,
					VERSION
				);
			}
			return true;
		}

		//--------------------------------------------------------------------------------
		// Dir/File 取得
		//--------------------------------------------------------------------------------
		private readonly List<string> GblRtnDirList = new List<string>();
		private readonly List<string> GblRtnFileList = new List<string>();

		private string RtnDirFileList(string path, string sDirFile, bool bRecursive)
		{
			try
			{
				path = Path.GetFullPath(path + "\\");
				sDirFile = sDirFile.ToLower();
			}
			catch
			{
				return "";
			}
			if (!Directory.Exists(path))
			{
				return "";
			}
			GblRtnDirList.Clear();
			GblRtnFileList.Clear();
			// Dir
			GblRtnDirList.Add(path);
			SubDirList(path, bRecursive);
			// File
			if (sDirFile != "d")
			{
				SubFileList();
			}
			string rtn;
			switch (sDirFile)
			{
				// Dir
				case "d":
					GblRtnDirList.Sort();
					rtn = string.Join(NL, GblRtnDirList);
					break;
				// File
				case "f":
					GblRtnFileList.Sort();
					rtn = string.Join(NL, GblRtnFileList);
					break;
				// Dir + File
				default:
					GblRtnDirList.AddRange(GblRtnFileList);
					GblRtnDirList.Sort();
					rtn = string.Join(NL, GblRtnDirList);
					break;
			}
			GblRtnDirList.Clear();
			GblRtnFileList.Clear();
			return rtn;
		}

		private void SubDirList(string path, bool bRecursive)
		{
			// Dir 取得
			// SearchOption.AllDirectories はシステムフォルダ・アクセス時にエラーが出るので使用不可
			foreach (string _s1 in Directory.GetDirectories(path, "*"))
			{
				GblRtnDirList.Add(_s1 + "\\");
				try
				{
					// 再帰
					if (bRecursive)
					{
						SubDirList(_s1, bRecursive);
					}
					else
					{
						return;
					}
				}
				catch
				{
					// エラー・キーは削除
					_ = GblRtnDirList.Remove(_s1 + "\\");
				}
			}
		}

		private void SubFileList()
		{
			// File 取得
			foreach (string _s1 in GblRtnDirList)
			{
				foreach (string _s2 in Directory.GetFiles(_s1, "*"))
				{
					GblRtnFileList.Add(_s2);
				}
			}
		}

		private void SubBgColorChange(string sColor)
		{
			Color curColor = BackColor;
			try
			{
				// RGB "#000000"
				if (sColor.StartsWith("#"))
				{
					BackColor = ColorTranslator.FromHtml(sColor.ToString());
				}
				// Default
				else if (sColor.Trim().Length == 0)
				{
					BackColor = Color.DimGray;
				}
				// 色名 "BLACK"
				else
				{
					BackColor = Color.FromName(sColor.ToString());
				}
			}
			catch
			{
				// 存在しない色名のとき
				BackColor = curColor;
			}
			TbCurDir.BackColor = CbCmdHistory.BackColor = CbResultHistory.BackColor = BackColor = NudTabSize.BackColor = NudFontSize.BackColor = BackColor;
		}

		//--------------------------------------------------------------------------------
		// Main()
		//--------------------------------------------------------------------------------
		private static string[] ARGS;

		private static class Program
		{
			[STAThread]
			private static void Main(string[] args)
			{
				Application.EnableVisualStyles();
				Application.SetCompatibleTextRenderingDefault(false);

				ARGS = args;

				Application.Run(new Form1());
			}
		}

		//--------------------------------------------------------------------------------
		// Debug
		//--------------------------------------------------------------------------------
		private void M(object obj, [CallerLineNumber] int line = 0)
		{
			_ = MessageBox.Show($"{obj}", $"L{line}");
		}
	}
}
