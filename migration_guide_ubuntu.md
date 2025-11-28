# Isolation Sphere サーバー Ubuntu 移行ガイド

このガイドでは、Isolation Sphere サーバーを macOS 開発環境から Ubuntu 本番/ステージング環境（**Ubuntu 22.04 LTS** および **ROS2 Humble** を推奨）へ移行する手順を説明します。

## 1. システム要件と準備

### OS
*   **Ubuntu 22.04 LTS (Jammy Jellyfish)**: ROS2 Humble の推奨環境です。

### ROS2 Humble のインストール
まだインストールされていない場合は、公式ガイドに従ってください：
```bash
sudo apt update && sudo apt install locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

sudo apt install software-properties-common
sudo add-apt-repository universe

sudo apt update && sudo apt install curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

sudo apt update
sudo apt install ros-humble-ros-base python3-colcon-common-extensions
sudo apt install ros-dev-tools
```

### システム依存関係
Python ツールと入力デバイスサポートをインストールします：
```bash
sudo apt install python3-pip python3-venv libgl1-mesa-dev
sudo apt install input-utils evtest  # ジョイスティックのデバッグ用
```

### Node.js (フロントエンド用)
```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
```

### uv (Python パッケージマネージャー)
開発環境と同様に `uv` の使用を推奨しますが、`pip` でも動作します。
```bash
curl -lsSf https://astral.sh/uv/install.sh | sh
```

---

## 2. コードのセットアップ

1.  **リポジトリのクローン**: Ubuntu マシンにコードをダウンロードします。
    ```bash
    git clone <your-repo-url> isolation-sphere
    cd isolation-sphere/isolation-server/server
    ```

2.  **Python 依存関係のインストール**
    コードは `evdev` と `rclpy` を自動的に検出します。Ubuntu では `rclpy` は ROS2 に含まれていますが、その他のライブラリが必要です。
    
    `uv` を使用する場合:
    ```bash
    uv sync
    ```
    
    *注意: `rclpy` は ROS2 のシステムパッケージです。仮想環境を使用する場合、システムサイトパッケージへのアクセスを許可するか、ROS2 環境変数をロードしてから Python を実行するのが一般的です。*

3.  **フロントエンド依存関係のインストール**
    ```bash
    cd frontend
    npm install
    ```

---

## 3. 設定の確認

コードは実行環境を自動検出するように設計されています：
*   **ジョイスティック**: `/dev/input/event*` が存在し、`evdev` がインストールされていれば、実際のハードウェアを使用します。
*   **ROS2**: `source /opt/ros/humble/setup.bash` が実行され、`import rclpy` が成功すれば、モックではなく実際の ROS2 ノードを使用します。

**重要**: `sudo` なしでジョイスティックイベントを読み取るために、ユーザーを `input` グループに追加してください：
```bash
sudo usermod -aG input $USER
# 設定を反映させるために、一度ログアウトして再ログインしてください
```

---

## 4. サービスの実行

3つのターミナル（または `tmux` / `systemd`）が必要です。

### ターミナル 1: バックエンド (Backend)
```bash
source /opt/ros/humble/setup.bash
cd isolation-sphere/isolation-server/server

# uv を使用する場合
uv run uvicorn app.main:app --host 0.0.0.0 --port 8000
```

### ターミナル 2: ジョイスティックデーモン (Joystick Daemon)
```bash
source /opt/ros/humble/setup.bash
cd isolation-sphere/isolation-server/server

# uv を使用する場合
uv run joystick/daemon.py
```

### ターミナル 3: フロントエンド (Frontend)
本番環境では静的ファイルをビルドして配信（Nginx や FastAPI 経由）しますが、テスト用には以下を実行します：
```bash
cd isolation-sphere/isolation-server/server/frontend
npm run dev -- --host
```

---

## 5. 動作確認

1.  **ジョイスティック確認**: `evtest` を実行し、コントローラーが Ubuntu に認識されているか確認します。
2.  **ROS2 確認**: `ros2 topic list` を実行し、`/isolation_sphere/ui/control` が表示されるか確認します。
3.  **UI 確認**: ブラウザで `http://<ubuntu-ip>:5173` を開きます。

## 6. 本番デプロイ (オプション)

恒久的なセットアップには、Systemd サービスを作成することをお勧めします。

**例: `isolation-backend.service`**
```ini
[Unit]
Description=Isolation Sphere Backend
After=network.target

[Service]
User=ubuntu
WorkingDirectory=/home/ubuntu/isolation-sphere/isolation-server/server
Environment="PATH=/home/ubuntu/.local/bin:/usr/bin:/bin"
ExecStart=/bin/bash -c 'source /opt/ros/humble/setup.bash && uv run uvicorn app.main:app --host 0.0.0.0 --port 8000'
Restart=always

[Install]
WantedBy=multi-user.target
```
(ジョイスティックデーモンやフロントエンド配信についても同様に作成します)。
