# Doxygen コメントテンプレート

このプロジェクトでは、すべてのクラス・関数にDoxygenコメントを付けることを標準とします。

## ヘッダーファイルテンプレート

```cpp
/**
 * @file ClassName.h
 * @brief クラスの簡潔な説明（1行）
 * @author sastle-com
 * @date YYYY-MM-DD
 */

#pragma once

#include <Arduino.h>

namespace sastle {

/**
 * @class ClassName
 * @brief クラスの詳細な説明
 * 
 * このクラスの目的、使用方法、主な機能について
 * 複数行で詳しく説明します。
 */
class ClassName {
public:
    /**
     * @brief コンストラクタの説明
     */
    ClassName();
    
    /**
     * @brief デストラクタの説明
     */
    ~ClassName();
    
    /**
     * @brief メソッドの簡潔な説明
     * @param param1 パラメータ1の説明
     * @param param2 パラメータ2の説明
     * @return 戻り値の説明
     * @note 特記事項があればここに記載
     */
    bool someMethod(int param1, const char* param2);
    
    /**
     * @brief getter メソッドの説明
     * @return 取得する値の説明
     */
    int getValue() const { return _value; }
    
private:
    int _value;          ///< メンバ変数の説明（インラインコメント）
    bool _initialized;   ///< 初期化状態フラグ
    
    /**
     * @brief プライベートヘルパー関数の説明
     * @param data 処理するデータ
     * @return 処理結果
     */
    bool _helperMethod(const uint8_t* data);
};

} // namespace sastle
```

## 構造体・列挙型テンプレート

```cpp
/**
 * @struct StructName
 * @brief 構造体の説明
 */
struct StructName {
    int field1;      ///< フィールド1の説明
    float field2;    ///< フィールド2の説明
    String field3;   ///< フィールド3の説明
};

/**
 * @enum EnumName
 * @brief 列挙型の説明
 */
enum class EnumName {
    VALUE1,  ///< 値1の説明
    VALUE2,  ///< 値2の説明
    VALUE3   ///< 値3の説明
};
```

## マクロ定義テンプレート

```cpp
/// マクロの簡潔な説明
#define MACRO_NAME 100

/**
 * @def COMPLEX_MACRO
 * @brief 複雑なマクロの詳細説明
 * @param x パラメータxの説明
 * @param y パラメータyの説明
 */
#define COMPLEX_MACRO(x, y) ((x) * (y))
```

## 実装ファイル (.cpp) のコメント

実装ファイルでは、ヘッダーで定義したDoxygenコメントを繰り返す必要はありません。
複雑なロジックや注意点がある場合のみ、追加の説明コメントを記載します。

```cpp
/**
 * @file ClassName.cpp
 * @brief クラス実装
 */

#include "ClassName.h"

namespace sastle {

ClassName::ClassName() : _value(0), _initialized(false) {
    // 初期化処理
}

bool ClassName::someMethod(int param1, const char* param2) {
    // 複雑な処理の場合、ここに実装の詳細コメントを追加
    // 例: ステートマシンの遷移、アルゴリズムの説明など
    
    if (!_initialized) {
        return false;
    }
    
    // 処理本体
    return true;
}

} // namespace sastle
```

## Doxygenタグ一覧

| タグ | 用途 |
|------|------|
| `@file` | ファイルの説明 |
| `@brief` | 簡潔な説明（1行） |
| `@class` | クラスの説明 |
| `@struct` | 構造体の説明 |
| `@enum` | 列挙型の説明 |
| `@param` | パラメータの説明 |
| `@return` | 戻り値の説明 |
| `@note` | 注意事項・補足説明 |
| `@warning` | 警告事項 |
| `@see` | 関連する項目への参照 |
| `@author` | 作成者 |
| `@date` | 作成日 |
| `@version` | バージョン情報 |
| `@todo` | 未実装・今後の課題 |
| `@deprecated` | 非推奨機能の警告 |

## インラインコメント

メンバ変数や簡単な定数には `///` を使用したインラインコメントを推奨します。

```cpp
private:
    int _counter;           ///< カウンタ変数
    bool _enabled;          ///< 有効化フラグ
    const int MAX_SIZE;     ///< 最大サイズ定数
```

## 例: SoundManager のテンプレート

以下は、次に実装する SoundManager のDoxygenコメント例です。

```cpp
/**
 * @file SoundManager.h
 * @brief サウンド出力管理クラス
 * @author sastle-com
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include "ConfigManager.h"

namespace sastle {

/**
 * @class SoundManager
 * @brief PWMまたはI2Sを使用したサウンド出力管理クラス
 * 
 * ジェスチャーフィードバックやシステムイベントの音声出力を提供します。
 * PWM圧電ブザーとI2S DACの両方に対応しています。
 */
class SoundManager {
public:
    SoundManager();
    ~SoundManager();
    
    /**
     * @brief サウンドシステムを初期化
     * @param config 設定マネージャー参照
     * @return true 初期化成功, false 初期化失敗
     */
    bool begin(ConfigManager& config);
    
    /**
     * @brief トーンを再生
     * @param frequency 周波数 (Hz)
     * @param duration 再生時間 (ms)
     */
    void playTone(uint16_t frequency, uint16_t duration);
    
    /**
     * @brief ビープ音を再生
     * @note UI入力フィードバック用の短い音
     */
    void playBeep();
    
private:
    bool _initialized;    ///< 初期化状態
    uint8_t _gpio;        ///< 出力GPIOピン番号
    uint8_t _channel;     ///< LEDC PWMチャンネル
};

} // namespace sastle
```

## コーディング規約

1. すべての public メソッドに `@brief` と必要に応じて `@param`, `@return` を記載
2. private メソッドは簡潔な説明のみでOK
3. メンバ変数にはインラインコメント `///` を使用
4. 複雑なアルゴリズムは実装ファイル内にも説明コメントを追加
5. ファイルヘッダーには必ず `@file`, `@brief`, `@author`, `@date` を記載
