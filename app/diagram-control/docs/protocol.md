# BLE protocol v1

GATT service: `f3641400-00b0-4240-ba50-05ca45bf8abc`

| Characteristic | UUID末尾前の識別子 | Properties | サイズ |
|---|---|---|---|
| Command | f3641401 | Write with response | 10 bytes |
| Telemetry | f3641402 | Notify | version=3、8＋13×IMU数＋3×距離数 bytes（最大268） |
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

ESP32のBLEコールバックは受信バイト列と接続世代を通常指令と同じキューへコピーする。`loop()`は現在のBLE接続世代を照合し、未知の種別・途中で切れたサーボレコード・CH／モード範囲外を拒否する。全件が正しい場合だけ`deviceInfos`・`deviceInfoCount`を置き換え、シリアルへ`DeviceInfo accepted`と各レコードを出力する。一覧の置換に成功するとSTREAMと超音波測定要求を停止し、旧測定を無効化する。新しい一覧で送信するにはSTREAMを再要求する。不正な内容は`DeviceInfo rejected`を出し、以前の一覧とSTREAMを保持する。通常のBLE接続・切断、Wi-Fi制御権の解除、キュー飽和では一覧も消去する。BLEからWi-Fi制御権を取得するときは一覧を引き継ぐ。STOPは確定済み一覧を残し、待機中の指令と機器情報を破棄する。

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
| 3 | STOP | 0 | 全出力無効・待機指令破棄。センサー通知は継続 |
| 4 | STREAM | 0/5/10/20 | 登録センサーの通知Hz、0は停止 |
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

flags: bit0 PCA正常、bit1 IMU正常、bit2いずれかのCH有効、bit3 センサー通知有効。
code: 0 OK、1形式エラー、2範囲エラー、3未有効/タイムアウト停止、4ハードウェア未検出、5混雑（予約）、6 I²Cエラー。
キュー飽和時は全PWM停止・キュー破棄となり、未処理の指令にはACKが返りません。Web側のACKタイムアウトで切断します。
不正な長さのパケットはFORMAT seq=0で通知します。

## 機器種別付きTelemetry（version=3）

BLE・Wi-Fiとも、DeviceInfoに登録されたセンサー順にレコードを連結し、サーボは除外する。送信対応はIMU6AxisとUltrasonicDistanceSensor。複数の同種別ブロックは同じ物理MPU-6050／HC-SR04の測定値を共有する。IMU9Axis・Cameraを含む一覧でのSTREAM開始はHARDWAREエラー。DeviceInfo未受信のSTREAM開始はFORMATエラー。センサー0件はヘッダーだけを送る。

STREAMは0/5/10/20Hzで、0は通知と超音波測定要求を停止する。開始時は登録された機器だけの利用可否を検査し、距離センサーだけならIMUを要求しない。HC-SR04はGPIO初期化と測定タスクの確保が成功していれば開始でき、接続・反射の有無は測定結果で判断する。ピン未割り当て・キュー／タスク確保失敗はHARDWAREエラー。

| Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | version=3。倒立用プロットのversion=2とは別 |
| 1 | u8 | flags（Statusと同じ。bit3はセンサー通知有効） |
| 2 | u16 | sample sequence、little endian |
| 4 | u32 | 通知を組み立てるサンプル時刻millis、little endian |
| 8以降 | レコード列 | DeviceInfo順。IMUは13 bytes、距離は3 bytes |

IMU6Axisレコードは従来と同じ。

| レコード内Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | DeviceType.IMU6Axis=0 |
| 1,3,5 | i16 | ax,ay,az raw、little endian |
| 7,9,11 | i16 | gx,gy,gz raw、little endian |

加速度はraw / 16384 × 9.80665 m/s²、角速度はraw / 131 °/sへ変換する。IMUは登録された場合だけ`loop()`で読み、失敗時は従来どおりSTREAMを停止してIO_ERRORを返す。

UltrasonicDistanceSensorレコードを追加する。

| レコード内Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | DeviceType.UltrasonicDistanceSensor=2 |
| 1 | u16 | 距離mm、little endian。有効値20〜4000、未測定・失敗は0xFFFF |

