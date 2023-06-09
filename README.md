# RSSN

RSS News Reader System for X680x0/Human68k

---

## About This

情報過多と言われるこの時代、エッセンスのみが抽出されたRSSで素早く多彩な情報をキャッチしてみませんか。

RSSN は、X680x0/Human68k 上で動作する、RSS News Reader Systemです。

- オンライン常時接続
- X680x0側にイーサネット環境不要
- ユーザーインターフェイスとして電脳倶楽部のDSHELLを採用
- あらかじめ実績のあるいくつかのRSSサイトを定義済み
- 38400bps高速通信対応(TMSIO.X)
- X68000Z対応(一部制限あり)

<img src='images/rssn1.png' width='800px'/>

<img src='images/rssn3.png' width='800px'/>

ただし注意点として、データ中継用にミニサーバ `rssnd` を68の外で同時に動かしておく必要があります。`rssnd` は Python で書かれており、Raspberry Piを含むLinux、macOS、Windowsなどの環境で動作させることができます。

<img src='images/rssn2.png' width='800px'/>

---

## インストール方法

あらかじめ電脳倶楽部に添付されていた DSHELL.X を用意しておく必要があります。

RSSNxxx.ZIP をダウンロードし、新規ディレクトリに展開する。

- RSSN.X ... 実行ファイル
- RSSN.CUT ... CUTファイル
- INDEX.DOC ... メインDOCファイル兼RSSサイト定義ファイル
- SETUP.DOC ... セットアップガイド(実機用)
- SETUPZ.DOC ... セットアップガイド(Z用)

展開したディレクトリに移動し、

        dshell index.doc

で起動します。サーバやケーブル接続に関するセットアップの詳細はそちらを参照してください。

---

## 更新履歴

0.5.0 (2023/06/09) ... 初版