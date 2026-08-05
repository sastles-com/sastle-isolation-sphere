> [English](README.md) · **日本語**

# Isolation Sphere ドキュメント

最終更新: 2025-12-02

このディレクトリには、Isolation Sphereプロジェクトの技術ドキュメントが含まれています。

## ドキュメント一覧

### システム設計

| ドキュメント | 説明 |
|-------------|------|
| [system_architecture.md](./system_architecture.md) | システム全体のアーキテクチャ、データフロー、技術スタック |
| [class_diagram.md](./class_diagram.md) | クラス図、コンポーネント構成、Mermaid図 |
| [implementation_spec.md](./implementation_spec.md) | 実装仕様書、コード例、トラブルシューティング |

### プロジェクト固有

- **ESP32仕様**: `../core/spec.md` - ファームウェア仕様
- **サーバー設計**: `../server/docs/` - 個別サーバードキュメント

## クイックリンク

### 新規開発者向け

1. [システム全体像を理解する](./system_architecture.md#概要)
2. [データフローを確認する](./class_diagram.md#データフロー)
3. [開発環境をセットアップする](./implementation_spec.md#6-ビルド・デプロイ)

### 実装者向け

- [MQTTデータフォーマット](./implementation_spec.md#51-mqtt-payload)
- [WebSocketメッセージ仕様](./implementation_spec.md#52-websocket-messages)
- [API Endpoints](./system_architecture.md#rest-api-endpoints)

### トラブルシューティング

- [よくある問題と解決策](./implementation_spec.md#7-トラブルシューティング)

## システム概要図

```
ESP32 (IMU) --[MQTT]--> Python Server --[WebSocket]--> Web Browser
                            ↓
                     State Manager
```

### 主要コンポーネント

1. **ESP32 Device**
   - IMU姿勢検知（BNO055）
   - MQTTでデータ送信
   - LED制御（800個）

2. **Python Server**
   - MQTT受信
   - WebSocket配信
   - REST API提供

3. **Web Frontend**
   - Three.js 3D可視化
   - リアルタイム姿勢同期
   - React UI

## 実装状況

### 完了機能 ✅

- ESP32 IMUデータ取得と送信
- MQTT通信
- WebSocketリアルタイム配信
- 3D球体姿勢可視化
- ダッシュボードUI
- config.json設定管理

### 未実装 ⏳

- LED映像ストリーミング（UDP）
- プレイリスト再生
- 設定画面からのMQTT設定変更

## 技術スタック

- **ESP32**: Arduino, PubSubClient, BNO055, FastLED
- **Server**: Python, FastAPI, paho-mqtt, uvicorn
- **Frontend**: React, Three.js, Material-UI, Vite

## 開発ガイドライン

### ブランチ戦略
- `main`: 安定版
- feature branches: 機能開発

### コミットメッセージ
```
feat: 新機能追加
fix: バグ修正
docs: ドキュメント更新
refactor: リファクタリング
```

### テスト
- ESP32: PlatformIO Native環境でユニットテスト
- Server: pytestでAPIテスト
- Frontend: Vitestでコンポーネントテスト

## 連絡先・リソース

- **GitHub**: https://github.com/sastles-com/sastle-isolation-sphere
- **プロジェクトルート**: `<repo-root>/sastle-isolation-sphere`

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2025-12-02 | ドキュメント初版作成、IMU統合完了 |
| 2025-12-01 | プロジェクト構造確立 |
