# UroaPlot

`uroa::PlotTelemetry`は6個の数値を20バイトの通知へまとめるクラス。履歴や描画はdiagramのPlotBlockが担当する。通信は送信コールバックで渡し、BLEや教材の設定には依存しない。同じタスクから`setRate()`と`publish()`を呼ぶ。

```cpp
#include <uroa_plot.h>
uroa::PlotTelemetry plot(1, [](const uint8_t *data, size_t length) {
    bluetooth.publishTelemetry(data, length);
});
// STREAMの値を反映。0=停止、1〜20Hz。接続がなければ0にする。
plot.setRate(10);
const float values[6] = {angle, target, rate, error, leftPercent, rightPercent};
plot.publish(millis(), values);
```

`publish()`は周期が来て送信関数を呼んだ場合にtrueを返す。BLEでの到達・描画の成功は表さない。初回と周期変更後は次の呼び出しで送信し、以降は`millis()`の差を符号なしで比較する。停止中に値を蓄積したり、遅れた通知をまとめて再送したりしない。sequenceは16bitで周回する。

`encode(packet, schema, sequence, now, values)`は通信なしで利用できる。schema=0、NaN、無限大、表現範囲外の値を拒否し、出力バッファを変更しない。数値は0.01刻みに丸める。利用側でschemaごとの系列名と単位を定め、diagramにも対応するパーサーとStructを登録する。任意のschemaを自動表示する機能はない。

## 通知形式

既存のIMU通知と同じTelemetry Characteristicへ送る。先頭バイトで区別し、IMU用のversion=1は変更しない。既定ATT MTU 23のペイロード20バイトに収まる。

| Offset | 型 | 内容 |
|---|---|---|
| 0 | u8 | プロット通知version=2 |
| 1 | u8 | schema。現在diagramは1（倒立）を解釈する |
| 2 | u16 | sequence、little endian |
| 4 | u32 | 取得時刻millis、little endian、周回あり |
| 8,10,12,14,16,18 | i16×6 | 数値×100、little endian。−327.68〜327.67 |

schema=1は順に姿勢角°、起動時の目標角°、bias補正後のX角速度°/s、姿勢誤差°、左速度指令%、右速度指令%。左右は車体の進行方向で符号を揃えた値で、右サーボの鏡像配置によるパルス反転は含まない。実測RPM・車速ではない。

diagramのパーサーが`Balance`構造体へ変換し、IMUBlockの2番目の`balance`出力からPlotBlockへ渡す。既存ファームウェアはこの出力を更新しない。IMUBlockの先頭出力と既存JSONの配線は維持する。

倒立教材のアプリは含めない。[diagram-control](../../app/diagram-control/README.md)は通常のIMU通知を送り、倒立用のPlot通知は送らない。上記schemaの説明は通信形式であり、倒立の動作確認結果ではない。

ヘッダーは`include/uroa_plot.h`、実装は`src/uroa_plot.cpp`。設定変更・I2C・通信を伴うメソッドの実装は`.cpp`に置く。
