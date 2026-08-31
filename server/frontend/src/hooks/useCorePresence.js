import { useState } from 'react';
import { useStateUpdate } from './useSphereState';

// 内容が同じなら前回の配列をそのまま返す。STATE_UPDATE は IMU 更新で
// 約17Hz 流れてくるため、毎回新しい配列を state に入れると購読側が
// 常時再レンダーされる (ドットは秒単位でしか変わらない)。
const sameIds = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const keepIfSame = (setter) => (next) =>
    setter((prev) => (sameIds(prev, next) ? prev : next));

/**
 * useCorePresence — 接続中の core を STATE_UPDATE から取り出す共通フック。
 *
 * spheres: config.json の登録一覧 [{id, static_ip}] (オフラインの core も含む)
 * online:  サーバーの死活判定を通った id 一覧 (StateManager.online_ids)。
 *          IMU/status/log の last_seen が DEVICE_OFFLINE_TIMEOUT_SEC 以内のもの。
 * target:  現在の操作対象 core の id ('all' = 全 core)
 *
 * online は「今生きている」、devices は「一度でも見えた」で意味が違う。
 * インジケータや死活表示は online を、最後の姿勢など値の参照は devices を使う。
 */
export const useCorePresence = () => {
    const [spheres, setSpheres] = useState([]);
    const [online, setOnline] = useState([]);
    const [target, setTarget] = useState(null);
    // サーバーが online を配信しているか。死活判定を持たない旧サーバーだと
    // online が来ないので、これが false のときは「全滅」ではなく「不明」。
    // 区別しないと、サーバー再起動前の画面が「全 core オフライン」に見えてしまう。
    const [presenceKnown, setPresenceKnown] = useState(false);

    useStateUpdate((payload) => {
        if (payload.spheres) {
            // id だけ比較すれば足りる (static_ip は起動後に変わらない)
            const ids = payload.spheres.map((s) => s.id).filter(Boolean);
            setSpheres((prev) => (sameIds(prev.map((s) => s.id), ids) ? prev : payload.spheres));
        }
        // 全 core がオフラインなら online は [] で届く。キーの有無で判定すること
        // (truthy 判定にすると [] を「未配信」と誤認する)
        if (Array.isArray(payload.online)) {
            keepIfSame(setOnline)(payload.online);
            setPresenceKnown(true);
        }
        if (payload.target) setTarget(payload.target);
    });

    return { spheres, online, target, presenceKnown };
};
