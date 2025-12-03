import React, { useRef, useEffect } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { Sphere } from '@react-three/drei';
import { useWebSocket } from '../../contexts/WebSocketContext';
import * as THREE from 'three';

// HUE (0-360) to RGB hex conversion
const hueToRgb = (hue) => {
    // Normalize hue to 0-360 range
    const normalizedHue = ((hue % 360) + 360) % 360;
    const h = normalizedHue / 60;
    const c = 1;
    const x = c * (1 - Math.abs((h % 2) - 1));
    let r = 0, g = 0, b = 0;
    
    if (h >= 0 && h < 1) { r = c; g = x; b = 0; }
    else if (h >= 1 && h < 2) { r = x; g = c; b = 0; }
    else if (h >= 2 && h < 3) { r = 0; g = c; b = x; }
    else if (h >= 3 && h < 4) { r = 0; g = x; b = c; }
    else if (h >= 4 && h < 5) { r = x; g = 0; b = c; }
    else if (h >= 5 && h < 6) { r = c; g = 0; b = x; }
    
    const toHex = (val) => Math.round(val * 255).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
};

const IMUControlledSphere = ({ hue, brightness }) => {
    const meshRef = useRef();
    const { lastMessage } = useWebSocket();
    const quaternionRef = useRef(new THREE.Quaternion(0, 0, 0, 1)); // x, y, z, w

    useEffect(() => {
        // Update quaternion when IMU data received
        if (lastMessage && lastMessage.type === 'STATE_UPDATE' && lastMessage.payload?.imu) {
            const { w, x, y, z } = lastMessage.payload.imu;
            // Three.js uses (x, y, z, w) order
            quaternionRef.current.set(x, y, z, w);
        }
    }, [lastMessage]);

    useFrame(() => {
        if (meshRef.current) {
            // Apply quaternion rotation from IMU
            meshRef.current.quaternion.copy(quaternionRef.current);
        }
    });

    // Convert HUE to color
    const sphereColor = hueToRgb(hue);
    // Convert brightness (0-100) to emissive intensity (0-1)
    const emissiveIntensity = brightness / 100;

    return (
        <Sphere args={[1, 32, 32]} ref={meshRef} scale={2.2}>
            <meshStandardMaterial
                color={sphereColor}
                wireframe={true}
                emissive={sphereColor}
                emissiveIntensity={emissiveIntensity}
                transparent
                opacity={0.6}
            />
        </Sphere>
    );
};

export const HoloSphere = ({ color = 120, brightness = 80 }) => {
    return (
        <div style={{ width: '100%', height: '100%', minHeight: '300px' }}>
            <Canvas camera={{ position: [0, 0, 6] }}>
                <ambientLight intensity={0.5} />
                <pointLight position={[10, 10, 10]} />
                <IMUControlledSphere hue={color} brightness={brightness} />
                {/* OrbitControls removed - sphere controlled by IMU only */}
            </Canvas>
        </div>
    );
};
