> [English](imu_compensation.md) · **日本語**

# IMU姿勢補正機能

## 概要

LED球体が物理的に回転しても、常に正しい向きで画像が表示されるように、IMUのquaternionを使って各LEDの座標を逆回転補正する機能です。

## 動作原理

### 問題

球体が回転すると、固定されたLED座標も一緒に回転してしまい、画像が球体の回転に追従してしまいます。

```
例：球体を90度傾けた場合
- LED #0 の物理位置: (0.261, 0.804, -0.533) → 傾斜後
- でも、画像は傾いていない空間座標で送られてくる
→ 結果：画像が傾いて見える
```

### 解決策：Quaternion逆回転

1. **IMUからquaternion取得**: 球体の現在姿勢 `q = (w, x, y, z)`
2. **共役quaternion計算**: 逆回転 `q^-1 = (w, -x, -y, -z)`
3. **LED座標を逆回転**: `LED座標' = q^-1 * LED座標 * q`
4. **逆回転後の座標でUV変換**: 画像のどの部分を表示するか決定

これにより、球体がどう傾いても、LED座標を「元の姿勢に戻す」ことで、正しい画像が表示されます。

## 実装詳細

### LEDManager::rotateByQuaternion()

```cpp
void LEDManager::rotateByQuaternion(float& x, float& y, float& z, 
                                     float qw, float qx, float qy, float qz) {
    // 最適化された quaternion ベクトル回転
    // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    
    float cross1_x = qy * z - qz * y;
    float cross1_y = qz * x - qx * z;
    float cross1_z = qx * y - qy * x;
    
    cross1_x += qw * x;
    cross1_y += qw * y;
    cross1_z += qw * z;
    
    float cross2_x = qy * cross1_z - qz * cross1_y;
    float cross2_y = qz * cross1_x - qx * cross1_z;
    float cross2_z = qx * cross1_y - qy * cross1_x;
    
    x += 2.0f * cross2_x;
    y += 2.0f * cross2_y;
    z += 2.0f * cross2_z;
}
```

### updateLEDBuffer() フロー

```cpp
void LEDManager::updateLEDBuffer() {
    // 1. IMUからquaternion取得
    float qw, qx, qy, qz;
    if (_imuManager->getQuaternion(qw, qx, qy, qz)) {
        // 共役計算（逆回転）
        qx = -qx;
        qy = -qy;
        qz = -qz;
    }
    
    for (uint16_t i = 0; i < _numLEDs; i++) {
        // 2. LED座標取得
        float x = ledLayout[i].x;
        float y = ledLayout[i].y;
        float z = ledLayout[i].z;
        
        // 3. 逆回転補正
        rotateByQuaternion(x, y, z, qw, qx, qy, qz);
        
        // 4. UV変換
        float u, v;
        sphereToUV(x, y, z, u, v);
        
        // 5. 画像ピクセル取得
        imageManager->getPixel(u * width, v * height, r, g, b);
        
        // 6. LEDに設定
        ledBuffer[i] = CRGB(r, g, b);
    }
}
```

## 有効化/無効化

### 自動有効化

IMUManagerが初期化されていれば、自動的に姿勢補正が有効になります。

```cpp
// main.cpp
IMUManager* imuPtr = imuSensor.isInitialized() ? &imuSensor : nullptr;
ledManager.begin(config, imageManager, imuPtr);
```

### 手動制御

```cpp
// 姿勢補正を無効化（デバッグ用）
ledManager.setIMUCompensation(false);

// 再度有効化
ledManager.setIMUCompensation(true);
```

## パフォーマンス

### 処理時間

| 処理 | 時間 (予測) | 最適化前 | 最適化後 |
|------|-------------|----------|----------|
| quaternion取得 | ~0.1 ms | 0.1 ms | 0.1 ms |
| 共役計算 | < 0.01 ms | < 0.01 ms | < 0.01 ms |
| 逆回転 × 800 | ~8-12 ms | 10-15 ms | 8-12 ms |
| UV変換 × 800 | ~10-15 ms | 15-20 ms | 10-12 ms |
| **合計** | **~18-27 ms** | **25-35 ms** | **18-25 ms** |

