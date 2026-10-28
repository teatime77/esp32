# UroaServo

`uroa_servo.h`の`uroa::Pca9685Servos`はPCA9685の16CHを操作する。`TwoWire&`と`Config`をコンストラクターに渡す。ピンを固定せず、I²C開始・クロック設定は教材側が行う。外部依存のバージョンは`library.json`に宣言する。

```cpp
uroa::Pca9685Servos::Config config;
// configのaddress、frequency、minAngle/maxAngle、minPulseUs/maxPulseUsを教材に合わせる。
uroa::Pca9685Servos servos(Wire, config); // 稼働中保持する
// setup(): Wire.begin(SDA_PIN, SCL_PIN); servos.begin();
// 明示的な操作で: servos.arm(0, 9000);
// 受付後: servos.move(0, 10500, 1000, millis());
// loop(): servos.tick(millis());
```

角度は0.01度。`minPulseUs/maxPulseUs`は公称0度／180度の換算幅で、受付可能角度とは別。`begin()`は全CHをOFFにし、自動ARMしない。`arm()`は即時出力、`move()`は最大10000msの線形補間、0msなら即時出力。既定の受付範囲は60～120度。

`disable(ch)`は個別停止、`allOff()`は全停止。`tick(now)`は20ms以上経過したときだけ更新し、I²C出力失敗のCH、または-1を返す。出力失敗時は全CHへのOFFを試みて`available`をfalseにする。機器へのOFF書き込みは電源遮断や実角度の確認を意味しない。

`State`は内部指令値で、実角度ではない。`Result`の通信応答への変換は教材側で行う。停止後も内部角度・目標値は保持する。I²Cを使うメソッドは同じ制御タスクから呼び、BLE／HTTPコールバックから呼ばない。

CH0〜15の状態は`servos.states[ch]`で読む。添字の範囲は呼出元で確認する。`available`と`states`へ直接代入しても機器は操作されないため、変更は`arm()`・`move()`・`disable()`・`allOff()`で行う。`anyArmed()`と`remaining()`は検査・計算を行うため残している。

ヘッダーは`include/uroa_servo.h`、実装は`src/uroa_servo.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。
