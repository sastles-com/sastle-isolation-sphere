> [English](state_manager_design.md) · **日本語**

# StateManager アーキテクチャ設計書

最終更新: 2025-12-02

## 概要

StateManagerは、Isolation Sphereシステムにおける**唯一の状態管理者**として機能します。

**重要**: 新しいデーモンではなく、**既存のFastAPIサーバー内のシングルトンサービス**として実装されます。

---

## システム構成

### デーモン構成（変更なし）

```
システム全体:
├── FastAPI Server (既存) :9000
│   ├── HTTP API
│   ├── WebSocket (/ws)
│   ├── StateManager ← ここに統合（新規デーモンなし）
│   └── MQTTService
├── MQTT Broker (mosquitto) :1883
└── Frontend (静的ファイル配信)
```

### プロセス構成

```
┌─────────────────────────────────────────────────────────┐
│ FastAPI Server (uvicorn) - 単一プロセス                  │
│                                                         │
│  main.py                                                │
│    ├── StateManager (Singleton)                         │
│    ├── MQTTService (Singleton)                          │
│    ├── WebSocket endpoints                              │
│    └── HTTP API endpoints                               │
└─────────────────────────────────────────────────────────┘
```

---

## 責務

| コンポーネント | 責務 | プロセス |
|---------------|------|---------|
| **FastAPI Server** | HTTP API, WebSocket, 起動管理 | uvicorn |
| **StateManager** | 状態管理, コマンド処理, ステート配信 | FastAPI内 |
| **MQTTService** | MQTT通信, コマンド購読 | FastAPI内 |
| **MQTT Broker** | メッセージング | mosquitto |

---

## シーケンス図

詳細なシーケンス図は `docs/mqtt_ui_control.md` を参照。

### 要約: データフロー

```
UI → Command → MQTT → StateManager → State → MQTT/WebSocket → 全員同期
```

---

## 実装チェックリスト

### Phase 1: StateManager統合

- [ ] StateManager拡張 (`app/services/state_manager.py`)
- [ ] MQTTService拡張 (`app/services/mqtt_service.py`)
- [ ] main.py修正（起動時の連携）
- [ ] WebSocketエンドポイント修正

### Phase 2: ESP32実装

- [ ] sphere/all/state 購読
- [ ] LED状態反映

### Phase 3: Web UI実装

- [ ] State購読・UI更新
- [ ] コマンド送信

---

## まとめ

- **新しいデーモンは不要** ✅
- **既存のFastAPIサーバー内で完結** ✅
- **インフラ変更なし** ✅
