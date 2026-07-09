### 実行に必要なファイル
- iwmfind.exe

---

### このプログラムについて
- ファイルサーバ管理コマンドとして作成しました。

- 以下のような使用を想定しています。
    | 使用例 | オプション例 |
    | :- | :- |
    | ワンオフ実行 | iwmfind "フォルダ名" -r |
    | DBに保存 | iwmfind "フォルダ名" -r -o="DBファイル名" |
    | 1週間以上前に更新されたファイルを検索 | iwmfind -i="DBファイル名" -w="mtime<=#{-1w} and type='f'" |

- インメモリDBを作成してから処理を開始するので、ファイル数による処理時間を考慮してください。

---

### SQLite3 との関係
- このプログラムは、**sqlite3.c** から生成したスタティックライブラリを使用しています。
  - https://www.sqlite.org/download.html
    - sqlite-amalgamation-xxx.zip
      - **sqlite3.c**
      - sqlite3.h 
