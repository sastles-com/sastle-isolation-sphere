// Studio は /studio で同一オリジン配信されるため、操作アプリと同じ API をそのまま叩ける (CORS フリー)。

/** 表示フォーマット(width/height/fps 上限) を取得。取れなければ null。 */
export async function fetchImageFormat() {
    try {
        const r = await fetch('/api/config/settings');
        if (r.ok) {
            const s = await r.json();
            return s.image || null;
        }
    } catch { /* offline */ }
    return null;
}

function sanitize(name) {
    return (name || '').trim().replace(/[^\w.\- ぁ-んァ-ヶ一-龠]/g, '_').slice(0, 60) || 'pattern';
}

/**
 * 書き出した webm を pattern としてアップロードする。
 * 既存の POST /api/playlist/videos に kind=pattern を渡す (spec §4)。
 */
export async function uploadPattern(blob, title) {
    const filename = `${sanitize(title)}.webm`;
    const file = new File([blob], filename, { type: 'video/webm' });
    const fd = new FormData();
    fd.append('file', file);
    fd.append('title', title || filename);
    fd.append('kind', 'pattern');
    const r = await fetch('/api/playlist/videos', { method: 'POST', body: fd });
    if (!r.ok) throw new Error(`upload failed: ${r.status} ${await r.text()}`);
    return r.json();
}
