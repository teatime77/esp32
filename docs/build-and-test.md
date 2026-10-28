# ビルドと検証

## 利用プロジェクトからの参照

ルートは共通ライブラリーと教材アプリをまとめる。PlatformIOの設定と実行コードは各`app/<教材>/`に置く。各アプリは必要な共通ライブラリーを参照する。

`app/<教材>/`からの相対参照例:

```ini
lib_deps =
    symlink://../../lib/protocol
    symlink://../../lib/imu
    symlink://../../lib/ble
    symlink://../../lib/plot
```

別リポジトリーの利用プロジェクトからは、そのプロジェクトを起点に相対パスを指定するか、`symlink://D:/usr/prj/esp32/lib/imu`などを指定する。`symlink://`は共通ソースを編集した結果を次のビルドに反映する。外部依存は各ライブラリーの`library.json`から導入する。

利用側ではVS Code + PlatformIOを使用し、対象環境を明示してビルド・書き込みする。書き込みはユーザーが指示した場合のみ行う。

```powershell
pio run --project-dir <利用プロジェクト> -e <対象環境>
pio run --project-dir <利用プロジェクト> -e <対象環境> -t upload
```

## 検証記録

2026-10-28: 6ライブラリーを参照する一時プロジェクトで、各ライブラリーのコンパイルと検証用ファームウェアのリンクを確認した。ビルド用の一時プロジェクトはこのリポジトリーへ含めず、倒立アプリも作成していない。

| 対象 | PlatformIO環境 | 結果 |
|---|---|---|
| ESP32 DevKit | `esp32dev` / `espressif32@6.10.0` / Arduino | 成功 |
| XIAO ESP32-S3 | `seeed_xiao_esp32s3` / `espressif32@6.10.0` / Arduino | 成功 |
| 配置・API | 6ライブラリーのヘッダーと実装、JSON、単純アクセサの除去、公開atomicの型 | 静的確認済み |
| 文書・書式 | リポジトリー内の文書リンク、clang-format | 確認済み |

検証用コードは公開状態の参照、`std::atomic`の型、コンストラクターの外部定義、Protocol・Plotなどを参照する。各ライブラリーの全操作や通信シーケンスを実行した試験ではない。ProtocolとPlotの実装はコメント・空白を除いて元のコードと一致することも確認した。

検証環境では、既存PlatformIOキャッシュへの書き込み権限が必要だった。また、XIAO側の長いSDK参照パスによってコンパイラーの子プロセス起動が失敗したため、検証用に既存パッケージディレクトリーへの短いジャンクションを作り、`PLATFORMIO_PACKAGES_DIR`をそのパスに指定して再実行した。並列数は2とした。ライブラリーのコードや依存バージョンを変更せずに解消し、リポジトリーに環境固有のパッケージパスは固定していない。

コンパイルとリンクの成功は、実機への書き込み、BLE/Wi-Fiの実通信、I2C機器・サーボの動作確認を意味しない。


## diagram-control（2026-10-28）

アプリの[操作・配線](../app/diagram-control/README.md)と[通信形式](../app/diagram-control/docs/protocol.md)を参照する。リポジトリールートから次を実行する。

```powershell
pio run --project-dir app/diagram-control -e esp32dev
pio run --project-dir app/diagram-control -e xiao_esp32s3
```

作成したアプリと共通ライブラリーを一時作業フォルダーに同じ相対配置で置き、PlatformIOでコンパイル、リンク、サイズ確認、`firmware.bin`生成を確認した。PlatformIOは`espressif32@6.10.0`、Arduinoフレームワークを使用した。

| 環境 | 結果 | RAM | Flash |
|---|---|---|---|
| `esp32dev` | SUCCESS | 58,968 / 327,680 bytes | 1,154,573 / 1,310,720 bytes |
| `xiao_esp32s3` | SUCCESS | 59,304 / 327,680 bytes | 1,054,245 / 3,342,336 bytes |

検証環境では、既存PlatformIOキャッシュへの書き込み権限と、前述の短いパッケージ参照パスを使用した。並列数は4。検証用のパスをリポジトリーの設定へ固定せず、生成物も移行先へ含めていない。

以下は静的に確認した。

- diagramの6種のUUIDおよび教材用共通設定との一致。
- 2基板の選択、共通ライブラリーの相対参照、`.h`と`.cpp`の配置、新しい公開状態APIの使用。
- コード書式、文書リンク、共通ライブラリーのコードを変更していないこと。
- 指令はloop()で処理し、STOPを優先すること、現在の通信経路・接続世代を照合すること、サーボ自動ARMを行わないことをソースで確認した。

起動、サーボの移動・停止、IMU通知、BLE/Wi-Fi切り替え、NVSの引継ぎ、カメラJPEG、Android・ブラウザーとの実通信は未確認。ハードウェアを模擬して実行する試験も行っていない。今回、実機への書き込みは実施していない。
