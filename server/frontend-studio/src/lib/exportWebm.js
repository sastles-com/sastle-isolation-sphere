import WebMWriter from 'webm-writer';
import { buildContentCanvas, renderFrame, frameCount, CANVAS_W, CANVAS_H } from './patternRenderer';

/** ブラウザが canvas → WebP エンコードに対応しているか (webm-writer の前提)。 */
export function canExportWebm() {
    try {
        const c = document.createElement('canvas');
        c.width = c.height = 8;
        return c.toDataURL('image/webp').startsWith('data:image/webp');
    } catch {
        return false;
    }
}

/**
 * Pattern を VP8 webm(Blob) に書き出す。ループ不変条件に従い N フレームを phase 0..(N-1)/N で描画。
 * サーバ側 cv2 は VP8/VP9 webm を復号可能なことを検証済み。
 * @returns {Promise<{blob: Blob, frames: number}>}
 */
export async function exportPatternWebm(pattern, onProgress) {
    if (!canExportWebm()) {
        throw new Error('このブラウザは canvas→WebP 変換に非対応です (Chrome/Edge/新しめの Firefox を使用してください)。');
    }
    const W = CANVAS_W;
    const H = CANVAS_H;
    const N = frameCount(pattern);
    const content = buildContentCanvas(pattern, W, H);

    const canvas = document.createElement('canvas');
    canvas.width = W;
    canvas.height = H;
    const ctx = canvas.getContext('2d');

    const writer = new WebMWriter({ quality: 0.95, frameRate: pattern.fps });
    for (let i = 0; i < N; i++) {
        renderFrame(ctx, pattern, content, i / N, W, H);
        writer.addFrame(canvas);
        if (onProgress) onProgress(i + 1, N);
        if (i % 8 === 0) await new Promise((r) => setTimeout(r)); // UI に制御を返す
    }
    const blob = await writer.complete();
    return { blob, frames: N };
}
