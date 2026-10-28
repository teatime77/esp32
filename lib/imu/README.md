# UroaIMU

`uroa_imu.h`の`uroa::Mpu6050`へ教材が初期化した`TwoWire&`を渡す。`begin()`は0x68、次に0x69を探索し、WHO_AM_Iを検査する。PLL、DLPF約44Hz、内部100Hz、加速度±2g、角速度±250度/秒を設定する。

```cpp
uroa::Mpu6050 imu(Wire);
// setup(): Wire.begin(SDA_PIN, SCL_PIN); imu.begin();
uint8_t raw[14];
// 制御ループ内: if (imu.readRaw(raw)) { /* 利用またはProtocol::telemetryへ渡す */ }
```

`readRaw(raw)`は加速度6・温度2・角速度6の14バイトをセンサーのbig endianのまま取得する。失敗すると`available`はfalseになり、自動再初期化しない。再試行は明示的に`begin()`を呼ぶ。取得周期・STREAM・通信応答は教材側が担当する。I²Cアクセスはサーボと同じ制御タスクで行う。

公開値は`imu.available`と`imu.address`で読む。I2C設定の変更・再初期化は`begin()`で行い、これらの値へアプリから代入しない。

ヘッダーは`include/uroa_imu.h`、実装は`src/uroa_imu.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。
