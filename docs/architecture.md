# ライブラリーの構成とAPI

共通ライブラリーは`lib/`に置き、各ライブラリーの宣言を`include/`、実装を`src/`に分離する。教材アプリは`app/`に独立したPlatformIOプロジェクトとして置く。現在のアプリは[diagram-control](../app/diagram-control/README.md)。教材アプリのピン、周期、キュー、停止方針、認証設定は共通ライブラリーへ固定しない。

PythonデバッグのBLE窓口は[uroa_ble.h](../lib/ble/include/uroa_ble.h)、Wi-Fi窓口は[uroa_wifi.h](../lib/wifi/include/uroa_wifi.h)、教材側の要求処理・命令フラグ・停止値は[bytecode_debugger.cpp](../app/diagram-control/src/bytecode_debugger.cpp)を参照する。通信の完全な形式はuroaの[debug-protocol.ts](../../uroa/diagram/ts/ble-wifi/debug-protocol.ts)を正本とし、ライブラリーはVMを直接操作しない。ビルド・実機確認の境界は[検証記録](build-and-test.md)に記載する。

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

- IMU・サーボ・Displayは教材が開始した`TwoWire&`を受け取る。同じ制御タスクからI2Cを操作する。
- BLE・Wi-Fiの指令コールバックでは受信内容を即座にコピーし、キューや停止要求へ渡す。コールバック中にI2Cを操作しない。
- BLE・Wi-Fiの`connected`と`generation`は`std::atomic`を保持する。タスク間の更新・参照の同期を通常のboolや整数で置き換えない。
- 通信オブジェクトは稼働中保持する。BLEとWi-Fi/mDNSはそれぞれ基板上で1インスタンスを前提とする。Wi-Fiタスクとサーバーを破棄するAPIは提供しない。
- STOP、ハートビート、指令キュー、STREAM周期と接続世代の照合は教材側で実装する。
- Plotは通信手段を知らず、`Sender`コールバックへ20バイトを渡す。同じタスクから`setRate()`と`publish()`を呼ぶ。

## 通信と依存関係

通常指令・通知の共通配送と接続照合は[control_transport.h](../app/diagram-control/include/control_transport.h)、Web側の待ち列・ACK・通知検証は[control-connection.ts](../../uroa/diagram/ts/ble-wifi/control-connection.ts)を参照する。WifiNetworkの通知APIは[publishStatus()/publishTelemetry()](../lib/wifi/include/uroa_wifi.h)へ分離した。

BLE・Wi-FiのTelemetryはversion=3とし、8バイトヘッダーの後へ登録センサー順のIMU6Axis（種別0・13バイト）／超音波距離（種別2・3バイト）を連結する。距離はmm単位のuint16 little endian、20〜4000mmが有効、未測定・失敗は0xFFFF。同種別の複数ブロックは同じ物理センサーの値を共有する。Protocol 0.4.0の`deviceTelemetryLength()`で通知長を計算し、`deviceTelemetry()`はセンサー種別配列・IMU生データ・距離mmを受け取って変換する。全件検査後に出力し、機器の測定や通信は担当しない。DeviceInfo未受信のSTREAMはFORMAT、IMU9Axis・Cameraを含む一覧はHARDWAREとして拒否する。利用可否は登録機器ごとに調べ、距離だけの構成ではIMUを要求しない。BLEは最大268バイト・希望MTU 271を維持し、通知長＋3が合意MTUを超えれば拒否する。Wi-Fiも最大268バイトの同じ形式を使う。

DeviceInfoはBLEとWi-Fiの両経路で登録する。機器一覧の形式は[device-telemetry.ts](../../uroa/diagram/ts/ble-wifi/device-telemetry.ts)、配送・完了の範囲は[control-connection.tsのsendDeviceInfo()](../../uroa/diagram/ts/ble-wifi/control-connection.ts)、内容検証・置換・停止と接続世代の照合は[main.cpp](../app/diagram-control/src/main.cpp)を正本とする。Wi-Fiの認証と配送は[dispatchControl()](../lib/wifi/src/uroa_wifi.cpp)を参照する。

