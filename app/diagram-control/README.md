# diagram-control

uroaのdiagramからESP32の角度・連続回転サーボとIMUをBLEまたはWi-Fiで操作するアプリ。既存の`uroa/esp32-ble-lab/firmware`をもとに、新しい共通ライブラリーの公開状態APIへ移行した。ESP32 DevKit V1、XIAO ESP32-S3 Sense、ESP32-S3 SuperMiniを対象とし、SenseではWi-Fi接続時にJPEGカメラも使える。

## 構成

| ファイル | 役割 |
|---|---|
| [include/config.h](include/config.h) | ピン、角度・パルス範囲、UUID、期限、教材用の公開設定 |
| [src/main.cpp](src/main.cpp) | 指令キュー、停止、サーボ補間、IMU通知とBLE/Wi-Fi連携 |
| [include/network.h](include/network.h)・[src/network.cpp](src/network.cpp) | WifiNetworkへ設定・カメラ処理を渡す |
| [include/camera.h](include/camera.h)・[src/camera.cpp](src/camera.cpp) | Senseのカメラ初期化・公開atomic・JPEG応答 |
| [platformio.ini](platformio.ini) | 対象基板と共通ライブラリーの相対参照 |

`.h`に宣言、`.cpp`に実装を置く。`imu.available`や`servos.states[ch]`を直接参照し、通信状態は`connected.load()`・`generation.load()`で読む。カメラも`cameraReady.load()`で参照し、値を返すだけの`cameraAvailable()`は作らない。

## ビルドと書き込み

VS Codeで`app/diagram-control/`を開き、PlatformIOの対象環境を選ぶ。既定はXIAO向けの`xiao_esp32s3`。DevKitでは`esp32dev`、SuperMiniでは`supermini_esp32s3`を指定する。以下はリポジトリールートから実行する。

```powershell
pio run --project-dir app/diagram-control -e esp32dev
pio run --project-dir app/diagram-control -e xiao_esp32s3
pio run --project-dir app/diagram-control -e supermini_esp32s3
# 書き込み先のCOM番号と対象環境を実機に合わせて指定する。
pio run --project-dir app/diagram-control -e xiao_esp32s3 -t upload --upload-port COM4
pio device monitor --project-dir app/diagram-control -e xiao_esp32s3 --port COM4
```

`pio`がPATHにない場合は`& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"`を使う。COM番号はソースへ固定していない。XIAOのMonitorにはDTR=1、RTS=0を設定し、速度は115200bpsとする。ライブラリーは`../../lib/`から参照するため、アプリだけでなくリポジトリー全体を取得する。

## 配線

[配線表](docs/wiring.md)を参照する。DevKitはSDA=GPIO21・SCL=GPIO22、XIAOはSDA=D4/GPIO5・SCL=D5/GPIO6、SuperMiniはSDA=GPIO8・SCL=GPIO9。PCA9685（0x40）とGY-521（0x68/0x69）を共通の100kHz I2Cへ接続する。サーボ電源は外部5V、I2Cのロジックとプルアップは3.3V、GNDは共通とする。

SenseのカメラにはPSRAMが必要。画像はJPEG、320×240、品質16、フレームバッファー1個。カメラなし・PSRAMなしでは利用不可として扱う。マイク・SDカードは使わない。DevKitとSuperMiniのカメラ要求はHTTP 503となる。

## diagramから使う

1. uroa側で`npm.cmd run dev`を起動し、ChromeなどWeb Bluetooth対応ブラウザーでdiagramを開く。別端末から使う場合はHTTPSが必要。
2. BLE接続ブロックで`ESP32-Diagram-Control`を選ぶ。サービスUUIDは既存BLE Labと同じで、diagram側の変更は不要。
3. サーボはServoMotorBlockのCHと角度／速度モードを選ぶ。角度は「90°で有効化」の後に60〜120度を指定し、移動時間は0〜10000ms。速度は「0%で有効化」の後に−100〜100%を指定し、時間補間はしない。CHは0〜15。モード変更はCHを無効化し、再有効化を必要とする。
4. IMUBlockを置くとdiagramがSTREAMを要求する。gyro・accを先頭のIMU出力からPlotBlockへ渡せる。このアプリは倒立用Balance通知を送らない。
5. 「全サーボ停止」またはdiagramのStopで全PWMと待機指令を止める。IMU通知は継続し、STREAM=0で通知を停止する。

## Wi-Fi接続とカメラ

BLE接続後にdiagramの「Wi-Fi設定／接続」を開き、SSIDとパスワードを設定する。OSからペアリングコードを求められた場合は、`config.h`の教材用共通値を使う。Wi-Fi認証情報はNVSに保存し、ソースやログには追加しない。

