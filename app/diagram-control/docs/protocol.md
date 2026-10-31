# BLE protocol v1

GATT service: `f3641400-00b0-4240-ba50-05ca45bf8abc`

| Characteristic | UUID末尾前の識別子 | Properties | サイズ |
|---|---|---|---|
| Command | f3641401 | Write with response | 10 bytes |
| Telemetry | f3641402 | Notify | version=3、8＋13×IMU数 bytes（最大268） |
| Status | f3641403 | Read / Notify | 12 bytes |
| Wi-Fi Config | f3641404 | Write（暗号化・認証必須） | 分割JSON要求、384バイト未満 |
| Wi-Fi Result | f3641405 | Read（暗号化・認証必須） | JSON結果、最大512バイト |
| DeviceInfo | f3641406 | Write with response | 0〜20 bytes。機器レコードの列 |

UUIDの残り部分はサービスと同じです。Command／Status／Telemetryの数値はlittle endianです。Command・Statusは既定ATT MTU 23（データ20byte）に収まります。機器種別付きTelemetryは後述のMTU交渉が必要です。Wi-Fi ConfigのUTF-8 JSONは分割Write、Wi-Fi Resultは長いReadを使用します。
BLEのWrite responseはGATTの受付を示し、Status通知がアプリ側の受付結果を示します。
MOVEの受付成功は動作完了を意味しません。INFOにチャンネルを指定すると現在の内部指令値・目標値・残り時間を取得できます。

Wi-Fi設定のJSON操作、分割送信、Wi-Fi上の指令・署名・セッション・ポート・停止は [アプリのWi-Fi接続手順](../README.md) を参照する。下記Command／Status／Telemetryのバイナリ形式はBLE・Wi-Fiで共通。

## DeviceInfo

DeviceInfoはdiagram上の機器情報を送る独立したCharacteristicで、Commandの10バイト形式やopcodeは使わない。1回の書き込み全体を機器一覧とし、先頭のversion・件数・終端・未使用領域の0埋めは付けない。各値はu8。diagramはセンサー、アクチュエータの順で`writeDeviceInfo()`の出力を連結し、使用したバイト数だけを送る。種別0はIMU6Axisなので、0埋めを機器一覧の末尾へ加えない。

| 種別 | 名前 | レコード長 | 内容 |
|---|---|---|---|
| 0 | IMU6Axis | 1 byte | 種別のみ |
| 1 | IMU9Axis | 1 byte | 種別のみ |
| 2 | UltrasonicDistanceSensor | 1 byte | 種別のみ |
| 3 | Camera | 1 byte | 種別のみ |
| 128 | ServoMotor | 3 bytes | 種別、CH 0〜15、モード（0=角度、1=速度） |

例: `[0, 128, 0, 1, 128, 1, 1]`は6軸IMUと速度モードのサーボCH0・CH1。種類が同じ機器も受信順に保持する。空の一覧は0バイトの書き込みで消去する。20バイト超はWeb側で送信前に拒否する。分割転送とWi-Fi経由の転送は未実装。

ESP32のBLEコールバックは受信バイト列と接続世代を通常指令と同じキューへコピーする。`loop()`は現在のBLE接続世代を照合し、未知の種別・途中で切れたサーボレコード・CH／モード範囲外を拒否する。全件が正しい場合だけ`deviceInfos`・`deviceInfoCount`を置き換え、シリアルへ`DeviceInfo accepted`と各レコードを出力する。不正な内容は`DeviceInfo rejected`を出し、以前の一覧を保持する。通常のBLE接続・切断、Wi-Fi制御権の解除、キュー飽和では一覧も消去する。BLEからWi-Fi制御権を取得するときは一覧を引き継ぐ。STOPは確定済み一覧を残し、待機中の指令と機器情報を破棄する。

ここで行うのは機器情報の保持であり、機器の存在確認・ドライバー追加・I2C設定・サーボのARM・計測開始は行わない。Write responseはGATTの受付を示し、`loop()`での検証完了を示さない。DeviceInfo専用のアプリ側ACKは未実装で、検証結果はシリアルで確認する。

## Command

| Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | version=1 |
| 1 | u8 | opcode |
| 2 | u16 | sequence 1..65535。0はサーバー自発通知用 |
| 4 | u8 | channel 0..15、全体の操作は255 |
| 5 | u8 | reserved=0 |
| 6 | u16 | value |
| 8 | u16 | duration_ms 0..10000 |

| Opcode | 名前 | value | 動作 |
|---|---|---|---|
| 1 | ARM | 角度×100 | PWM開始、即時指令 |
| 2 | MOVE | 角度×100 | durationで補間、0なら即時 |
| 3 | STOP | 0 | 全出力無効・待機指令破棄。IMU通知は継続 |
| 4 | STREAM | 0/5/10/20 | IMU通知Hz、0は停止 |
| 5 | PING | 0 | ハートビート |
| 6 | INFO | 0 | 状態取得 |
| 7 | DISABLE | 0 | 指定CHのみ無効 |