Protocolのcommand/statusは既存version=1、IMUはversion=3、Plotはversion=2を使う。NVS形式、Wi-Fiのポート80/81/82、HMAC認証、CORS、BLEの認証済み分割設定窓口は既存実装を引き継ぐ。

BLEはNimBLE-Arduino、Wi-FiはArduinoJsonとWebSockets、ServoはAdafruit PWM Servo DriverとBusIO、DisplayはAdafruit SSD1306・GFX・BusIOに依存する。バージョンは各`library.json`を参照する。IMU、Protocol、Plotには追加の外部ライブラリー依存はない。

Displayの公開APIと転送仕様は[uroa_display.h](../lib/display/include/uroa_display.h)、Pythonの`Display`とVMの接続・寿命管理は[device_native.cpp](../app/diagram-control/src/device_native.cpp)を参照する。利用例は[ライブラリーREADME](../lib/display/README.md)に置く。

## バイトコードの受信とRAM保持

プログラム転送はBLE／Wi-Fiの[共通転送仕様](../../uroa/diagram/ts/ble-wifi/bytecode-transfer.ts)を使う。各通信ライブラリーは要求のコピーと結果保持を担当し、[ControlTransport](../app/diagram-control/include/control_transport.h)が受信元・接続世代を照合して配送する。

受信器のキュー・形式検証・確定と取消は[bytecode_receiver.cpp](../app/diagram-control/src/bytecode_receiver.cpp)、両経路で使う識別子は[control_session.h](../app/diagram-control/include/control_session.h)を正本とする。受信完了・開始・実行終了は別々の状態として扱う。

## PythonサブセットVM

VM開始・停止・実行元セッションの照合・バッファの借用は[main.cppのrunBytecode()/handle()](../app/diagram-control/src/main.cpp)、PythonBlockとの連携は[python-block.ts](../../uroa/diagram/ts/python/python-block.ts)を参照する。

命令・値・機器スタブ・実行制限・エラーと再開・ステップ制御は[VM仕様](../app/diagram-control/src/bytecode_vm.cpp)、ブレークポイント・ステップ用停止点・停止値と要求受付は[BytecodeDebugger](../app/diagram-control/src/bytecode_debugger.cpp)を正本とする。

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

`app/diagram-control/`は既存のuroa BLE Labを基に、公開メンバーAPIへ移行したアプリ。ESP32 DevKit、XIAO ESP32-S3 Sense、ESP32-S3 SuperMiniを対象とする。基板固有番号付きのBLE／Wi-Fi表示名は[device_name.cpp](../app/diagram-control/src/device_name.cpp)を参照する。UUID、バイナリ形式、Wi-Fiの教材設定・NVS形式は維持する。

- `include/config.h`: ピン、サーボ範囲、UUID、期限、公開の教材設定。
- `src/main.cpp`: 初期化、16件の指令キュー、接続世代、優先STOP、5秒期限監視、サーボ補間、登録センサー通知、超音波測定タスク。
- `include/bytecode_receiver.h`・`src/bytecode_receiver.cpp`: 分割バイトコード専用の4件キュー、CRC・形式の検証、正常なCOMMITだけをRAMへ確定する処理。
- `include/network.h`・`src/network.cpp`: 共通WifiNetworkへの設定とコールバックの受け渡し。
- `include/camera.h`・`src/camera.cpp`: Senseカメラと公開の`cameraReady` atomic。カメラ処理も宣言と実装を分離する。

起動と接続変更で出力・STREAMを停止し、ARMは明示操作とする。STOPはキューを迂回し、センサー通知と超音波測定を継続する。キューからは1周最大4件を処理し、現在の通信経路と接続世代を検査する。I2Cはsetup()とloop()に集約する。I2C開始失敗時はI2C機器の初期化を行わず、指令キュー確保失敗時は出力停止を試みて通信を開始しない。

カメラはWi-Fiのサービス開始時に初期化する。開始済みなら再初期化せず、非対応基板・PSRAMなし・初期化失敗では利用不可とする。画像の認証・CORSはWifiNetworkが担当する。アプリはProtocol・Servo・IMU・Ultrasonic・BLE・Wi-Fiを使い、倒立用のPlot通知は送らない。