接続キーはdiagram側と一致する公開の教材用固定値を引き継ぐ。実際の秘密情報をソースへ追加するための設定ではない。NVS名前空間は`uroa-net`を維持し、既存の基板IDとWi-Fi設定を利用できる形式とする。

Wi-Fiで制御権を取得するとBLE指令を抑止し、BLEを遅延停止する。Wi-Fi制御権の解除時はBLEを再開する。サーボとIMUは同じブロックから操作する。SenseではCameraBlockからXIAOカメラを選択し、認証付きのJPEG取得を使う。HTTP=80、WebSocket=81、画像HTTP=82、mDNSとHMAC認証は共通Wi-Fiライブラリーの実装を使う。

## 停止と通信仕様

- 起動時、接続・切断時、制御権変更時、キュー飽和時は全PWMとIMU通知を停止する。再接続で自動ARMしない。
- STOPは通常の16件の指令キューを迂回する。全PWMと待機指令を停止し、IMU通知は継続する。
- 1周に通常指令を最大4件処理し、受信元と接続世代が一致するものだけ実行する。
- 有効形式の指令が5秒間ない場合、全PWMとIMU通知を停止する。diagramは通常1.5秒周期でPINGを送る。
- I2Cは初期化時のsetup()と制御ループだけで操作する。通信コールバックはコピーと停止要求を行う。
- MOVEのACKと角度は受付結果・内部指令値であり、実角度や移動完了の測定結果ではない。

バイナリ形式は[protocol v1](docs/protocol.md)を参照する。Commandは10byte、Statusは12byte、IMUは20byte。共通ライブラリーの範囲検査・補間・同期を利用する。

DeviceInfo用のWrite Characteristic（`f3641406-00b0-4240-ba50-05ca45bf8abc`、最大20byte／書き込み）も同じServiceへ公開する。2026-10-31時点では窓口の追加のみで、受信後の解析・適用・応答は実装していない。diagram側も取得・保持までとし、機器情報の送信処理は作りかけのまま維持する。

## 確認範囲

検証結果は[ビルドと検証](../../docs/build-and-test.md)へ記録する。ビルドと実機の動作確認を区別し、Upload・実BLE/Wi-Fi・カメラ・サーボ・IMUの確認は実施後に記録する。

元のMITライセンス表記は[LICENSE](LICENSE)に保持する。

## 連続回転サーボ

FS90Rでは速度0%が中立パルス、正負が回転方向を表す。実測速度ではない。「速度0で停止」は有効状態を保ち、「このCHを無効化」と全体STOPはPWMを無効化する。中立や回転方向は個体・取り付けに合わせて確認する。

CHごとの中立と反転は、`src/main.cpp`の`servoConfig()`で返す前に設定できる。

```cpp
config.speed[0].neutralUs = 1500;
config.speed[0].spanUs = 500;
config.speed[0].direction = 1;
config.speed[1].direction = -1;
```

新しい速度操作にはこのアプリの更新後のファームウェアが必要。旧BLE Labでは角度操作を引き続き利用できるが、速度操作は拒否される。通信形式は[Protocol拡張](docs/protocol.md)を参照する。

## ESP32-S3 SuperMini

VS CodeのPlatformIO Project Tasksで`supermini_esp32s3`を選び、Build・Upload・Monitorを実行する。既定環境はXIAOのままなので、環境を明示する。以下はリポジトリールートからの例。COM5は実際の接続ポートへ合わせる。

```powershell
pio run --project-dir app/diagram-control -e supermini_esp32s3
pio run --project-dir app/diagram-control -e supermini_esp32s3 -t upload --upload-port COM5
pio device monitor --project-dir app/diagram-control -e supermini_esp32s3 --port COM5
```

既存のSuperMini教材と同じGPIO8/9と4MB Flashの構成を引き継ぐ。PlatformIOの汎用S3ボード定義を基に、Flash 4MB・DIO・4MB用パーティション・USB CDCを指定し、PSRAMを前提としない。SuperMiniの製品名だけで全基板のFlash容量や配線を自動判別するものではない。

BLE表示名は`ESP32-Diagram-Control`。ServoMotorBlockの角度／速度指定、IMUBlock、BLEによるWi-Fi設定とWi-Fi制御は共通。カメラは搭載しない構成で、CameraBlockのXIAO画像取得は利用しない。起動時の期待ログは`I2C config: SDA=GPIO8 SCL=GPIO9 ...`。PWMの自動有効化は行わず、SG90は90度、FS90Rは速度0%で明示的に有効化する。

新しい環境のビルド結果と実機確認の範囲は[検証記録](../../docs/build-and-test.md)を参照する。
