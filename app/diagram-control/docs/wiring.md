# 配線表

以下はdiagram-control用のDevKit V1とXIAO ESP32S3／Senseの配線表です。SuperMiniは後述の専用配線を参照します。2026-10-20、XIAOのSDA=D4（GPIO5）、SCL=D5（GPIO6）の設定をソースで再確認しました。PCA9685とMPU6050／GY-521は同じI²Cバスへ並列に接続します。

| DevKit V1の接続元 | XIAO ESP32S3／Senseの接続元 | 接続先 | 備考 |
|---|---|---|---|
| GPIO21 | D4／GPIO5 | PCA9685 SDA / GY-521 SDA | I²Cデータ共通 |
| GPIO22 | D5／GPIO6 | PCA9685 SCL / GY-521 SCL | I²Cクロック共通 |
| 3V3 | 3V3_OUT／3V3 | PCA9685 VCC / GY-521 VCC | 制御用3.3V。GY-521の電源条件は実基板の仕様も確認 |
| GND | GND | PCA9685 GND / GY-521 GND / 外部電源− | 共通GND |
| 外部安定化5V電源＋ | 外部安定化5V電源＋ | PCA9685端子台 V+ | サーボ用。VCCにはつながない |
| 外部電源− | 外部電源− | PCA9685端子台 GND | サーボ電流の戻りは直接配線推奨 |
| PCA9685 CH0 PWM | PCA9685 CH0 PWM | SG90信号線（通常橙／黄） | 実物の配線色を確認 |
| PCA9685 CH0 V+ | PCA9685 CH0 V+ | SG90電源線（通常赤） | 5V |
| PCA9685 CH0 GND | PCA9685 CH0 GND | SG90GND線（通常茶／黒） | コネクターの向きを確認 |
| USB | USB | 各ボードのUSB端子 | PCまたはUSB電源から給電 |

- OEは基板上でLOWにプルダウンされるAdafruitタイプなら未接続で利用可能。互換基板の仕様を確認してください。
- GY-521のXDA・XCL・INTは未使用。一般的な基板はAD0がLOWで0x68。ソフトは0x68と0x69を検出します。
- PCA9685のアドレスジャンパーは初期状態（0x40）を想定。
- PCA9685側のI²Cプルアップも3.3Vにします。5VでSDA/SCLをプルアップしないでください。
- GY-521の基板上レギュレーターの個体差で3.3V入力時に不安定な場合は、実基板の回路・IC電源を確認してください。単純に5Vへ変更してI²Cを5Vにしないでください。
- ブレッドボードの電源レールが途中で分断されていないか、電源OFFで導通確認。
- ESP32の3V3やUSB経由でサーボ電源を供給しないでください。
- サーボを増やすときは電源容量と配線を確認し、まず1個ずつ追加してください。

ソフト上のPWM停止とは別に、サーボ用5V電源を切れるスイッチがあると扱いやすくなります。

## XIAO ESP32S3／Senseで使う場合

上の表のXIAO列を使う。D4/D5は基板の表記、GPIO5/6はESP32-S3の番号である。[Seeed公式ピン表](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)を参照する。

PCA9685のV+とSG90には引き続き外部5V電源を使う。I²Cのプルアップは3.3V。サーボ用5VはXIAOの3V3へ接続しない。外部MPU-6050／GY-521をそのまま使う。SenseのカメラはWi-Fi接続時に初期化し、マイク・SDカードは使用しない。[アプリのWi-Fi接続手順](../README.md)も参照する。

`platformio.ini`の`xiao_esp32s3`環境を選ぶと、`config.h`がI²CをGPIO5/6へ切り替える。`esp32dev`環境はGPIO21/22のまま。

両ボードでPCA9685のサーボ制御とMPU6050の初期化・読み出しは同じ処理を使う。I²Cは100kHz、PCA9685は`0x40`、MPU6050は`0x68`または`0x69`を探索する。XIAO向けファームウェアをUploadし、Monitorで次のログを確認する。これは期待する表示例で、実機の成功記録ではない。

```text
I2C config: SDA=GPIO5 SCL=GPIO6 clock=100000 Hz init=OK
I2C: ...
PCA9685=OK MPU6050=OK (0x68)
```

MPU6050のアドレスが`0x69`の基板では末尾も`0x69`となる。`MISSING`の場合は電源・GND・SDA/SCL・アドレスを確認してから再起動する。diagramではDevKitと同じServoMotorBlock・IMUBlock・PlotBlockを使い、サーボは接続後に「90°で有効化」を明示する。起動・再接続だけでは動かさない。

BLE利用時は付属アンテナをXIAOのWiFi/BTアンテナ端子へ取り付ける。[Seeed公式の取り付け説明](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#installation-of-antenna)を参照する。

## ESP32-S3 SuperMiniで使う場合

`supermini_esp32s3`環境を選ぶと、共通のPCA9685・GY-521のSDAをGPIO8、SCLをGPIO9へ接続する。DevKit用のGPIO21/22、XIAO用のGPIO5/6とは配線が異なる。

| SuperMiniの接続元 | 接続先 |
|---|---|
| GPIO8 | PCA9685 SDA・GY-521 SDA |
| GPIO9 | PCA9685 SCL・GY-521 SCL |
| 3V3 | PCA9685 VCC・3.3V入力に対応するGY-521 VCC |
| GND | PCA9685・GY-521・サーボ外部電源の共通GND |
| 外部5V | PCA9685のサーボ電源V+ |

角度サーボSG90と連続回転サーボFS90RをPCA9685の任意のCHへ接続し、diagram側でCHと角度／速度モードを合わせる。I2Cのプルアップは3.3V、サーボは外部電源を使う。SuperMini環境はカメラ・PSRAMを使わない。USB接続・書き込み方法は[アプリREADME](../README.md)を参照する。実機の中立パルスと回転方向は別途確認する。
