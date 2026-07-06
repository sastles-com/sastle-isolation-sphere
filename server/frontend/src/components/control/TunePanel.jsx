import React, { useRef, useState } from 'react';
import { useStateUpdate } from '../../hooks/useSphereState';
import { useParamSender } from '../../hooks/useParamSender';
import { GlassSlider } from '../ui/GlassSlider';
import { Section } from './Section';

/**
 * TunePanel — TUNE セクション。speed / saturation スライダー。
 * SET_PARAMS を 60ms デバウンスで送信。ドラッグ中はエコーバック抑制 (仕様 §5)。
 * (brightness/hue は STAGE のエッジドラッグ側で扱う)
 */
export const TunePanel = () => {
    const sendParam = useParamSender();
    const [speed, setSpeed] = useState(50);
    const [saturation, setSaturation] = useState(80);
    const dragging = useRef({ speed: false, saturation: false });

    useStateUpdate((payload) => {
        if (!payload.params) return;
        if (!dragging.current.speed && typeof payload.params.speed === 'number') {
            setSpeed(payload.params.speed);
        }
        if (!dragging.current.saturation && typeof payload.params.saturation === 'number') {
            setSaturation(payload.params.saturation);
        }
    });

    const handle = (key, setter) => (v) => { setter(v); sendParam(key, v); };
    const setDrag = (key) => (d) => { dragging.current[key] = d; };

    return (
        <Section title="Tune">
            <GlassSlider label="SPEED" value={speed} min={0} max={100}
                onChange={handle('speed', setSpeed)} onDragStateChange={setDrag('speed')} />
            <GlassSlider label="SAT" value={saturation} min={0} max={100} unit="%"
                onChange={handle('saturation', setSaturation)} onDragStateChange={setDrag('saturation')} />
        </Section>
    );
};
