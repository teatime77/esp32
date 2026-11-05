# UroaProtocol

`uroa_protocol.h`の`uroa::Protocol`は、protocol v1と機器種別付きTelemetry version=3の読み書きを担当する。Arduino・BLE・Wi-Fiには依存しない。

- `command(data, length)`は10バイトをコピーする。不正な長さではversion=0を返す。
- `valid(command)`はversion、reserved、opcodeを検査する。CH・値・時間の検査は教材／機器側で行う。
- PythonBlockの開始・停止で使う操作の契約は[Opの仕様コメント](include/uroa_protocol.h)、教材側の処理は[main.cpp](../../app/diagram-control/src/main.cpp)を参照する。
- `urgentStop(data, length)`は従来互換の優先STOP判定。reserved等を検査する通常経路とは意図的に分かれている。
- `status(out, ...)`には12バイト以上の出力領域を渡す。
- `deviceTelemetryLength(sensorTypes, sensorCount)`は、サーボを除いたDeviceInfo順のセンサー種別から必要な通知長を計算する。未対応種別・不正な配列・サイズのオーバーフローでは0を返す。センサー0件はヘッダー8バイト。
- `deviceTelemetry(out, capacity, flags, sequence, now, sensorTypes, sensorCount, imuRaw, distanceMm)`はversion=3のヘッダーと、指定順のIMU（種別0・13バイト）／超音波距離（種別2・3バイト）を連結する。IMUの14バイト生データと距離は呼出側が測定して渡し、同種別の複数レコードでは同じ物理センサーの結果を共有する。全引数と容量を検査してから書く。成功時は使用バイト数、引数不正・未対応種別・容量不足では0を返し、出力を変更しない。IMUを含まなければ`imuRaw=nullptr`でよい。

角度は0.01度、時間はms。符号付きのIMU生データもビット列のままlittle endianへ変換する。シーケンス番号の管理・送信先の判断・ACKは教材側の責任。

ヘッダーは`include/uroa_protocol.h`、実装は`src/uroa_protocol.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。

## 速度操作の拡張（0.3.0以降）

version=1の10byte指令へ`ARM_SPEED=8`（value=0・duration=0）と`SET_SPEED=9`（value=符号付き0.01%、−10000〜10000・duration=0）を追加する。負数はlittle endianの2の補数。`readSigned16()`で読み出す。既存の操作1〜7、UUID、サイズ、角度の単位は維持する。

12byte Statusのflags bit4（`FLAG_SPEED=0x10`）は指定CHの速度モードを示す。この場合current/targetは符号付き0.01%で読み、残り時間は0。角度モードでは従来どおり符号なし0.01度。コード7はモード不一致で、無効化してから別モードで有効化する。

旧ファームウェアは新操作をFORMATとして拒否する。Web側は速度操作を角度操作へ置き換えず、対応ファームウェアが必要であることを表示する。詳細は[Protocol仕様](../../app/diagram-control/docs/protocol.md)を参照する。

## IMUと超音波距離の混在（0.4.0）

従来の`raw, imuCount`引数を、`sensorTypes, sensorCount, imuRaw, distanceMm`へ置き換えた。通信上のversion=3ヘッダーとIMUレコードは維持する。通知長は`8 + 13 × IMU数 + 3 × 距離数`。サーボ種別128は呼出側で配列から除外する。IMU9AxisとCameraは未対応。

距離レコードは`[2, mmの下位バイト, mmの上位バイト]`。有効値は20〜4000mm、未測定・失敗は`InvalidDistanceMm=0xFFFF`とする。0やその他の範囲外値を距離として渡すと生成を拒否する。呼出側で`distanceCm * 10`を四捨五入する。1mm単位の表現であり、1mmの測定精度を保証するものではない。

```cpp
uint8_t packet[24];
const uint8_t sensorTypes[] = {Protocol::UltrasonicDeviceType, Protocol::IMU6AxisDeviceType};
// imuRawは別途取得した14バイト。距離1234mm、距離→IMUの順で24バイトを生成する。
const size_t length = Protocol::deviceTelemetry(
    packet, sizeof packet, 0, 1, millis(), sensorTypes, 2, imuRaw, 1234);
```

Protocolはセンサーの初期化・周期・失敗判定・測定タスク・接続世代を管理しない。diagram-control側が行う。diagramの距離レコード受信対応は別作業で、今回変更していない。ビルド・実機・通信の確認範囲は[検証記録](../../docs/build-and-test.md)を参照する。