`distanceCm * 10`を四捨五入して格納する。距離0で失敗を表さない。HC-SR04のGPIO初期化・測定・状態参照は専用タスクだけが担当し、測定後に`ULTRASONIC_SAMPLE_INTERVAL_MS`（既定100ms）待機する。ライブラリーも測定開始間隔65ms未満を拒否する。同期測定の最大約30msの待機を`loop()`へ追加しない。20Hz送信などでは最新の測定結果を再利用し、ヘッダーのmillisは距離の測定時刻そのものではない。

測定タスクは「要求世代・距離mm」を最新1件のキューへ渡す。STREAM開始・停止、DeviceInfoの置換、接続変更・切断、キュー飽和、期限超過では要求世代を更新する。送信側は現在の世代と一致する結果だけを使い、それまでは0xFFFFを送る。タイムアウト・範囲外・ECHO事前HIGHでも0xFFFFを送り、距離の失敗だけではSTREAMを停止しない。明示STOPは全PWMを停止するが、従来どおりSTREAMと測定は継続する。

通知長は`8 + 13 × IMU数 + 3 × 距離数`。距離1個は11バイト、IMU1個と距離1個は24バイト。例としてDeviceInfoが`[2, 0, 2, 128, 0, 1]`なら、距離3バイト→IMU13バイト→同じ距離3バイトを連結し、サーボを除く合計27バイトになる。

Characteristic最大長は268バイト、希望ATT MTUは271を維持する。実際の通知長が合意MTU−3以内かを開始時と送信前に検査する。IMU1個ならMTU 24以上、IMU1個と距離1個ならMTU 27以上が必要。不足時はRANGEエラーとし、切り詰め・分割は行わない。最大20個を常に送れる保証ではない。

Wi-Fi制御はBLEの機器一覧を引き継ぎ、同じversion=3をHTTP／WebSocketで送る。最大268バイト、HTTPは最新のセンサー通知を1件保持する。制御権の変更ではSTREAMと測定要求を停止し、新しい経路でのSTREAM開始を待つ。Wi-Fi経由でのDeviceInfo再登録は未実装。

diagram側は今回変更していない。現行ブラウザーはIMUだけのレコード形式を前提としており、距離を含む通知には別途受信処理とSTREAM要求の対応が必要。IMUだけの通知のバイト形式は維持し、旧version=1のIMU形式は復活させない。温度・姿勢角は含めない。IMU内部設定100Hz、通知最大20Hzであり、高周波解析用ではない。

Web transportはGATT操作を直列化し、ACKを待ってから次の操作を行う。ACK待ち上限2.5秒。

## 角度／速度の拡張（2026-10-28）

既存1〜7の意味とversion=1・10byte/12byteのサイズは維持する。

| Opcode | 名前 | value | duration | 動作 |
|---|---|---|---|---|
| 8 | ARM_SPEED | 0 | 0 | 中立パルス（速度0%）でCHを有効化 |
| 9 | SET_SPEED | i16、速度%×100（−10000〜10000） | 0 | 有効な速度CHへ即時出力 |

SET_SPEEDのvalueは2の補数のlittle endian。例えば−25.25%はi16の−2525（0xF623）。範囲外、NaN相当の入力や不正なdurationは受け付けない。NaNはWeb側で拒否し、バイナリへ変換しない。

ARM/MOVEは角度用、ARM_SPEED/SET_SPEEDは速度用。CHが別モードで有効な場合はcode=7（モード不一致）。DISABLEで無効化してから再ARMする。モード選択は接続したサーボの種類に合わせて利用者が行い、ハードウェアの種類を自動判別しない。

Statusのflags bit4は、そのCHの速度モードを示す。速度モードのcurrent/targetはi16の速度%×100、remainingは0。角度モードのcurrent/targetは従来どおりu16の角度×100。全体通知CH=255ではbit4を立てない。bit0〜2、IMUレコードの形式と意味は維持し、bit3は登録センサーの通知有効を示す。

速度0%はPWMを維持する中立指令。DISABLE/STOPはPWMを無効化する。STOP後のSTREAM継続、接続・切断時と5秒期限超過時の出力・STREAM停止も維持する。
