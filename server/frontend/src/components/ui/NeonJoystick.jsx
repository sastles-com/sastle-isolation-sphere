import React, { useState, useRef, useEffect, useCallback } from 'react';
import { Box } from '@mui/material';

export const NeonJoystick = ({ size = 150, onChange }) => {
    const containerRef = useRef(null);
    const [position, setPosition] = useState({ x: 0, y: 0 });
    const [isDragging, setIsDragging] = useState(false);

    const radius = size / 2;
    const knobSize = size * 0.3;
    const maxDist = radius - knobSize / 2;

    const handleStart = (clientX, clientY) => {
        setIsDragging(true);
        updatePosition(clientX, clientY);
    };

    const handleMove = useCallback((clientX, clientY) => {
        if (!isDragging) return;
        updatePosition(clientX, clientY);
    }, [isDragging]);

    const handleEnd = () => {
        setIsDragging(false);
        setPosition({ x: 0, y: 0 });
        if (onChange) onChange({ x: 0, y: 0 });
    };

    const updatePosition = (clientX, clientY) => {
        if (!containerRef.current) return;
        const rect = containerRef.current.getBoundingClientRect();
        const centerX = rect.left + radius;
        const centerY = rect.top + radius;

        let dx = clientX - centerX;
        let dy = clientY - centerY;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist > maxDist) {
            const ratio = maxDist / dist;
            dx *= ratio;
            dy *= ratio;
        }

        setPosition({ x: dx, y: dy });

        if (onChange) {
            // Normalize to -1 to 1
            onChange({
                x: dx / maxDist,
                y: dy / maxDist
            });
        }
    };

    // Mouse events
    useEffect(() => {
        const onMouseMove = (e) => handleMove(e.clientX, e.clientY);
        const onMouseUp = () => handleEnd();

        if (isDragging) {
            window.addEventListener('mousemove', onMouseMove);
            window.addEventListener('mouseup', onMouseUp);
        }

        return () => {
            window.removeEventListener('mousemove', onMouseMove);
            window.removeEventListener('mouseup', onMouseUp);
        };
    }, [isDragging, handleMove]);

    return (
        <Box
            ref={containerRef}
            sx={{
                width: size,
                height: size,
                borderRadius: '50%',
                border: '2px solid rgba(0, 229, 255, 0.3)',
                bgcolor: 'rgba(0, 0, 0, 0.3)',
                boxShadow: '0 0 15px rgba(0, 229, 255, 0.1), inset 0 0 20px rgba(0, 229, 255, 0.1)',
                position: 'relative',
                touchAction: 'none', // Prevent scrolling
                cursor: 'pointer',
                mx: 'auto', // Center horizontally
            }}
            onMouseDown={(e) => handleStart(e.clientX, e.clientY)}
            onTouchStart={(e) => {
                // e.preventDefault(); // Prevent scrolling - handled by touchAction: none
                const touch = e.touches[0];
                handleStart(touch.clientX, touch.clientY);
            }}
            onTouchMove={(e) => {
                // e.preventDefault();
                const touch = e.touches[0];
                handleMove(touch.clientX, touch.clientY);
            }}
            onTouchEnd={handleEnd}
        >
            {/* Center Point */}
            <Box
                sx={{
                    position: 'absolute',
                    top: '50%',
                    left: '50%',
                    width: 4,
                    height: 4,
                    bgcolor: 'rgba(0, 229, 255, 0.5)',
                    borderRadius: '50%',
                    transform: 'translate(-50%, -50%)',
                }}
            />

            {/* Knob */}
            <Box
                sx={{
                    position: 'absolute',
                    top: '50%',
                    left: '50%',
                    width: knobSize,
                    height: knobSize,
                    borderRadius: '50%',
                    bgcolor: isDragging ? 'primary.main' : 'rgba(0, 229, 255, 0.8)',
                    boxShadow: isDragging
                        ? '0 0 20px rgba(0, 229, 255, 0.8), inset 0 0 10px rgba(255, 255, 255, 0.5)'
                        : '0 0 10px rgba(0, 229, 255, 0.5)',
                    transform: `translate(calc(-50% + ${position.x}px), calc(-50% + ${position.y}px))`,
                    transition: isDragging ? 'none' : 'transform 0.2s cubic-bezier(0.175, 0.885, 0.32, 1.275)',
                    pointerEvents: 'none', // Pass events to container
                }}
            />
        </Box>
    );
};