## 角度と速度のサーボ制御（0.3.0）

同一PCA9685のCHごとに`Mode::Angle`・`Mode::Speed`と有効状態を保持する。`moveAngle(ch, degrees, duration, now)`は度を0.01度へ換算し、従来の角度補間を使う。`setSpeed(ch, percent)`は方向付き%を校正済みの中立パルスから換算し、即時に反映する。いずれも入力値・CH・機器・有効状態・モードを検査する。

角度は`armAngle()`、速度は`armSpeed()`で明示的に有効化する。別モードで有効なCHへのARM・設定は`WrongMode`。変更には先に無効化が必要。速度0は中立パルス、DISABLE/STOP/切断/期限超過はPWM無効化。再接続・保存復元から自動ARMしない。

Protocolはversion=1へopcode 8/9とStatus bit4、code=7を追加する。角度用の1〜7は維持し、旧ファームウェアが速度指令を角度指令として受理しないようにする。速度値は符号付き0.01%、角度は従来の符号なし0.01度。詳細は[Protocol](../app/diagram-control/docs/protocol.md)。

## SuperMini用環境

diagram-controlの`supermini_esp32s3`は汎用S3定義を基にFlash 4MB・DIO・USB CDCを指定する。専用マクロ`UROA_BOARD_SUPERMINI_ESP32S3`でI2CをGPIO8/9へ切り替え、既存のXIAOのGPIO5/6とDevKitのGPIO21/22を維持する。PSRAMを要求せず、XIAO専用のカメラ初期化はコンパイル対象にしない。BLE・Wi-Fi・角度／速度制御・Protocolは共通である。


## HC-SR04の超音波距離測定（UroaUltrasonic 0.1.0）

[ultrasonic](../lib/ultrasonic/README.md)の`HcSr04`は、教材が指定したTRIG/ECHOのGPIOを使う。`begin()`でGPIOを検査・初期化し、`read()`で10usのトリガーを生成してECHO幅からcm単位の距離を求める。2〜400cmを有効とし、未初期化・測定間隔不足・ECHO事前HIGH・タイムアウト・範囲外を戻り値で区別する。`distanceCm`・`echoDurationUs`・`initialized`はライブラリーが更新し、呼出側は参照だけを行う。`end()`でTRIGをLOWにして入力へ戻す。

測定開始は65ms以上の間隔を確保し、さらに遅い周期・複数センサーの順序・送信は教材が管理する。ECHO待機は30000usで打ち切る同期処理であり、同じタスクから呼び出す。割り込みや通信コールバックでは測定しない。通常の5V版HC-SR04のECHOは分圧またはレベル変換を通して接続する。追加の外部ライブラリー依存はない。

diagram-controlの`esp32dev`環境は`board = esp32doit-devkit-v1`を指定し、`config.h`のTRIG=GPIO25・ECHO=GPIO26を`HcSr04::Config`へ渡す。`setup()`は最新1件のキューと専用タスクを確保し、タスクだけがGPIO初期化・測定・公開値の参照を行う。XIAO・SuperMiniでは両ピンを`-1`とし、タスクを開始しない。距離を登録してSTREAMを開始すると、タスクが同期測定し、各処理後に既定100ms待つ。`loop()`のSTOP・サーボ補間にはECHO待機を追加しない。

loop()はatomicで測定有効ビットと要求世代を渡す。測定タスクは「要求世代・距離mm」の構造体をキューへコピーし、loop()は現在の世代と一致する最新結果だけを送る。開始・停止、DeviceInfo置換、接続変更・切断、キュー飽和、期限超過では世代を進め、未測定値0xFFFFから再開する。DeviceInfoの置換はSTREAMを停止する。明示STOPは従来どおりSTREAMを継続する。20Hz送信では距離を再利用するため、ヘッダー時刻は距離の測定時刻そのものではない。

diagram側は今回変更していない。ブラウザーの距離レコード受信とSTREAM要求は後続作業であり、IMUだけの通知のバイト形式は維持する。2026-11-01はユーザー指定によりテストを実行しない。ビルドと実機・通信の確認範囲は[検証記録](build-and-test.md)を参照する。
