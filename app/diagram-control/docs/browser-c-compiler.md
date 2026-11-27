# ブラウザ用Cコンパイラーの選定

選定日: 2026-11-27。

[C数値カーネルの仕様](numeric-kernel.md)に対して、Cコンパイラーにwcc、生成Wasmの最適化にBinaryenを採用する。ブラウザ内で完結し、既存のWorker接続を利用できることを優先する。複数コンパイラーの性能比較は行わない。コンパイラーの選定は完了し、Web側への導入と実行確認は後続作業とする。

## 1 採用ツールと固定版

| 役割 | 採用するもの | 固定する版 | ライセンス |
|---|---|---|---|
| CからWasmへの変換 | [tyfkda/xccのwcc](https://github.com/tyfkda/xcc) | コミット`659a1773f20499864a8c3fdc7cd25cb506092bc1` | MIT |
| Wasmの最適化 | [npmパッケージbinaryen](https://github.com/AssemblyScript/binaryen.js) | `132.0.0` | Apache-2.0 |

wccはリポジトリーの固定スナップショットから用意する。package.jsonにある`Wcc`の`0.1.0`だけではソースを識別できないため、npmの同名パッケージや移動するmain/masterを取得元にしない。[採用コミット](https://github.com/tyfkda/xcc/commit/659a1773f20499864a8c3fdc7cd25cb506092bc1)を取得元とする。

Binaryenは`^`や`~`を付けない`132.0.0`をWeb側の依存に指定し、package-lock.jsonも一緒に保持する。選定時にnpmレジストリーで確認した配布物のintegrityは次のとおり。

```text
sha512-4Usg7yEvwO3FMso3Oov6qkOOHSod1IEv+boKkh77kTljM+eNBouqCTbqWswGzbG+vDNO/b/v2C4gfEh1WgB9Lg==
```

Binaryenの取得元は`https://registry.npmjs.org/binaryen/-/binaryen-132.0.0.tgz`。この記録は選定版の指定であり、現時点でWeb側のpackage.jsonやpackage-lock.jsonへ追加済みという意味ではない。

## 2 この構成を選ぶ理由

- wccには、ブラウザ内でCをWasmへ変換する[公開デモ](https://tyfkda.github.io/xcc/)と、ソースファイルの書込み・コンパイル・出力読出しを行うWorker接続がある。
- 使用するCを数値型・ポインター・条件分岐・ループ・静的に解決できる関数呼出しへ限定できるため、C++の言語機能や標準ライブラリーをブラウザへ持ち込む必要がない。
- wcc単体の最適化に依存せず、Binaryenで計算用Wasmにfast mathと最適化を適用できる。
- wccのエディター、Alpine.js、画面分割、IDE用UIは取り込まず、コンパイラー資産とWorker接続だけを利用する。
- Wasmはブラウザの標準WebAssembly APIで実行し、別の数値インタープリターを追加しない。

軽量さはこの構成を選ぶ方針であり、配布量や使用メモリーの数値を実測済みとして扱わない。初回読込み量と導入の成立確認は行うが、別コンパイラーとの比較計画は追加しない。

## 3 ソースで確認した対応範囲

採用コミットの公開ソースとBinaryen 132.0.0の型定義を確認した。ここでの確認はソース上の対応であり、生成カーネルをブラウザで実行した結果ではない。

| 必要なもの | 確認した接続・実装 |
|---|---|
| int32とfloat32の演算、配列データ | [gen_wasm_expr.c](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/gen_wasm_expr.c)のi32・f32処理、f32 load/store |
| Cソースの供給と生成Wasmの読出し | [wcc_runner.ts](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/www/wcc_runner.ts)のsetUp・writeFile・compile・readFile |
| mainを実行せず公開関数を呼ぶ形式 | [wcc.c](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/wcc.c)の空のentry-point指定と公開関数指定 |
| 計算用メモリーへのアクセス | [wasm_linker.c](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/wasm_linker.c)のメモリーexport |
| ブラウザ内のコンパイル | [wasi_worker.ts](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/www/wasi_worker.ts)と[wa_proc.ts](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/www/wa_proc.ts)のWASI実行 |
| fast mathとWasmの最適化 | [binaryen 132.0.0の型定義](https://unpkg.com/binaryen@132.0.0/index.d.ts)のsetFastMath・setOptimizeLevel・setShrinkLevel・readBinary・optimize・emitBinary・dispose |

必要な制御文・関数・数値処理はC生成側の対応範囲に限定する。wccのC言語全体の互換性を実証する作業や、未対応機能を取り込むためのコンパイラー改造は初期導入の前提にしない。

## 4 ブラウザへの組込み方法

### 4.1 wccの資産と接続コード

採用コミットの[Makefile](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/Makefile)にある`make assets`で、コンパイラー資産の`wccfiles.zip`を開発環境で用意する。ブラウザ自身でwccをビルドしない。

[lib_list.json](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/src/wcc/www/lib_list.json)で定義された、`/usr/bin/cc`のWasm、ヘッダー、`wcrt0.a`、`wlibc.a`を初期資産とする。初期導入では資産の独自削減やWASIホストの自作を行わず、既存の組合せを利用する。

取り込む接続コードはwcc_runner.ts、wasi_worker.ts、wa_proc.tsを基にする。`?worker`など上流のビルド方式に依存するimportだけを、uroa側のWorker配布方法へ合わせる。IDE用のmain.tsや画面部品は取り込まない。資産URLは配置先から決められるようにし、公開デモの更新されるファイルを実行時に直接参照しない。

接続コードが使う依存は、採用コミットのpackage-lock.jsonに合わせて次の版に固定する。実際に組み込む際には推移依存もWeb側のロックファイルへ保持する。

| 依存 | 固定版 | 役割 |
|---|---|---|
| `@wasmer/wasi` | `0.12.0` | コンパイラー用WASIホスト |
| `@wasmer/wasmfs` | `0.12.0` | コンパイラーの仮想ファイルシステム |
| `fflate` | `0.8.3` | コンパイラー資産の展開 |
| `path-browserify` | `1.0.1` | 仮想パス処理 |
| `buffer` | `6.0.3` | WASI／ファイル処理のブラウザ依存 |

### 4.2 コンパイルと最適化

接続処理は次の順序に固定する。

1. WccRunnerのsetUpで固定資産を仮想ファイルシステムへ用意する。
2. writeFileで共通生成Cと固定の割当て・解放用接続コードを書き込む。
3. compileでWasmを生成する。entry-pointは空にし、カーネルのrun関数と割当て・解放の関数を公開する。
4. コンパイルの終了コードと診断を受け取り、成功時だけreadFileでWasmを取得する。
5. Binaryenで取得した計算用Wasmを最適化する。
6. 最適化済みWasmを実行用Workerへ渡し、メモリーと公開関数を接続してからVMを開始する。

wccの初期オプションは`-o kernel.wasm --entry-point= -e <公開関数名のカンマ区切り>`を基準とする。wccにClang用の`-O3`や`-ffast-math`を渡さず、ブラウザのfast mathは後段のBinaryenで指定する。補助的なメモリー割当てとライブラリー初期化は、WebAssemblyのstart sectionを含む生成物に従って接続し、カーネル関数を初期化目的で自動実行しない。

Binaryenは`setOptimizeLevel(3)`、`setShrinkLevel(0)`、`setFastMath(true)`を用いる。readBinaryで読み、optimize、emitBinaryの順に処理し、最後にdisposeで最適化用モジュールを解放する。これらの設定を使うのは計算用Wasmだけであり、VM用Wasmをこの経路へ渡さない。

wccは既存RunnerのWorkerでコンパイルする。BinaryenもWorker内で実行する。接続処理を直列化し、Binaryenのプロセス共通設定が別の最適化へ混ざらないようにする。数値カーネルの実行はVMと同じ実行用Worker内で同期的に完了させ、要素ごとのメッセージやTypeScript呼出しを行わない。

### 4.3 配布とライセンス

wccfiles.zipと接続コードは自サイトの固定バージョン用パスから配布する。Binaryenはnpmの固定依存としてWebの成果物へ含める。ブラウザの通常のHTTPキャッシュを利用し、カーネルごとにコンパイラーを取得し直さない。

配布時にはwccの[MITライセンス](https://github.com/tyfkda/xcc/blob/659a1773f20499864a8c3fdc7cd25cb506092bc1/LICENSE)、BinaryenのApache-2.0ライセンスと同梱される告知、実際に含めた依存のライセンスを保持する。バージョン更新は固定情報・ロックファイル・コンパイラー資産を一緒に更新する作業とする。

## 5 選定完了と後続作業の境界

この段階の成果は、採用ツール、ソースの固定点、パッケージの固定版、依存、接続API、最適化設定、配布方法を決めたことである。Web側へのパッケージ追加、コンパイラー資産のビルド・配置、実行用アダプターの実装、Cカーネルの実行確認は後続の導入作業で行う。

導入では、仕様で使用する数値演算・ループ・配列・export・メモリー割当てが一連の経路で成立することを確認する。性能順位の比較やコンパイラーの追加は行わない。VMとC数値計算の速度計測は、カーネル機能を実装した後の別段階で行う。
