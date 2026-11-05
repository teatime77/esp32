# UroaDisplay

`uroa_display.h`の`uroa::Ssd1306Display`でI2C接続のSSD1306を操作する。描画・初期化・分割転送の仕様と制約は[ヘッダー](include/uroa_display.h)、実装は[uroa_display.cpp](src/uroa_display.cpp)を参照する。Adafruit SSD1306・GFX・BusIOのバージョンは[library.json](library.json)で指定する。

I2Cのピン・開始・タイムアウトは教材側で決め、同じタスクから操作する。diagram-controlのESP32 DevKitではSDA=GPIO21・SCL=GPIO22、既定の表示器は128×64・アドレス`0x3C`。表示器の電源条件を確認し、ESP32とGNDを共有してSDA/SCLを接続する。

## C++での利用例

`app/<教材>/platformio.ini`から参照する場合:

```ini
lib_deps =
    symlink://../../lib/display
```

```cpp
/** @file main.cpp @brief SSD1306へ文字と円を表示する例。 */
#include <Arduino.h>
#include <uroa_display.h>

uroa::Ssd1306Display::Config displayConfig;
uroa::Ssd1306Display screen(Wire, displayConfig);
bool sending = false;

/** @brief 教材のI2Cを開始し、RAMへ描画する。 */
void setup() {
    if (!Wire.begin(21, 22)) {
        return;
    }
    Wire.setClock(100000);
    Wire.setTimeOut(30);
    if (screen.begin() != uroa::Ssd1306Display::Result::Ok) {
        return;
    }
    const char text[] = "Hello SSD1306";
    screen.print(text, sizeof text - 1);
    screen.drawCircle(64, 40, 15, 1);
    sending = true;
}

/** @brief 画素を分割転送し、各回で他の制御へ戻る。 */
void loop() {
    if (sending) {
        const auto result = screen.displayStep();
        sending = result == uroa::Ssd1306Display::Result::Pending;
        // Okで完了。その他の結果はエラーなので、必要ならbegin()から再試行する。
    }
}
```

## PythonのDisplayからの利用

diagram-controlは[DeviceNative](../../app/diagram-control/src/device_native.cpp)を通してこのライブラリーを呼ぶ。API宣言はuroaの[device.pyi](../../../uroa/diagram/python/device.pyi)。既存のクラス・メソッド・ABI指紋は変更しない。

このPythonサブセットでは機器APIをimportせずに利用する。

```python
screen = Display()
screen.set_text_color(1)
screen.set_cursor(0, 0)
screen.print("Hello SSD1306")
screen.draw_line(0, 12, 127, 12, 1)
screen.draw_rect(4, 20, 30, 20, 1)
screen.draw_circle(60, 32, 12, 1)
screen.fill_circle(100, 32, 8, 1)
screen.display()
```

文字は標準GFXフォントを使用し、日本語の表示には対応しない。描画後に`display()`を呼ぶと画面へ反映する。複数の`Display()`は同じ物理表示器とバッファを共有する。VMの文字列処理・停止・分割転送の接続仕様はDeviceNativeのコメントを参照する。

ビルド結果と未確認範囲は[検証記録](../../docs/build-and-test.md)を参照する。
