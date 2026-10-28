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

Protocolのcommand/status/IMUは既存version=1、Plotはversion=2を維持する。NVS形式、Wi-Fiのポート80/81/82、HMAC認証、CORS、BLEの認証済み分割設定窓口は既存実装を引き継ぐ。コードの再配置とAPIの変更は通信形式の変更を意味しない。

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

`app/diagram-control/`は既存のuroa BLE Labを基に、公開メンバーAPIへ移行したアプリ。ESP32 DevKitとXIAO ESP32-S3 Senseを対象とする。新しいBLE表示名は`ESP32-Diagram-Control`で、UUID、バイナリ形式、Wi-Fiの教材設定・NVS形式は維持する。

- `include/config.h`: ピン、サーボ範囲、UUID、期限、公開の教材設定。
- `src/main.cpp`: 初期化、16件の指令キュー、接続世代、優先STOP、5秒期限監視、サーボ補間、IMU通知。
- `include/network.h`・`src/network.cpp`: 共通WifiNetworkへの設定とコールバックの受け渡し。
- `include/camera.h`・`src/camera.cpp`: Senseカメラと公開の`cameraReady` atomic。カメラ処理も宣言と実装を分離する。

起動と接続変更で出力・STREAMを停止し、ARMは明示操作とする。STOPはキューを迂回し、IMU通知を継続する。キューからは1周最大4件を処理し、現在の通信経路と接続世代を検査する。I2Cはsetup()とloop()に集約する。I2C開始失敗時は機器の初期化を行わず、キュー確保失敗時は出力停止を試みて通信を開始しない。

カメラはWi-Fiのサービス開始時に初期化する。開始済みなら再初期化せず、非対応基板・PSRAMなし・初期化失敗では利用不可とする。画像の認証・CORSはWifiNetworkが担当する。アプリはProtocol・Servo・IMU・BLE・Wi-Fiを使い、倒立用のPlot通知は送らない。
