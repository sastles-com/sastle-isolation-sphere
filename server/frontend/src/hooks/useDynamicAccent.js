import { useEffect } from 'react';

/**
 * useDynamicAccent — params.hue を document の --sphere-hue に反映する。
 * これにより --accent / --aurora など hue 依存トークンが UI 全体で即応する
 * (本デザインの署名: ダイナミックアクセント, 仕様 §0 / §3.1)。
 */
export const useDynamicAccent = (hue) => {
    useEffect(() => {
        if (typeof hue === 'number' && !Number.isNaN(hue)) {
            document.documentElement.style.setProperty('--sphere-hue', String(Math.round(hue)));
        }
    }, [hue]);
};
