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

## 角度と連続回転速度（0.3.0以降）

同じPCA9685を1個の`Pca9685Servos`で管理し、CHごとに`states[ch].mode`を保持する。角度は`armAngle()`・`moveAngle()`、速度は`armSpeed()`・`setSpeed()`を使う。いずれも明示的に有効化する。別モードの有効CHへの指令は`WrongMode`となり、切り替えには先に`disable(ch)`が必要。

```cpp
servos.armAngle(0, 90);          // 度単位。初期受付範囲は60〜120度。
servos.moveAngle(0, 105, 1000);  // 1000msで角度を補間する。
servos.armSpeed(1);             // 中立パルス（速度指令0%）で有効化。
servos.setSpeed(1, -30);         // 方向付きの指令%。−100〜100、即時反映。
servos.setSpeed(1, 0);           // 中立パルスを維持する。
servos.disable(1);              // PWM信号を無効化する。
```

速度は実測RPMではない。負荷や電源、個体の不感帯によって実際の速度は変わる。`states[ch].speed`は%の指令値、`current`・`target`は角度モードの0.01度指令値。速度モードでは角度補間を行わず、`remaining()`は0。

FS90Rの中立と方向は、コンストラクターへ渡す前の`Config::speed[ch]`で調整する。既定は`neutralUs=1500`、`spanUs=500`、`direction=1`。パルス幅は`neutralUs + direction * percent * spanUs / 100`。変化幅は正、方向は±1、中立±変化幅は700〜2300µsの範囲とする。CHごとに異なる中立・反転を設定できる。`armSpeed()`は設定を検査し、0%で開始する。

`setSpeed(ch, 0)`は有効状態を保ち、中立パルスを出す。`disable()`・`allOff()`・STOP・切断・期限超過はPWMを無効化して速度指令を0へ戻す。停止後に入力が変わっても、自動で再有効化しない。実機の中立・回転・信号消失時の挙動は別途確認する。

旧C++呼出元向けの`arm()`・`move()`（0.01度単位）は互換窓口として残す。新しい呼出元は度単位の`armAngle()`・`moveAngle()`を使う。新しい速度操作のProtocolは[アプリ仕様](../../app/diagram-control/docs/protocol.md)を参照する。
