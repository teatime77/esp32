# diagram-control

uroaのdiagramからESP32の角度サーボとIMUをBLEまたはWi-Fiで操作するアプリ。既存の`uroa/esp32-ble-lab/firmware`をもとに、新しい共通ライブラリーの公開状態APIへ移行した。ESP32 DevKit V1とXIAO ESP32-S3 Senseを対象とし、SenseではWi-Fi接続時にJPEGカメラも使える。

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

VS Codeで`app/diagram-control/`を開き、PlatformIOの対象環境を選ぶ。既定はXIAO向けの`xiao_esp32s3`。DevKitでは`esp32dev`を指定する。以下はリポジトリールートから実行する。

```powershell
pio run --project-dir app/diagram-control -e esp32dev
pio run --project-dir app/diagram-control -e xiao_esp32s3
# 書き込み先のCOM番号と対象環境を実機に合わせて指定する。
pio run --project-dir app/diagram-control -e xiao_esp32s3 -t upload --upload-port COM4
pio device monitor --project-dir app/diagram-control -e xiao_esp32s3 --port COM4
```

`pio`がPATHにない場合は`& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"`を使う。COM番号はソースへ固定していない。XIAOのMonitorにはDTR=1、RTS=0を設定し、速度は115200bpsとする。ライブラリーは`../../lib/`から参照するため、アプリだけでなくリポジトリー全体を取得する。

## 配線

[配線表](docs/wiring.md)を参照する。DevKitはSDA=GPIO21・SCL=GPIO22、XIAOはSDA=D4/GPIO5・SCL=D5/GPIO6。PCA9685（0x40）とGY-521（0x68/0x69）を共通の100kHz I2Cへ接続する。サーボ電源は外部5V、I2Cのロジックとプルアップは3.3V、GNDは共通とする。

SenseのカメラにはPSRAMが必要。画像はJPEG、320×240、品質16、フレームバッファー1個。カメラなし・PSRAMなしでは利用不可として扱う。マイク・SDカードは使わない。DevKitのカメラ要求はHTTP 503となる。

## diagramから使う

1. uroa側で`npm.cmd run dev`を起動し、ChromeなどWeb Bluetooth対応ブラウザーでdiagramを開く。別端末から使う場合はHTTPSが必要。
2. BLE接続ブロックで`ESP32-Diagram-Control`を選ぶ。サービスUUIDは既存BLE Labと同じで、diagram側の変更は不要。
3. サーボはServoMotorBlockのCHを設定し、「90°で有効化」を明示してから角度を送る。初期範囲は60〜120度、移動時間は0〜10000ms、CHは0〜15。
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

## 確認範囲

検証結果は[ビルドと検証](../../docs/build-and-test.md)へ記録する。ビルドと実機の動作確認を区別し、Upload・実BLE/Wi-Fi・カメラ・サーボ・IMUの確認は実施後に記録する。

元のMITライセンス表記は[LICENSE](LICENSE)に保持する。
