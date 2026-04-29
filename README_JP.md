# PICO TNC D+

Daisuke JA1UMW / CQAKIBA.TOKYO Ver.

[原作 PICO-TNC](https://github.com/amedes/pico_tnc)

[イントロダクション](/INTRODUCTION_JP.md)

PICO TNCはRaspberry Pi Picoを使ったTNC(ターミナルノードコントローラー)です．

# 特長

* WB8WGA PICTNCとほぼ同等の機能
* Bell 202 1200bps AFSKの送信と受信
* AX.25のUIフレームの送信
* 定期的なBEACONの送信
* UI-DIGIの機能
* KISSプロトコルをサポート

![pico-tnc](images/pico-tnc.jpg)

# 回路図

[![pico-tnc-schemantic](images/PICOTNC-sch-S.jpg)](images/PICOTNC-sch-L.jpg)

# 部品リスト

|記号|値|品名|備考|
|---|:---:|---|---|
|U1||Raspberry Pi Pico||
|Q1||DTC143EL|デジタルトランジスタ||
|R1,R2|10k|抵抗|
|R3|100|抵抗|
|R4,R5|1k|抵抗|
|R6|4.7k/22k|抵抗|YESU 4.7k,ICOM/STANDARD 22k|
|D2,D3|1N4148|ダイオード|
|D1|赤|LED|
|D4|緑|LED|
|C1,C2,C3|0.1uF|コンデンサ|
|VR1|10k|半固定抵抗|Bourns 3362P|
|以下オプション|||
|J1||2.5mm Jack|マル信電機MJ-2-34B0|
|J3||3.5mm Jack|マル信電機MJ-8435|
|SW1||スイッチ|秋月P-03647|

# シルク図(部品面)

![pico-tnc-silk](images/PICOTNC-silk.jpg)

# ハンダジャンパの設定

J1,J3を使う場合のみ必要．J2の端子から無線機に接続する場合は必要なし．

||JP1|JP2|JP3|JP4|
|---|:---:|---|---|---|
|ICOM/STANDARD/YAESU|1-2|1-2|オープン|ショート|
|KENWOOD|2-3|2-3|ショート|オープン|

# ファームウェアの書き込み

pico_tnc.uf2

# コマンドリスト(設定例)

|コマンド|設定例|説明|
|---|:---:|---|
|HELP|help, help ja sjis, help ja utf8|ヘルプ表示．`help`は英語，`help ja sjis`/`help ja`はSJIS日本語，`help ja utf8`はUTF-8日本語．`MYCALL`または`UNPROTO`が未設定の場合は設定警告を表示|
|MYCALL|mycall jn1dff-2|コールサインの設定|
|UNPROTO|unproto jn1dff-1 v jn1dff-1|送信先，デジピーターの指定，デジピーターは3つまで|
|CON|con|コンバースモードへ移行，CRで入力した文字列がUIフレームとしてUNPROTOの設定先に送信される，Ctrl-Cで終了|
|ABOUT|about|バージョン情報とサードパーティコンポーネント情報の表示|
|BTEXT|btext this is beacon|ビーコンで送信するテキストの設定，最大100バイト|
|BEACON|beacon every n|ビーコンの送信間隔を分で指定，n=0でオフ，設定範囲は1～59分|
|MONitor|mon all, mon me, mon off|モニターするするパケットの指定，all 全て，me 自局宛のみ，off モニターしない|
|MYALIAS|myalias RELAY|エイリアスの指定，指定されたコールサインでデジピーターとして動作する|
|PERM|perm|設定を不揮発性メモリに保存する|
|DISP|disp|設定値の表示|
|ECHO|echo on, echo off|シリアル入力のエコーバックの設定，on 表示する，off 表示しない|
|GPS|gps $GPGAA, gps $GPGLL, gps $GPRMC|GPSのどのメッセージを送信するかの設定，それぞれの$GPSメッセージを送信|
|PRIVKEY GEN|privkey gen p2pkh|Monacoin秘密鍵の生成（生成後にアドレス表示、`Space`で再生成、`Enter`でRAM反映。Flash保存は`perm`が必要）|
|PRIVKEY SET|privkey set p2wpkh:`<WIF>`|Monacoin秘密鍵のインポート. 保存対象は32byte生鍵+compressed+active typeのみ．`set m/p/mona1/p2pkh/p2sh/p2wpkh`はactive typeのみ正規化して更新．|
|PRIVKEY SHOW|privkey show|Monacoin秘密鍵の表示. `show`はセキュリティ確認後に秘密情報を表示.|
|SIGN MSG|sign msg `<text>`|`{"FR":"<MYCALL>","MSG":"<text>"}`を署名して署名文字列を連結し，AX.25 UIフレーム送信前にEnter/ESC確認|
|SIGN ADV|sign adv -name `<name>` -bio `<bio>`|`{"FR":"<MYCALL>","ADV":{"N":"<name>","B":"<bio>","A":"<active_address>"}}`を署名して署名文字列を連結し，AX.25 UIフレーム送信前にEnter/ESC確認|
|SIGN QSL|sign qsl ...|`{"FR":"<MYCALL>","QSL":{"C":"...","S":"...","D":"...","T":"...","F":"...","M":"...","P":"..."}}`形式を署名して送信待機．引数指定またはウィザード入力に対応（各オプション値はスペースを含めて指定可能で，次の`-option`直前までを取得）|
|SIGN RECOVERY|sign recovery `{JSON}<署名>`|受信パケット時と同じ署名リカバリ処理へ手入力ペイロードを流し込み，受信側表示をデバッグ|
|SYSTEM|system usb_bootloader|USB BOOTSELモード移行コマンド．10秒以内に `Y` `E` `S` `Enter` を順に1文字ずつ入力した場合のみ実行|
|TERMTEST|termtest|端末入力バイトの検査モード．受信バイトを16進で表示し，主要制御文字名も併記．Ctrl-Cで終了|
|TXDELAY|txdelay n / txdelay nms / txdelay ns|送信遅延の設定（0～1000ms）．単位なしnは従来互換で10ms単位|
|AXDELAY|axdelay n / axdelay nms / axdelay ns|AX.25送信前ディレイの設定（0～1000ms）．単位なしnは従来互換で10ms単位|
|AXHANG|axhang n / axhang nms / axhang ns|送信データ終了後のPTT保持時間を設定（0～1000ms）．単位なしnは従来互換で10ms単位|
|CALIBRATE|calibrate|テスト用のトーンを送信する，スペースでトーンの切り替え，Ctrl-Cで終了|
|KISS|kiss on, kiss off|KISSモードのオン，オフ|

# コマンドライン編集と履歴

- 入力バッファ: `1024`バイト（1024バイトを超える入力は末尾を切り捨て）
- 履歴バッファ: `1024`バイト × `8`本
- ANSIエスケープシーケンス対応キー:
  - `↑ / ↓`: 履歴の前/次
  - `← / →`: カーソル左/右
  - `Home / End`: 行頭/行末
  - `Backspace / Delete`: 行途中の編集
- ANSI非対応端末向けの同等キー:
  - `Ctrl+P / Ctrl+N`: 履歴の前/次
  - `Ctrl+B / Ctrl+F`: カーソル左/右
  - `Ctrl+A / Ctrl+E`: 行頭/行末
  - `Ctrl+H`: バックスペース

![bell202-wave](images/bell202-wave.png)
![command line](images/command.png)
[![schemantic](images/schematic.jpg)](schematic.png)
