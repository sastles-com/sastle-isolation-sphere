import React from 'react';
import { GlassButton } from '../ui/GlassButton';
import { GlassSlider } from '../ui/GlassSlider';
import {
    IconPlay, IconPause, IconStop, IconSkipNext, IconSkipPrev, IconLoop,
} from '../ui/icons';

/**
 * TransportDock — STAGE 下部のトランスポート (glass)。
 * ⏮ / ▶⏸ / ⏭ / ⟳ + 明るさミニスライダー常駐。
 * ⏮⏭ は P2 (トラック送り) で配線するため P1 では無効表示。
 */
export const TransportDock = ({
    isPlaying, isStreaming, loopOn,
    onTogglePlay, onStop, onToggleLoop,
    onPrev, onNext,
    brightness, onBrightnessChange, onBrightnessDragState,
}) => (
    <div
        className="glass"
        // ドック上の操作がステージのパン/フリック判定に化けないよう遮断
        // (capture ではなく bubble で止める — capture だと子のスライダーに届かない)
        onPointerDown={(e) => e.stopPropagation()}
        style={{
            margin: '0 16px',
            borderRadius: 'var(--radius)',
            padding: '10px 16px 4px',
            display: 'flex', flexDirection: 'column', gap: 0,
        }}
    >
        <div style={{
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 14,
        }}>
            <GlassButton title="前へ" onClick={onPrev} disabled={!onPrev}>
                <IconSkipPrev />
            </GlassButton>
            <GlassButton
                title={isPlaying ? '一時停止' : '再生'}
                onClick={onTogglePlay}
                active={isPlaying}
                style={{ width: 56, height: 56 }}
            >
                {isPlaying ? <IconPause size={26} /> : <IconPlay size={26} />}
            </GlassButton>
            <GlassButton title="次へ" onClick={onNext} disabled={!onNext}>
                <IconSkipNext />
            </GlassButton>
            <GlassButton title="停止" onClick={onStop} disabled={!isStreaming}>
                <IconStop size={18} />
            </GlassButton>
            <GlassButton title={loopOn ? 'ループ ON' : 'ループ OFF'} active={loopOn} onClick={onToggleLoop}>
                <IconLoop size={18} />
            </GlassButton>
        </div>
        <GlassSlider
            label="BRT"
            value={brightness}
            min={0} max={100}
            unit="%"
            onChange={onBrightnessChange}
            onDragStateChange={onBrightnessDragState}
        />
    </div>
);
