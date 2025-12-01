import React, { useState } from 'react';
import { FrameCorners } from '@arwes/react';

export const CyberButton = ({ children, onClick, style = {}, color = '#00F0FF' }) => {
    const [isHovered, setIsHovered] = useState(false);
    const [isPressed, setIsPressed] = useState(false);

    return (
        <div
            style={{
                position: 'relative',
                display: 'inline-block',
                cursor: 'pointer',
                userSelect: 'none',
                ...style
            }}
            onMouseEnter={() => setIsHovered(true)}
            onMouseLeave={() => { setIsHovered(false); setIsPressed(false); }}
            onMouseDown={() => setIsPressed(true)}
            onMouseUp={() => setIsPressed(false)}
            onClick={onClick}
        >
            <FrameCorners
                strokeWidth={2}
                cornerLength={10}
                style={{
                    // Simple visual feedback via opacity/color
                    opacity: isHovered ? 1 : 0.7,
                    color: color,
                    transition: 'all 0.2s ease-out'
                }}
            >
                <div style={{
                    padding: '10px 20px',
                    background: isPressed ? `rgba(${parseInt(color.slice(1, 3), 16)}, ${parseInt(color.slice(3, 5), 16)}, ${parseInt(color.slice(5, 7), 16)}, 0.2)` : 'transparent',
                    textAlign: 'center',
                    transition: 'background 0.1s'
                }}>
                    {children}
                </div>
            </FrameCorners>
        </div>
    );
};