ARM/MOVEは初期設定で6000..12000のみ受け付けます。
接続時・切断時は全出力とストリームを停止。5秒間有効形式の指令がなければ全出力とストリームを停止します。
Web側PINGは1.5秒周期。停止後にPINGが再開しても出力は再有効化されません。
BLEコールバックでは指令をキューへ入れ、loopでI²C処理を実行します。STOPは優先フラグを使用します。

## Status

| Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | version=1 |
| 1 | u8 | code |
| 2 | u16 | ack sequence、0は自発通知 |
| 4 | u8 | flags |
| 5 | u8 | channel、全体255 |
| 6 | u16 | 内部指令角度×100（チャンネル指定時） |
| 8 | u16 | 目標角度×100 |
| 10 | u16 | 残り時間ms |

flags: bit0 PCA正常、bit1 IMU正常、bit2いずれかのCH有効、bit3 IMU通知有効。
code: 0 OK、1形式エラー、2範囲エラー、3未有効/タイムアウト停止、4ハードウェア未検出、5混雑（予約）、6 I²Cエラー。
キュー飽和時は全PWM停止・キュー破棄となり、未処理の指令にはACKが返りません。Web側のACKタイムアウトで切断します。
不正な長さのパケットはFORMAT seq=0で通知します。

## 機器種別付きTelemetry（version=3）

BLE・Wi-Fiとも、DeviceInfoに登録されたセンサー順にレコードを連結する。DeviceInfo未受信で0以外のSTREAMを要求するとFORMATエラーを返す。サーボのレコードはTelemetryへ含めない。現在送信できるセンサーはIMU6Axisだけで、複数のIMUブロックは同じ物理MPU-6050のサンプルを共有する。IMU9Axis・距離・カメラを含むDeviceInfoは保持できるが、その一覧でSTREAMを開始すると未対応としてHARDWAREエラーを返す。

| Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | version=3。倒立用プロットのversion=2とは別 |
| 1 | u8 | flags（Statusと同じ） |
| 2 | u16 | sample sequence、little endian |
| 4 | u32 | ESP32 millis、little endian |
| 8以降 | レコード列 | DeviceInfoのセンサー順。各IMUは下表の13 bytes |

| レコード内Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | DeviceType.IMU6Axis=0 |
| 1,3,5 | i16 | ax,ay,az raw、little endian |
| 7,9,11 | i16 | gx,gy,gz raw、little endian |

通知長は8＋13×IMU数で、IMU1個は21バイト、2個は34バイト。加速度はraw / 16384 × 9.80665 m/s²、角速度はraw / 131 °/sへ変換する。登録センサー数と長さ・種別を全件検証してから、diagramの各`readStatus()`へ渡す。旧version=1のIMU生成・受信処理は削除済み。

diagram-controlのCharacteristic最大長は268バイト、希望ATT MTUは271とする。実際に使える長さは接続相手との合意MTU−3バイトであり、IMU1個には最低24、2個には最低37のMTUが必要。STREAM要求時に不足していればRANGEエラーと必要MTUのシリアルログを返し、通知を開始しない。送信前にも確認し、収まらない通知を切り詰めたり分割したりしない。最大20個を常に送れる保証ではない。

Wi-Fi制御はBLEで受信済みの機器一覧を引き継ぎ、同じversion=3をHTTP／WebSocketで送る。送信バッファーは最大268バイトで、HTTPは最新のIMU通知を1件だけ保持する。Wi-Fi経由でのDeviceInfo再登録は未実装のため、一覧を変更する場合はBLEで再接続する。

温度・姿勢角はこの版に含めない。100Hzの内部センサー設定を最大20Hzで読み出すため、高周波の動きの解析用ではない。

Web transportはGATT操作を直列化し、ACKを待ってから次の操作を行います。ACK待ち上限2.5秒。

## 角度／速度の拡張（2026-10-28）

既存1〜7の意味とversion=1・10byte/12byteのサイズは維持する。

| Opcode | 名前 | value | duration | 動作 |
|---|---|---|---|---|
| 8 | ARM_SPEED | 0 | 0 | 中立パルス（速度0%）でCHを有効化 |
| 9 | SET_SPEED | i16、速度%×100（−10000〜10000） | 0 | 有効な速度CHへ即時出力 |

SET_SPEEDのvalueは2の補数のlittle endian。例えば−25.25%はi16の−2525（0xF623）。範囲外、NaN相当の入力や不正なdurationは受け付けない。NaNはWeb側で拒否し、バイナリへ変換しない。

ARM/MOVEは角度用、ARM_SPEED/SET_SPEEDは速度用。CHが別モードで有効な場合はcode=7（モード不一致）。DISABLEで無効化してから再ARMする。モード選択は接続したサーボの種類に合わせて利用者が行い、ハードウェアの種類を自動判別しない。

Statusのflags bit4は、そのCHの速度モードを示す。速度モードのcurrent/targetはi16の速度%×100、remainingは0。角度モードのcurrent/targetは従来どおりu16の角度×100。全体通知CH=255ではbit4を立てない。bit0〜3、IMU通知の形式と意味は維持する。

速度0%はPWMを維持する中立指令。DISABLE/STOPはPWMを無効化する。STOP後のSTREAM継続、接続・切断時と5秒期限超過時の出力・STREAM停止も維持する。
