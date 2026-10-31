# ライブラリーの構成とAPI

共通ライブラリーは`lib/`に置き、各ライブラリーの宣言を`include/`、実装を`src/`に分離する。教材アプリは`app/`に独立したPlatformIOプロジェクトとして置く。現在のアプリは[diagram-control](../app/diagram-control/README.md)。教材アプリのピン、周期、キュー、停止方針、認証設定は共通ライブラリーへ固定しない。

## 公開状態への移行

バージョン0.2.0では、元の0.1.0の呼出元に対して以下のAPI変更がある。元のuroaリポジトリーは変更しない。

| 旧API | 新API |
|---|---|
| `imu.available()` | `imu.available` |
| `imu.address()` | `imu.address` |
| `servos.available()` | `servos.available` |
| `servos.state(ch)` | `servos.states[ch]`。CH0〜15の範囲は呼出元が確認する |
| `bluetooth.connected()` | `bluetooth.connected.load()` |
| `bluetooth.generation()` | `bluetooth.generation.load()` |
| `network.connected()` | `network.connected.load()`。ルーター接続ではなく制御権の状態 |
| `network.generation()` | `network.generation.load()` |

公開状態はライブラリーが更新し、アプリは参照だけを行う。直接代入しても機器・接続は変更されない。機器の操作は`begin()`、`arm()`、`move()`、`allOff()`、`stopControl()`などで行う。

`anyArmed()`はCHを走査し、`remaining()`は範囲検査と残り時間を計算する。`bleUnavailable()`はBLEの停止予定・停止済みの2状態を調べる。`setRate()`は0〜20Hzの範囲検査と送信タイミングの更新を行う。これらは残す。

## 呼出元の責任

- IMU・サーボは教材が開始した`TwoWire&`を受け取る。同じ制御タスクからI2Cを操作する。
- BLE・Wi-Fiの指令コールバックでは受信内容を即座にコピーし、キューや停止要求へ渡す。コールバック中にI2Cを操作しない。
- BLE・Wi-Fiの`connected`と`generation`は`std::atomic`を保持する。タスク間の更新・参照の同期を通常のboolや整数で置き換えない。
- 通信オブジェクトは稼働中保持する。BLEとWi-Fi/mDNSはそれぞれ基板上で1インスタンスを前提とする。Wi-Fiタスクとサーバーを破棄するAPIは提供しない。
- STOP、ハートビート、指令キュー、STREAM周期と接続世代の照合は教材側で実装する。
- Plotは通信手段を知らず、`Sender`コールバックへ20バイトを渡す。同じタスクから`setRate()`と`publish()`を呼ぶ。

## 通信と依存関係

BLE・Wi-FiのTelemetryはversion=3とし、8バイトヘッダーの後へ登録センサー順に13バイトのIMU6Axisレコードを並べる。Protocolの`deviceTelemetry()`が種別・加速度・角速度の順で変換する。同じMPU-6050の読み取りを複数ブロックへ共有する。DeviceInfo未受信のSTREAM開始要求はFORMAT、IMU6Axis以外のセンサーを含むSTREAMはHARDWAREとして拒否する。BLE側はCharacteristic容量を268バイト、希望MTUを271に設定し、公開atomicの合意`mtu`を確認する。通知長＋3がMTUを超えるSTREAMは拒否し、送信時も上限を検査する。Wi-Fiは最大268バイトの可変長通知を扱い、BLEからの制御権取得時には機器一覧を引き継ぐ。旧20バイトTelemetryのAPI・実装は削除済み。

2026-10-31に、既存ServiceへDeviceInfoのWrite用Characteristic（`f3641406-00b0-4240-ba50-05ca45bf8abc`、最大20バイト／書き込み）を追加した。教材から`BleTransport::Config::deviceInfoUuid`を渡し、空なら登録しない。専用の`Callbacks::deviceInfo`へ渡し、教材が通常指令と同じキューへ内容と接続世代をコピーする。`loop()`は種別・レコード長・サーボCH／モードを全件検証し、成功時だけ受信順の一覧を置き換える。接続・制御権変更とキュー飽和では一覧を消去する。既存Commandとして解釈せず、機器初期化・ARM・STREAM開始も行わない。検証結果はシリアルへ表示し、専用ACK・分割転送・Wi-Fi転送は未実装。BLEの停止・再開でCharacteristicの参照を解除・再登録する。形式は[protocol.md](../app/diagram-control/docs/protocol.md)を参照する。

