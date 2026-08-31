// 全 core へのブロードキャストを表す操作対象。サーバー側 DEVICE_TARGET_ALL と一致
export const TARGET_ALL = 'all';

/**
 * STATE_UPDATE の payload から「表示に使う1台ぶんの IMU」を選ぶ。
 *
 * 3Dスフィアは1台ぶんの姿勢しか描けないため、操作対象 (コマンドの宛先) とは別に
 * 表示対象を1台に決める必要がある。特に operation target が 'all' のとき、
 * payload.devices に 'all' というキーは存在しない。
 *
 * ここを素朴に `devices[selected]?.imu || payload.imu` と書くと、'all' のときに
 * payload.imu (= StateManager が device を問わず最後の受信値で毎回上書きする
 * 後方互換フィールド) にフォールバックする。2台が各約8.4Hz で publish して
 * いるので、合計約17Hz で2台の異なる姿勢が交互に代入され、スフィアが
 * 2つの向きの間を高速で往復して振動する。
 *
 * 対策として、'all'/未選択のときは代表機を1台に固定する。優先順位:
 *   1. 選択中の core (実データがあるとき)
 *   2. オンラインの core のうち id 昇順で先頭 (死んだ core の凍結姿勢を避ける)
 *   3. devices に居る core のうち id 昇順で先頭
 * 「昇順で先頭」に固定するのが要点で、ここを受信順にすると振動が再発する。
 *
 * @returns {{imu: object|null, deviceId: string|null}} 選んだ IMU と、その出所
 */
export const selectDeviceImu = (payload, selectedDeviceId) => {
    const devices = payload?.devices || {};

    // 1. 特定の core を選んでいて、その実データがある
    if (selectedDeviceId && selectedDeviceId !== TARGET_ALL) {
        const imu = devices[selectedDeviceId]?.imu;
        if (imu) return { imu, deviceId: selectedDeviceId };
    }

    // 2. オンラインの core を優先 (online はサーバーの死活判定済み id 一覧)
    const online = Array.isArray(payload?.online) ? payload.online : [];
    const onlineWithImu = online.filter((id) => devices[id]?.imu).sort();
    if (onlineWithImu.length > 0) {
        return { imu: devices[onlineWithImu[0]].imu, deviceId: onlineWithImu[0] };
    }

    // 3. 死活情報が無い旧サーバー等では devices から選ぶ
    const anyWithImu = Object.keys(devices).filter((id) => devices[id]?.imu).sort();
    if (anyWithImu.length > 0) {
        return { imu: devices[anyWithImu[0]].imu, deviceId: anyWithImu[0] };
    }

    // 4. devices がまだ無い最初期のみ、単一値にフォールバック
    //    (1台運用では正しく、複数台では上の経路が必ず先に当たる)
    return payload?.imu ? { imu: payload.imu, deviceId: null } : { imu: null, deviceId: null };
};
