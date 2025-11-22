import React, { useState, useRef, useEffect } from 'react';

export const NeonDial = ({ value, min, max, onChange, label }) => {
    const [isDragging, setIsDragging] = useState(false);
    const dialRef = useRef(null);

    // Convert value to angle (-135 to 135 degrees)
    const valueToAngle = (val) => {
        const percent = (val - min) / (max - min);
        return percent * 270 - 135;
    };

    // Convert angle to value
    const angleToValue = (angle) => {
        // Normalize angle to 0-270 range (from -135 start)
        let normalizedAngle = angle + 135;
        if (normalizedAngle < 0) normalizedAngle = 0;
        if (normalizedAngle > 270) normalizedAngle = 270;

        const percent = normalizedAngle / 270;
        return Math.round(min + percent * (max - min));
    };

    const handlePointerDown = (e) => {
        e.preventDefault();
        setIsDragging(true);
        updateValueFromPointer(e);
    };

    const updateValueFromPointer = (e) => {
        if (!dialRef.current) return;
        const rect = dialRef.current.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;

        // Calculate angle from center to pointer
        // atan2 returns angle in radians from -PI to PI
        // -PI/2 is up, 0 is right, PI/2 is down, PI/-PI is left
        const x = e.clientX - centerX;
        const y = e.clientY - centerY;
        let angleRad = Math.atan2(x, -y);
        let angleDeg = angleRad * (180 / Math.PI);

        // Now 0 is Top.
        // We want range from -135 (Bottom Left) to 135 (Bottom Right).
        // Clamp to valid range
        if (angleDeg > 135) angleDeg = 135;
        if (angleDeg < -135) angleDeg = -135;

        const newValue = angleToValue(angleDeg);
        onChange(newValue);
    };

    useEffect(() => {
        const handlePointerMove = (e) => {
            if (isDragging) {
                e.preventDefault();
                updateValueFromPointer(e);
            }
        };

        const handlePointerUp = () => {
            setIsDragging(false);
        };

        if (isDragging) {
            window.addEventListener('pointermove', handlePointerMove);
            window.addEventListener('pointerup', handlePointerUp);
        }

        return () => {
            window.removeEventListener('pointermove', handlePointerMove);
            window.removeEventListener('pointerup', handlePointerUp);
        };
    }, [isDragging]);

    const rotation = valueToAngle(value);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', width: 100, margin: 10 }}>
            <div
                ref={dialRef}
                onPointerDown={handlePointerDown}
                style={{
                    width: 80,
                    height: 80,
                    position: 'relative',
                    cursor: isDragging ? 'grabbing' : 'grab',
                    touchAction: 'none',
                    userSelect: 'none'
                }}
            >
                {/* Outer Ring Glow */}
                <div style={{
                    position: 'absolute',
                    inset: 0,
                    borderRadius: '50%',
                    border: '2px solid rgba(0, 240, 255, 0.2)',
                    boxShadow: '0 0 15px rgba(0, 240, 255, 0.1)'
                }} />

                {/* Active Arc Indicator (Visual only) */}
                <div style={{
                    position: 'absolute',
                    inset: -2,
                    borderRadius: '50%',
                    border: '2px solid transparent',
                    borderTopColor: '#00F0FF',
                    borderRightColor: '#00F0FF',
                    transform: `rotate(${rotation}deg)`,
                    opacity: 0.5,
                    pointerEvents: 'none',
                    transition: isDragging ? 'none' : 'transform 0.1s ease-out'
                }} />

                {/* Knob Indicator */}
                <div style={{
                    position: 'absolute',
                    top: '50%',
                    left: '50%',
                    width: '100%',
                    height: '100%',
                    transform: `translate(-50%, -50%) rotate(${rotation}deg)`,
                    pointerEvents: 'none',
                    transition: isDragging ? 'none' : 'transform 0.1s ease-out'
                }}>
                    <div style={{
                        position: 'absolute',
                        top: 5,
                        left: '50%',
                        width: 6,
                        height: 12,
                        background: '#00F0FF',
                        transform: 'translateX(-50%)',
                        boxShadow: '0 0 8px #00F0FF',
                        borderRadius: 2
                    }} />
                </div>

                {/* Value Display */}
                <div style={{
                    position: 'absolute',
                    top: '50%',
                    left: '50%',
                    transform: 'translate(-50%, -50%)',
                    color: '#00F0FF',
                    fontFamily: '"Source Code Pro", monospace',
                    fontSize: '1.5rem',
                    fontWeight: 'bold',
                    textShadow: '0 0 5px rgba(0, 240, 255, 0.5)',
                    pointerEvents: 'none'
                }}>
                    {Math.round(value)}
                </div>
            </div>
            <div style={{
                marginTop: 15,
                color: 'rgba(0, 240, 255, 0.8)',
                fontFamily: '"Titillium Web", sans-serif',
                fontSize: '0.9rem',
                fontWeight: 600,
                letterSpacing: 1,
                textTransform: 'uppercase'
            }}>
                {label}
            </div>
        </div>
    );
};
