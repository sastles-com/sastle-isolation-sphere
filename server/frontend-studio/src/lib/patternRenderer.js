// Pattern の描画ロジック (equirectangular 2:1 マップ)。
// P3 のエフェクトは「球面スピン(経度スクロール)」のみ。P4 で面内回転/拡大パルス/フェードを追加予定。
//
// ループ不変条件 (spec §2):
//  - 経度ラップ: content を水平タイル(off-W, off, off+W)で合成 → シームまたぎで途切れない。
//  - 時間ループ: 総移動 = spinTurns(整数) × W。phase 0→1 で offset が W の整数倍 → t=0 と t=1 が一致。
//  - 末尾非重複: フレーム i の phase = i/N (i=0..N-1)。t=1 (=t=0) は出力しない。

export const CANVAS_W = 640;   // 作業解像度 (2:1)。サーバが 320x160 に resize する。
export const CANVAS_H = 320;

/**
 * strokes(手書き) と texts をまとめた「コンテンツ層」を透過キャンバスに描く。
 * スピンではこの層全体が水平スクロールする。
 */
export function buildContentCanvas(pattern, W = CANVAS_W, H = CANVAS_H) {
    const c = document.createElement('canvas');
    c.width = W;
    c.height = H;
    const ctx = c.getContext('2d');

    for (const s of pattern.strokes || []) {
        ctx.strokeStyle = s.color;
        ctx.fillStyle = s.color;
        ctx.lineWidth = s.width;
        ctx.lineJoin = 'round';
        ctx.lineCap = 'round';
        const pts = s.points;
        if (!pts || pts.length === 0) continue;
        if (pts.length === 1) {
            ctx.beginPath();
            ctx.arc(pts[0][0], pts[0][1], Math.max(0.5, s.width / 2), 0, Math.PI * 2);
            ctx.fill();
            continue;
        }
        ctx.beginPath();
        pts.forEach((p, i) => (i ? ctx.lineTo(p[0], p[1]) : ctx.moveTo(p[0], p[1])));
        ctx.stroke();
    }

    for (const t of pattern.texts || []) {
        ctx.font = `700 ${t.size}px sans-serif`;
        ctx.fillStyle = t.color;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(t.text, t.x, t.y);
    }
    return c;
}

/**
 * phase ∈ [0,1) のフレームを ctx に描画する。
 * @param contentCanvas buildContentCanvas() の結果 (キャッシュして渡す)
 */
export function renderFrame(ctx, pattern, contentCanvas, phase, W = CANVAS_W, H = CANVAS_H) {
    ctx.fillStyle = pattern.bg;
    ctx.fillRect(0, 0, W, H);

    const turns = pattern.spinTurns | 0;
    let off = 0;
    if (turns !== 0) {
        // 総移動 = turns × W。phase での水平オフセット(px)を [0,W) に正規化。
        off = ((((phase * turns) % 1) + 1) % 1) * W;
    }
    // 経度ラップ: 3 枚タイルでシームをまたいで連続描画。
    ctx.drawImage(contentCanvas, off - W, 0);
    ctx.drawImage(contentCanvas, off, 0);
    ctx.drawImage(contentCanvas, off + W, 0);
}

/** 尺と fps から総フレーム数 N を求める (末尾非重複なので N 枚 = phase 0..(N-1)/N)。 */
export function frameCount(pattern) {
    return Math.max(1, Math.round(pattern.fps * pattern.duration));
}