### 最適化内容

#### 1. common.h 高速近似関数の使用

**_sqrt() - 高速平方根**
- 従来: `sqrt()` 標準ライブラリ
- 最適化: `_sqrt()` 4次収束法による近似
- **効果**: 約2-3倍高速化

**_atan2() - 高速逆正接**
- 従来: `atan2f()` 標準ライブラリ  
- 最適化: `_atan2()` 4次多項式近似
- **効果**: 約3-5倍高速化

```cpp
// 最適化前 (標準ライブラリ)
float len = sqrt(x*x + y*y + z*z);
u = 0.5f + atan2f(nx, ny) / (2.0f * PI);              // 経度 → 幅
v = atan2f(sqrt(nx*nx + ny*ny), nz) / PI;             // 極角 (+Zから) → 高さ

// 最適化後 (FastMath.h。_atan2 は「度/180」を返すので /PI 相当の除算は不要)
float len = _sqrt(x*x + y*y + z*z);
u = (_atan2(nx, ny) + 1.0f) / 2.0f;                   // 経度 -180..180 → [0,1]
v = _atan2(_sqrt(nx*nx + ny*ny), nz);                 // 極角 0..180 → [0,1]
```

> 極軸=Z の標準正距円筒。`v` は第1引数が ≥0 なので `_atan2 ∈ [0,1]` に収まり、`(+1)/2` は付けない。
> 実装は [`LEDManager::sphereToUV()`](../src/LEDManager.cpp) が正典で、WebUIツイン
> `server/frontend/src/components/sphere/HoloSphere.jsx` と同一式・同一量子化。

#### 2. Quaternion回転の最適化

従来の実装でも既に最適化済み:
- 外積ベース計算: 12回の積和演算
- 行列計算を回避: メモリアクセス削減

800個のLEDに対して、フレームあたり約18-25msの処理時間です。
目標30fps (33ms/frame) に対して**十分な余裕**があります。

## 座標系

### BNO055 IMUの座標系

```
      +Z (天頂)
       |
       |
       +------ +X (前方)
      /
     /
   +Y (右)
```

### LED球体の座標系

`led_layouts-5strip.csv` で定義される座標系がIMU座標系と一致していることを前提とします。

もし座標系が異なる場合、`rotateByQuaternion()` の前に座標変換が必要です。

## デバッグ

### シリアル出力例

```
[LEDManager] Initializing...
[LEDManager] IMU compensation enabled
[LEDManager] Loaded 800 LED coordinates
[LEDManager] LEDs per strip: [200, 200, 200, 200]
[LEDManager] Initialization complete

[LED_Render] Task started on core 1
```

### 姿勢補正の確認方法

1. **静止状態**: 球体を動かさない → 画像が正常に表示
2. **回転状態**: 球体を傾ける → 画像は常に正立を維持
3. **補正無効化**: `setIMUCompensation(false)` → 画像が球体と一緒に回転

## 制限事項

1. **IMU更新レート**: 100Hz (10ms間隔)
   - LED更新30fpsより速いため、十分な精度

2. **Quaternion正規化**:
   - BNO055は正規化済みquaternionを出力
   - 他のIMUを使う場合は正規化が必要

3. **座標系の一致**:
   - LED座標系とIMU座標系が一致していることが前提
   - キャリブレーション必要な場合あり

## 今後の改善

- [ ] **キャリブレーション機能**: 座標系オフセット補正
- [ ] **スムージング**: Quaternionの補間で滑らかな動き
- [x] **高速近似関数**: common.h の `_sqrt()`, `_atan2()` 使用で3-5倍高速化
- [ ] **ルックアップテーブル**: さらなる高速化（精度とのトレードオフ）
- [ ] **座標系変換UI**: 異なる座標系への対応

## 参考資料

- [Isolation sphere の制作 - elchika](https://elchika.com/article/9b97a7b7-561f-4795-9e16-3e011c045913/)
  - IMUによるLEDの仮想回転実装
  - Quaternionを使った姿勢補正の原理
- [common.h 高速近似関数](../src/common.h)
  - `_sqrt()`: 4次収束法による平方根近似
  - `_atan2()`: 4次多項式による逆正接近似
