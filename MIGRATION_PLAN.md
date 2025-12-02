# Data Directory Migration Plan

## 目的
ESP32とServer間でconfig.jsonとデータファイルを共有する

## 移行手順

### 1. 共有dataディレクトリ作成
```bash
mkdir -p data
mv core/data/* data/
```

### 2. シンボリックリンク作成
```bash
rmdir core/data
ln -s ../data core/data
```

### 3. 影響確認
- PlatformIO: シンボリックリンク経由でdata/をアップロード
- ESP32コード: パス変更不要（LittleFS内の絶対パス）
- Server: data/config.jsonを読み込み可能に

## 不整合チェック結果

✅ PlatformIO: 変更不要（dataフォルダ自動検出）
✅ ESP32コード: 変更不要（LittleFS内絶対パス使用）
✅ config.json: 変更不要（内部パスは相対的）
✅ Git管理: .gitignoreにimages/を追加推奨（1.2MB）

## リスク
- 低: シンボリックリンクはPlatformIOとGitで正常動作
- imagesフォルダは大きいのでGit管理外推奨