Protocolのcommand/statusは既存version=1、IMUはversion=3、Plotはversion=2を使う。NVS形式、Wi-Fiのポート80/81/82、HMAC認証、CORS、BLEの認証済み分割設定窓口は既存実装を引き継ぐ。

BLEはNimBLE-Arduino、Wi-FiはArduinoJsonとWebSockets、ServoはAdafruit PWM Servo DriverとBusIOに依存する。バージョンは各`library.json`を参照する。IMU、Protocol、Plotには追加の外部ライブラリー依存はない。

## 呼び出し例

以下は参照方法を示す断片で、完全な教材アプリではない。実際のピン設定とI2Cの初期化は呼出元で行う。

```cpp
#include <uroa_imu.h>
#include <uroa_ble.h>

uroa::Mpu6050 imu(Wire);
uroa::BleTransport bluetooth;

// 同じ制御タスクで参照する。
const bool imuReady = imu.available;
const uint8_t imuAddress = imu.address;
// 他タスクの更新を読むときもatomicの同期を維持する。
const bool connected = bluetooth.connected.load();
const uint32_t generation = bluetooth.generation.load();
```


## diagram-control

`app/diagram-control/`は既存のuroa BLE Labを基に、公開メンバーAPIへ移行したアプリ。ESP32 DevKit、XIAO ESP32-S3 Sense、ESP32-S3 SuperMiniを対象とする。新しいBLE表示名は`ESP32-Diagram-Control`で、UUID、バイナリ形式、Wi-Fiの教材設定・NVS形式は維持する。

- `include/config.h`: ピン、サーボ範囲、UUID、期限、公開の教材設定。
- `src/main.cpp`: 初期化、16件の指令キュー、接続世代、優先STOP、5秒期限監視、サーボ補間、IMU通知。
- `include/network.h`・`src/network.cpp`: 共通WifiNetworkへの設定とコールバックの受け渡し。
- `include/camera.h`・`src/camera.cpp`: Senseカメラと公開の`cameraReady` atomic。カメラ処理も宣言と実装を分離する。

起動と接続変更で出力・STREAMを停止し、ARMは明示操作とする。STOPはキューを迂回し、IMU通知を継続する。キューからは1周最大4件を処理し、現在の通信経路と接続世代を検査する。I2Cはsetup()とloop()に集約する。I2C開始失敗時は機器の初期化を行わず、キュー確保失敗時は出力停止を試みて通信を開始しない。

カメラはWi-Fiのサービス開始時に初期化する。開始済みなら再初期化せず、非対応基板・PSRAMなし・初期化失敗では利用不可とする。画像の認証・CORSはWifiNetworkが担当する。アプリはProtocol・Servo・IMU・BLE・Wi-Fiを使い、倒立用のPlot通知は送らない。

## 角度と速度のサーボ制御（0.3.0）

同一PCA9685のCHごとに`Mode::Angle`・`Mode::Speed`と有効状態を保持する。`moveAngle(ch, degrees, duration, now)`は度を0.01度へ換算し、従来の角度補間を使う。`setSpeed(ch, percent)`は方向付き%を校正済みの中立パルスから換算し、即時に反映する。いずれも入力値・CH・機器・有効状態・モードを検査する。

角度は`armAngle()`、速度は`armSpeed()`で明示的に有効化する。別モードで有効なCHへのARM・設定は`WrongMode`。変更には先に無効化が必要。速度0は中立パルス、DISABLE/STOP/切断/期限超過はPWM無効化。再接続・保存復元から自動ARMしない。

Protocolはversion=1へopcode 8/9とStatus bit4、code=7を追加する。角度用の1〜7は維持し、旧ファームウェアが速度指令を角度指令として受理しないようにする。速度値は符号付き0.01%、角度は従来の符号なし0.01度。詳細は[Protocol](../app/diagram-control/docs/protocol.md)。

## SuperMini用環境

diagram-controlの`supermini_esp32s3`は汎用S3定義を基にFlash 4MB・DIO・USB CDCを指定する。専用マクロ`UROA_BOARD_SUPERMINI_ESP32S3`でI2CをGPIO8/9へ切り替え、既存のXIAOのGPIO5/6とDevKitのGPIO21/22を維持する。PSRAMを要求せず、XIAO専用のカメラ初期化はコンパイル対象にしない。BLE・Wi-Fi・角度／速度制御・Protocolは共通である。
