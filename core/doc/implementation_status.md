# 実装ステータス

## 2025-12-01 デュアルコア LED レンダリング実装

### 完了項目

#### 1. LEDManager クラス実装
- ✅ **ファイル**: `src/LEDManager.h`, `src/LEDManager.cpp`
- ✅ **機能**:
  - FastLED ライブラリ統合 (WS2812B × 4ストリップ)
  - LEDレイアウトCSV読み込み (800 LEDs)
  - 3D球面座標 → UV座標変換 (`sphereToUV()`)
  - Core 1 でのレンダリングタスク (FreeRTOS)
  - ImageManager からのピクセル取得
  - 統計情報 (FPS, レンダリング時間, マッピング時間)

#### 2. ImageManager 連携
- ✅ **フレーム準備完了コールバック**: `FrameReadyCallback` 型定義
- ✅ **セマフォ通知**: `LEDManager::onFrameReady()` でセマフォ送信
- ✅ **ダブルバッファ**: `_drawBuffer` / `_decodeBuffer` のポインタスワップ

#### 3. デュアルコア タスク分散
- ✅ **Core 0 (PRO_CPU)**: 
  - Arduino `setup()` / `loop()`
  - WiFi/MQTT 通信
  - UDP 画像受信 (`ImageManager::update()`)
  - JPEG デコード (`TJpg_Decoder`)
  - IMU 更新 (`IMUManager`)
  - ジェスチャー検出 (`GestureManager`)
  - サウンド出力 (`SoundManager`)

- ✅ **Core 1 (APP_CPU)**:
  - LED レンダリングタスク (`ledRenderTask`)
  - 座標マッピング (`updateLEDBuffer()`)
  - FastLED 出力 (`FastLED.show()`)

#### 4. 統合・ビルド
- ✅ **platformio.ini**: FastLED@^3.7.8 追加
- ✅ **main.cpp**: ImageManager, LEDManager 初期化
- ✅ **起動時LEDテスト**: 赤→緑→青 点灯確認
- ✅ **コンパイル成功**: RAM 15.5% (50,756 bytes), Flash 58.9% (926,397 bytes)

### メモリ使用量

| リソース | 使用量 | 割合 | 備考 |
|---------|--------|------|------|
| SRAM | 50,756 B | 15.5% | LEDバッファ 2.4KB + レイアウト 9.6KB 含む |
| Flash | 926,397 B | 58.9% | FastLED, TJpg_Decoder 含む |
| PSRAM | ~265 KB | 3.3% | 画像バッファ 200KB + UDP 64KB |

### 主要データフロー

```
┌─────────────────────────────────────────┐
│ PC (udp_image_sender.py)                │
└──────────────┬──────────────────────────┘
               │ UDP: 8889
               │ JPEG (10-30KB)
               ▼
┌─────────────────────────────────────────┐
│ Core 0: ImageManager                    │
│ 1. receivePacket()                      │
│ 2. decodeJPEG()        [TJpg_Decoder]   │
│ 3. tjpgOutput() → _decodeBuffer         │
│ 4. swapBuffers()                        │
│ 5. _frameReadyCallback()                │
└──────────────┬──────────────────────────┘
               │ Semaphore Give
               ▼
┌─────────────────────────────────────────┐
│ Core 1: LEDManager                      │
│ 1. xSemaphoreTake(frameReadySemaphore)  │
│ 2. updateLEDBuffer()                    │
│    - sphereToUV(x,y,z) × 800            │
│    - getPixel(u,v) × 800                │
│ 3. FastLED.show()      [DMA]            │
└─────────────────────────────────────────┘
               │ GPIO 5,6,7,8
               ▼
         [WS2812B × 800]
```

### パフォーマンス予測

| 処理 | 時間 (予測) | 備考 |
|------|-------------|------|
| UDP受信 | ~1-2 ms | LwIP 内部DMA |
| JPEG デコード | 15-25 ms | 320x160, quality 70-90 |
| バッファスワップ | < 0.01 ms | ポインタ交換のみ |
| 座標マッピング | 10-15 ms | 800 LEDs × 三角関数 |
| FastLED出力 | 5-10 ms | WS2812B タイミング |
| **合計フレーム時間** | **30-50 ms** | **20-30 fps** |

### 未実装・今後の課題

#### 1. 実機テスト
- [ ] ESP32-S3 への書き込み
- [ ] PC → UDP → LED パイプライン動作確認
- [ ] FPS 実測値取得
- [ ] LED 配色確認（UV マッピング正確性）

#### 2. 最適化
- [ ] **I2S DMA 方式への移行**: FastLED は RMT 使用、I2S なら CPU 負荷削減
- [ ] **座標マッピング ルックアップテーブル**: 三角関数計算の事前計算
- [ ] **JPEG デコード並列化**: マルチコア活用（TJpg_Decoder 制約確認）
- [ ] **セマフォ → メッセージキュー**: フレームドロップ検出改善

#### 3. 機能追加
- [ ] **輝度自動調整**: IMU による姿勢検出 → LED 明るさ制御
- [ ] **エフェクトモード**: 画像なし時のデフォルトアニメーション
- [ ] **FPS 統計 MQTT 送信**: リアルタイムモニタリング
- [ ] **UDP パケットロス検出**: フレームID 連続性チェック

#### 4. デバッグ機能
- [ ] **シリアル統計出力**: 10秒毎に FPS, ドロップ数, レンダリング時間
- [ ] **LED インデックス表示**: 特定 LED の点灯テスト
- [ ] **座標可視化**: UV マッピング結果の確認方法

### 技術的決定事項

| 項目 | 決定内容 | 理由 |
|------|---------|------|
| LED ライブラリ | FastLED 3.7.8 | 安定性、WS2812B 対応、豊富な実績 |
| コア分散 | Core 0: 入力処理, Core 1: LED 描画 | LED 出力のリアルタイム性確保 |
| 同期方式 | バイナリセマフォ | シンプル、オーバーヘッド最小 |
| バッファ配置 | PSRAM: 画像, SRAM: LED | アクセス速度と容量のバランス |
| UV マッピング | 球面座標変換 (atan2, asin) | 汎用性、数学的正確性 |
| フレームレート目標 | 30 fps | 人間の視覚に十分、LED 更新周期 33ms |

### 参考ドキュメント

- [dual_core_design.md](dual_core_design.md) - デュアルコア設計詳細
- [image_manager_design.md](image_manager_design.md) - ImageManager アーキテクチャ
- [udp_image_protocol.md](udp_image_protocol.md) - UDP 画像プロトコル仕様
- [spec.md](../spec.md) - プロジェクト全体仕様

### 次のステップ

1. **実機書き込み**: `pio run -t upload`
2. **PC 側送信**: `python tools/udp_image_sender.py --camera --fps 30`
3. **動作確認**: LED 球体での画像表示テスト
4. **パフォーマンス計測**: シリアル出力で FPS 確認
5. **調整**: 輝度、色補正、マッピング微調整
