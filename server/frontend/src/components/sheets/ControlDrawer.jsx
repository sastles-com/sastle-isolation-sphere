import React, { useState } from 'react';
import { AnimatePresence } from 'framer-motion';
import { DevicePanel } from '../control/DevicePanel';
import { OrientationPad } from '../control/OrientationPad';
import { TunePanel } from '../control/TunePanel';
import { PatternPanel } from '../control/PatternPanel';
import { ConfigForm } from '../control/ConfigForm';
import { LogViewer } from '../control/LogViewer';
import { GlassButton } from '../ui/GlassButton';
import { Section } from '../control/Section';
import { IconChevronUp } from '../ui/icons';

/**
 * ControlDrawer — DEVICE / ORIENTATION / TUNE / PATTERN / CONFIG / LOGS。
 * LOGS はタップでフルスクリーンのログビューアを開く (仕様 §2.3)。
 */
export const ControlDrawer = () => {
    const [logsOpen, setLogsOpen] = useState(false);

    return (
        <div style={{
            flex: 1, minHeight: 0, overflowY: 'auto',
            padding: '4px 20px 12px',
            paddingTop: 'max(12px, env(safe-area-inset-top))',
        }}>
            <DevicePanel />
            <OrientationPad />
            <TunePanel />
            <PatternPanel />
            <ConfigForm />

            <Section title="Logs">
                <GlassButton variant="pill" title="ログビューアを開く"
                    onClick={() => setLogsOpen(true)}
                    style={{ width: '100%', fontSize: 13, gap: 8 }}>
                    <IconChevronUp size={16} />
                    デバイスログを開く
                </GlassButton>
            </Section>

            <AnimatePresence>
                {logsOpen && <LogViewer onClose={() => setLogsOpen(false)} />}
            </AnimatePresence>
        </div>
    );
};
