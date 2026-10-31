# ESP32教材と共有ライブラリー

VS CodeとPlatformIOで開発するESP32向けの教材アプリと共通C++ライブラリー。共有部分はuroaの`esp32/lib/`をもとに、ヘッダーと実装を分離し、単純なアクセサを公開メンバーへ変更した。6ライブラリーと、diagramから指示して動かす[diagram-control](app/diagram-control/README.md)を配置する。倒立ロボットのアプリは含めない。

```text
esp32/
├─ AGENTS.md
├─ docs/
├─ app/
│  └─ diagram-control/
└─ lib/
   ├─ ble/       # BleTransport
   ├─ wifi/      # WifiNetwork
   ├─ protocol/  # Protocol
   ├─ servo/     # Pca9685Servos
   ├─ imu/       # Mpu6050
   └─ plot/      # PlotTelemetry
```

各ライブラリーは`library.json`、`README.md`、`include/uroa_*.h`、`src/uroa_*.cpp`を持つ。PlatformIOの既定値で`include/`と`src/`を利用できる。各ライブラリーのREADMEに呼び出し例と制約を記載する。

| ライブラリー | 目的 |
|---|---|
| [ble](lib/ble/README.md) | BLE指令受信、接続世代、Status/Telemetry通知、認証済み設定窓口 |
| [wifi](lib/wifi/README.md) | Wi-Fi設定、NVS、mDNS、認証付きHTTP/WebSocket |
| [protocol](lib/protocol/README.md) | 指令・Status・IMUのバイナリ形式 |
| [servo](lib/servo/README.md) | PCA9685の角度／連続回転サーボ16CH、明示ARM、角度補間、速度指令、停止 |
| [imu](lib/imu/README.md) | MPU-6050の探索・初期化・14バイト取得 |
| [plot](lib/plot/README.md) | 6系列を20バイトのプロット通知へ変換 |

開発は[コーディング規約](docs/coding-style.md)、構成とAPI変更は[設計](docs/architecture.md)、ビルド方法と検証範囲は[検証記録](docs/build-and-test.md)を参照する。

外部依存は`library.json`にバージョンを指定する。Arduino IDEのスケッチブックやグローバルライブラリーの配置には依存しない。現在の対象はPlatformIOの`espressif32`・`framework = arduino`であり、他のArduino製マイコンへの移植を確認済みとはしない。

共有ライブラリーの元のMIT表記は[lib/LICENSE](lib/LICENSE)に保持している。リポジトリーの[LICENSE](LICENSE)も参照する。
