# UroaUltrasonic

`uroa_ultrasonic.h`の`uroa::HcSr04`でHC-SR04を測定する。ArduinoフレームワークのESP32向け。追加の外部ライブラリー依存はない。

TRIGへ10usのHIGHを送り、ECHOのHIGH幅を`us / 58`でcmへ変換する。2〜400cmを有効範囲とし、整数へ丸めず`float`で保持する。[HC-SR04データシート](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf)に基づく。温度補償・平滑化は行わない。

## 配線

| HC-SR04 | 接続先 |
|---|---|
| VCC | 5V電源 |
| GND | ESP32と共通のGND |
| TRIG | 呼出側が指定する出力可能なGPIO |
| ECHO | 分圧またはレベル変換を通した入力可能なGPIO |

通常の5V版HC-SR04のECHOをESP32へ直結しない。分圧例は`ECHO → 10kΩ → GPIO入力 → 15kΩ → GND`で、5Vを約3.0Vへ下げる。TRIGは3.3VのGPIOから入力する。

ピンは教材が選び、`Config`へGPIO番号で渡す。`begin()`はGPIOの有効性・出力可否・重複を検査する。基板上のFlash・PSRAM・カメラ・USBなどとの競合は判定しないため、基板の回路図と教材の割り当てを確認する。

## API

- `HcSr04(const Config&)`: 設定をコピーする。GPIO操作は行わない。
- `begin()`: GPIOを初期化し、結果を消去する。不正なGPIOなら`false`。再呼び出し時は一度`end()`する。センサー接続の検出ではない。
- `read()`: 同期測定し、下表の結果を返す。`begin()`または前回の測定開始から65ms未満なら待機せず`TooSoon`を返す。これより遅い測定周期は教材側で決める。
- `end()`: TRIGをLOWにして入力へ戻し、結果を消去する。電源は遮断しない。

| `HcSr04::Result` | 状態・公開値 |
|---|---|
| `Ok` | `distanceCm`へ新しい距離、`echoDurationUs`へECHO幅を設定 |
| `NotInitialized` | `begin()`未成功または`end()`後。測定値は変更しない |
| `TooSoon` | 測定間隔不足。測定値は変更しない。前回値を新規結果として送らない |
| `EchoHigh` | トリガー前からECHOがHIGH。トリガーを送らず距離は`NAN`、幅は0 |
| `Timeout` | 30000us以内にECHOの開始と終了を取得できない。距離は`NAN`、幅は0 |
| `OutOfRange` | 2〜400cmの範囲外。距離は`NAN`、幅は実測値 |

`initialized`はGPIO設定済みを示す。`distanceCm`と`echoDurationUs`は`begin()`/`read()`/`end()`が更新する公開値で、呼出側は参照だけを行う。測定失敗後も初期化状態は維持し、65ms以上あけて再測定できる。

`read()`はECHO待機で最大約30msブロックする。これはリアルタイム保証ではなく、割り込み・他タスクの実行による遅延と計測誤差がある。同じタスクから初期化・測定・終了・結果参照を行い、割り込みやBLE/Wi-Fiコールバックでは呼ばない。短い制御周期が必要な処理には、そのまま組み込まない。複数センサーの相互干渉を避ける順序・間隔は教材側で管理する。

## 呼び出し例

`app/<教材>/platformio.ini`から参照する場合:

```ini
lib_deps =
    symlink://../../lib/ultrasonic
```

以下はESP32 DevKitでGPIO25/26を他用途に使っていない場合の例。他の基板ではGPIOを選び直す。

```cpp
/** @file main.cpp @brief HC-SR04の距離をシリアルへ表示する教材例。 */
#include <Arduino.h>
#include <uroa_ultrasonic.h>

/**
 * @brief 教材のGPIO割り当てを作る。
 * @return TRIG=GPIO25、ECHO=GPIO26の設定。
 */
uroa::HcSr04::Config makeSensorConfig() {
    uroa::HcSr04::Config config;
    config.triggerPin = 25;
    config.echoPin = 26;
    return config;
}

uroa::HcSr04::Config sensorConfig = makeSensorConfig();
uroa::HcSr04 sensor(sensorConfig);

/** @brief シリアルとセンサーのGPIOを初期化する。 */
void setup() {
    Serial.begin(115200);
    if (!sensor.begin()) {
        Serial.println("HC-SR04: invalid GPIO configuration");
    }
}

/** @brief 新しい測定に成功した場合だけcm単位で表示する。 */
void loop() {
    const uroa::HcSr04::Result result = sensor.read();
    if (result == uroa::HcSr04::Result::Ok) {
        Serial.printf("distance=%.1f cm\n", sensor.distanceCm);
    }
    else if (result != uroa::HcSr04::Result::TooSoon) {
        Serial.printf("HC-SR04 result=%u\n", static_cast<unsigned>(result));
    }
    delay(100);
}
```

## diagramとの境界・検証範囲

`UltrasonicDistanceSensorBlock`の単一数値出力へ後で渡せる距離を提供する。現在のブロックは単位を規定していないため、接続時はcmに統一するか呼出側で変換する。測定成功時だけ新しい値を渡す方針とし、失敗を距離0として扱わない。

このライブラリーはDeviceInfo・Telemetry・STREAM・BLE/Wi-Fi送信を実装しない。diagram-controlはDOIT ESP32 DEVKIT V1のTRIG=GPIO25・ECHO=GPIO26を`HcSr04::Config`へ渡し、専用タスクだけが初期化・測定・公開値参照を行う。距離の登録とSTREAM開始後、測定タスクは各処理後に既定100ms待機する。結果は世代付きの最新1件キューを経由してloop()へ渡し、`distanceCm * 10`を四捨五入したmm単位のuint16として送信する。未測定・失敗は0xFFFF。IMUを登録していなければIMUは不要。ライブラリーは教材の`config.h`やFreeRTOSのキューへ依存しない。diagram側の距離受信対応は別作業。

2026-11-01: ユーザー指定によりテストは実行していない。diagram-controlのesp32dev（DOIT ESP32 DEVKIT V1）でコンパイル・リンクを確認した。Upload・実機測定・通信は未確認。詳細は[検証記録](../../docs/build-and-test.md)を参照する。
