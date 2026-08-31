import React from 'react';

/**
 * CoreIndicator — 接続中の core をドットで表示する。
 *
 * spheres = config.json の登録一覧 (オフラインも含む全 core)、
 * online  = サーバーの死活判定を通った id 一覧 (StateManager.online_ids)。
 * 「登録されているが黙っている core」を欠落ではなく消灯ドットとして見せたいので、
 * 表示する数は spheres 側で決め、色を online で決める。
 *
 * ドットの状態:
 *   ● 緑ベタ + ハロー = オンライン (publish が DEVICE_OFFLINE_TIMEOUT_SEC 以内)
 *   ○ 枠だけ          = 登録済みだが応答なし
 *   ○ 枠 (黄)         = サーバーが死活を報告していない = 不明 (presenceKnown=false)
 * リングは操作対象が特定の core のときだけ付く。target='all' は全 core が対象で、
 * 全ドットに付けても情報量がゼロなので付けない (数字側で ALL と分かる)。
 */
export const CoreIndicator = ({ spheres = [], online = [], target, presenceKnown = true, compact = false }) => {
    // 登録が空でもオンラインの core は出す (config 未登録の実機を見落とさない)
    const ids = spheres.length > 0 ? spheres.map((s) => s.id).filter(Boolean) : [...online];
    if (ids.length === 0) return null;

    const size = compact ? 9 : 10;
    const ringed = target && target !== 'all';   // 特定 core を選んでいるときだけリング

    const label = !presenceKnown
        ? `?/${ids.length}`
        : `${online.length}/${ids.length}`;

    const tip = presenceKnown
        ? ids.map((id) => `${id}: ${online.includes(id) ? 'online' : 'offline'}`).join('\n')
        : 'サーバーが死活情報を配信していません (server_restart.sh で再起動が必要)';

    return (
        <span
            role="status"
            aria-label={presenceKnown
                ? `cores online: ${online.length} of ${ids.length}`
                : 'core presence unknown'}
            title={tip}
            style={{ display: 'inline-flex', alignItems: 'center', gap: compact ? 4 : 5, flexShrink: 0 }}
        >
            {ids.map((id) => {
                const isOnline = presenceKnown && online.includes(id);
                const isTarget = ringed && target === id;
                return (
                    <span
                        key={id}
                        style={{
                            width: size, height: size, borderRadius: '50%',
                            background: isOnline ? 'var(--ok)' : 'transparent',
                            border: isOnline
                                ? 'none'
                                : `1px solid ${presenceKnown ? 'var(--tx-3)' : 'var(--warn)'}`,
                            boxSizing: 'border-box',
                            // オンラインはハローで「点灯している」ことを明示する。
                            // 操作対象のリングは ground を挟んだ二重リングで区別。
                            boxShadow: [
                                isOnline ? '0 0 6px 1px var(--ok-glow)' : null,
                                isTarget ? `0 0 0 2px var(--ground), 0 0 0 3px ${isOnline ? 'var(--ok)' : 'var(--tx-3)'}` : null,
                            ].filter(Boolean).join(', ') || 'none',
                            opacity: isOnline ? 1 : 0.6,
                            transition: 'background 200ms, box-shadow 200ms, opacity 200ms',
                        }}
                    />
                );
            })}
            <span className="mono" style={{
                fontSize: compact ? 10 : 11,
                color: presenceKnown ? 'var(--tx-3)' : 'var(--warn)',
                marginLeft: 2, letterSpacing: '.02em',
            }}>
                {label}
            </span>
        </span>
    );
};
