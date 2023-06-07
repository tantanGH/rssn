# RSSN.X

RSS News Reader for X680x0/Human68k

---

# About This

情報過多と言われるこの時代、エッセンスのみが抽出されたRSSで素早く多彩な情報をキャッチしてみませんか。

RSSN.X は、X680x0/Human68k 上で動作する、RSS News リーダーアプリケーションです。往年のパソコン通信BBS用オフラインログリーダーライクなUIを持ちながらも、68側から指定したサイトのRSSフィードをオンラインでブラウズすることが可能です。

<img src='images/rssn2.jpeg' width='800'/>

- RSSサイト定義ファイルサンプル同梱
- emacs/vi風キーバインドで手に馴染むキーボード操作
- ハイメモリ対応(使える状況にあれば自動的に使います)
- 38400bps高速通信対応(要TMSIO.X)
- CUTファイル対応(要CUT.R)
- X68000Z対応

ただし注意点として、データ中継用にミニサーバ `rssnd` を68の外で同時に動かしておく必要があります。`rssnd`は Python で書かれており、Raspberry Piを含むLinux、macOS、Windowsなどの環境で動作させることができます。


以下は X68000XVI 実機と Raspberry Pi 4B を使った動作確認環境の構成例です。

<img src='images/rssn1.png' width='800'/>

RSSN.X は `rssnd` とRS232Cクロスで接続し通信を行います。(通信速度は設定可能です)

<img src='images/rssn3.jpeg' width='600'/>

---

# ハードウェアの準備

RSSN.X を動かすX680x0実機と `rssnd` サーバ機を接続するために以下のケーブルを使います。

1. RS232C 25pin オス - 9pin メス クロスケーブル

[SANWA SUPPLY KRS-423XF-07K](https://amazon.co.jp/dp/B00008BBFQ) など、現在でも新品で購入可能です。ストレートケーブルと間違わないように気をつけてください。

2. RS232C 9pin オス - USB 変換ケーブル

相性問題の出にくい[FTDI製チップセットを使ったもの](https://amazon.co.jp/dp/B07589ZF9X)を個人的にはお勧めします。

必要に応じてさらに USB TypeA - TypeC ケーブルなどを追加してください。

---

# rssnd サーバのセットアップ

---

# RSSN.X のセットアップ


---

# 使い方


---

# RSSサイトリストの編集

---

# CUTファイルの利用


---

# Special Thanks

---

# 更新履歴
