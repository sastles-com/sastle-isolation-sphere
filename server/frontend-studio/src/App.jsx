import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
    CANVAS_W, CANVAS_H, buildContentCanvas, renderFrame, frameCount,
} from './lib/patternRenderer';
import { exportPatternWebm, canExportWebm } from './lib/exportWebm';
import { uploadPattern, fetchImageFormat } from './lib/api';
import { SpherePreview } from './components/SpherePreview';

const EQUATOR_BAND = 0.5; // 赤道帯: 中央 ±(H*BAND/2) を文字推奨域として表示

export const App = () => {
    // --- Pattern モデル ---
    const [bg, setBg] = useState('#05080f');
    const [strokes, setStrokes] = useState([]);   // [{color,width,points:[[x,y]]}]
    const [texts, setTexts] = useState([]);        // [{id,text,x,y,size,color}]
    const [spinTurns, setSpinTurns] = useState(1); // 整数周回 (±)。0=静止
    const [fps, setFps] = useState(15);
    const [duration, setDuration] = useState(4);

    // --- 編集ツール ---
    const [mode, setMode] = useState('draw');      // 'draw' | 'text'
    const [penColor, setPenColor] = useState('#ffd166');
    const [penWidth, setPenWidth] = useState(8);
    const [textValue, setTextValue] = useState('こんにちは');
    const [textColor, setTextColor] = useState('#4dd0e1');
    const [textSize, setTextSize] = useState(64);

    const [title, setTitle] = useState('pattern');
    const [status, setStatus] = useState('');
    const [busy, setBusy] = useState(false);
    const [fpsCap, setFpsCap] = useState(20);

    const editRef = useRef(null);
    const [previewCanvas, setPreviewCanvas] = useState(null); // callback ref で捕捉 (SpherePreview へ渡す)
    const contentRef = useRef(null);     // キャッシュしたコンテンツ層
    const drawing = useRef(null);        // 進行中のストローク
    const phaseRef = useRef(0);
    const lastTsRef = useRef(0);

    const pattern = useMemo(
        () => ({ bg, strokes, texts, spinTurns, fps, duration }),
        [bg, strokes, texts, spinTurns, fps, duration],
    );

    useEffect(() => {
        fetchImageFormat().then((img) => {
            if (img && img.fps) setFpsCap(img.fps);
            if (img && img.fps && img.fps < fps) setFps(img.fps);
        });
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    // コンテンツ層を再構築 (strokes/texts 変化時)。プレビュー/書き出しで共有。
    useEffect(() => {
        contentRef.current = buildContentCanvas(pattern);
    }, [strokes, texts, pattern]);

    // --- 編集キャンバス描画 (静止 + 赤道帯ガイド) ---
    const drawEditor = useCallback(() => {
        const c = editRef.current;
        if (!c) return;
        const ctx = c.getContext('2d');
        ctx.fillStyle = bg;
        ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);
        const content = contentRef.current || buildContentCanvas(pattern);
        ctx.drawImage(content, 0, 0);
        // 赤道帯ガイド
        const bandH = CANVAS_H * EQUATOR_BAND;
        ctx.strokeStyle = 'rgba(255,255,255,0.18)';
        ctx.setLineDash([6, 6]);
        ctx.lineWidth = 1;
        ctx.strokeRect(0, (CANVAS_H - bandH) / 2, CANVAS_W, bandH);
        ctx.setLineDash([]);
        ctx.strokeStyle = 'rgba(255,255,255,0.28)';
        ctx.beginPath();
        ctx.moveTo(0, CANVAS_H / 2);
        ctx.lineTo(CANVAS_W, CANVAS_H / 2);
        ctx.stroke();
    }, [bg, pattern]);

    useEffect(() => { drawEditor(); }, [drawEditor, strokes, texts]);

    // --- プレビュー rAF (実速度でループ再生) ---
    useEffect(() => {
        if (!previewCanvas) return undefined;
        let raf;
        const ctx = previewCanvas.getContext('2d');
        const tick = (ts) => {
            const dt = lastTsRef.current ? (ts - lastTsRef.current) : 0;
            lastTsRef.current = ts;
            const dur = Math.max(0.1, duration);
            phaseRef.current = (phaseRef.current + dt / 1000 / dur) % 1;
            const content = contentRef.current || buildContentCanvas(pattern);
            renderFrame(ctx, pattern, content, phaseRef.current);
            raf = requestAnimationFrame(tick);
        };
        raf = requestAnimationFrame(tick);
        return () => cancelAnimationFrame(raf);
    }, [pattern, duration, previewCanvas]);

    // --- ポインタ座標 → キャンバスピクセル ---
    const toCanvas = (e) => {
        const c = editRef.current;
        const rect = c.getBoundingClientRect();
        return [
            ((e.clientX - rect.left) / rect.width) * CANVAS_W,
            ((e.clientY - rect.top) / rect.height) * CANVAS_H,
        ];
    };

    const onPointerDown = (e) => {
        e.currentTarget.setPointerCapture?.(e.pointerId);
        const [x, y] = toCanvas(e);
        if (mode === 'text') {
            if (!textValue.trim()) return;
            setTexts((t) => [...t, {
                id: `${Math.round(x)}_${Math.round(y)}_${t.length}`,
                text: textValue, x, y, size: textSize, color: textColor,
            }]);
            return;
        }
        drawing.current = { color: penColor, width: penWidth, points: [[x, y]] };
        setStrokes((s) => [...s, drawing.current]);
    };
    const onPointerMove = (e) => {
        if (mode !== 'draw' || !drawing.current) return;
        const [x, y] = toCanvas(e);
        drawing.current.points.push([x, y]);
        setStrokes((s) => s.slice()); // 再描画トリガ
    };
    const onPointerUp = () => { drawing.current = null; };

    const undoStroke = () => setStrokes((s) => s.slice(0, -1));
    const clearAll = () => { setStrokes([]); setTexts([]); };
    const removeText = (id) => setTexts((t) => t.filter((x) => x.id !== id));

    const handleSave = async () => {
        if (!canExportWebm()) {
            setStatus('⚠ このブラウザは書き出し非対応 (Chrome/Edge/Firefox 推奨)');
            return;
        }
        setBusy(true);
        setStatus('書き出し中… 0%');
        try {
            const { blob, frames } = await exportPatternWebm(pattern, (i, n) => {
                setStatus(`書き出し中… ${Math.round((i / n) * 100)}% (${i}/${n})`);
            });
            setStatus(`アップロード中… (${frames}フレーム / ${(blob.size / 1024).toFixed(0)}KB)`);
            const v = await uploadPattern(blob, title);
            setStatus(`✓ 保存しました: "${v.title}" (id=${v.id})`);
        } catch (err) {
            setStatus(`✗ ${err.message || err}`);
        } finally {
            setBusy(false);
        }
    };

    const N = frameCount(pattern);

    return (
        <div className="studio">
            <header className="bar">
                <h1>Pattern Studio</h1>
                <span className="muted">equirectangular {CANVAS_W}×{CANVAS_H} → 320×160 / VP8 webm</span>
            </header>

            <div className="layout">
                {/* 左: ツール */}
                <aside className="panel">
                    <div className="seg">
                        <button className={mode === 'draw' ? 'on' : ''} onClick={() => setMode('draw')}>✎ 手書き</button>
                        <button className={mode === 'text' ? 'on' : ''} onClick={() => setMode('text')}>T テキスト</button>
                    </div>

                    {mode === 'draw' ? (
                        <div className="group">
                            <label>ペン色 <input type="color" value={penColor} onChange={(e) => setPenColor(e.target.value)} /></label>
                            <label>太さ {penWidth}
                                <input type="range" min="1" max="40" value={penWidth} onChange={(e) => setPenWidth(+e.target.value)} />
                            </label>
                            <div className="row">
                                <button onClick={undoStroke} disabled={!strokes.length}>1つ戻す</button>
                                <button onClick={clearAll} disabled={!strokes.length && !texts.length}>全消去</button>
                            </div>
                        </div>
                    ) : (
                        <div className="group">
                            <label>文字<input type="text" value={textValue} onChange={(e) => setTextValue(e.target.value)} /></label>
                            <label>色 <input type="color" value={textColor} onChange={(e) => setTextColor(e.target.value)} /></label>
                            <label>サイズ {textSize}
                                <input type="range" min="16" max="160" value={textSize} onChange={(e) => setTextSize(+e.target.value)} />
                            </label>
                            <p className="muted small">キャンバスの赤道帯をタップして配置</p>
                            {texts.length > 0 && (
                                <ul className="textlist">
                                    {texts.map((t) => (
                                        <li key={t.id}><span>{t.text}</span><button onClick={() => removeText(t.id)}>×</button></li>
                                    ))}
                                </ul>
                            )}
                        </div>
                    )}

                    <div className="group">
                        <label>背景 <input type="color" value={bg} onChange={(e) => setBg(e.target.value)} /></label>
                        <label>スピン(周回) {spinTurns}
                            <input type="range" min="-4" max="4" step="1" value={spinTurns} onChange={(e) => setSpinTurns(+e.target.value)} />
                        </label>
                        <span className="muted small">整数周回のみ = ループ端が連続</span>
                    </div>

                    <div className="group">
                        <label>尺(秒) {duration}
                            <input type="range" min="1" max="20" step="1" value={duration} onChange={(e) => setDuration(+e.target.value)} />
                        </label>
                        <label>fps {fps} (上限 {fpsCap})
                            <input type="range" min="5" max={fpsCap} step="1" value={fps} onChange={(e) => setFps(+e.target.value)} />
                        </label>
                        <span className="muted small">{N} フレーム出力</span>
                    </div>

                    <div className="group save">
                        <label>タイトル<input type="text" value={title} onChange={(e) => setTitle(e.target.value)} /></label>
                        <button className="primary" onClick={handleSave} disabled={busy}>
                            {busy ? '処理中…' : 'Sphere へ保存'}
                        </button>
                        {status && <p className="status">{status}</p>}
                    </div>
                </aside>

                {/* 右: 編集 + プレビュー */}
                <main className="stage">
                    <div className="block">
                        <div className="caption">編集 (2:1・赤道帯ガイド)</div>
                        <canvas
                            ref={editRef}
                            width={CANVAS_W}
                            height={CANVAS_H}
                            className="canvas edit"
                            style={{ touchAction: 'none', cursor: mode === 'text' ? 'text' : 'crosshair' }}
                            onPointerDown={onPointerDown}
                            onPointerMove={onPointerMove}
                            onPointerUp={onPointerUp}
                            onPointerLeave={onPointerUp}
                        />
                    </div>
                    <div className="previews">
                        <div className="block">
                            <div className="caption">プレビュー (ループ)</div>
                            <canvas ref={setPreviewCanvas} width={CANVAS_W} height={CANVAS_H} className="canvas" />
                        </div>
                        <div className="block">
                            <div className="caption">球面</div>
                            <SpherePreview sourceCanvas={previewCanvas} size={240} />
                        </div>
                    </div>
                </main>
            </div>
        </div>
    );
};
