import React, { useRef, useEffect } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { Sphere } from '@react-three/drei';
import { useWebSocket } from '../../contexts/WebSocketContext';
import * as THREE from 'three';

const IMUControlledSphere = () => {
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

    return (
        <Sphere args={[1, 32, 32]} ref={meshRef} scale={2.2}>
            <meshStandardMaterial
                color="#00F0FF"
                wireframe={true}
                emissive="#00F0FF"
                emissiveIntensity={0.8}
                transparent
                opacity={0.6}
            />
        </Sphere>
    );
};

export const HoloSphere = () => {
    return (
        <div style={{ width: '100%', height: '100%', minHeight: '300px' }}>
            <Canvas camera={{ position: [0, 0, 6] }}>
                <ambientLight intensity={0.5} />
                <pointLight position={[10, 10, 10]} />
                <IMUControlledSphere />
                {/* OrbitControls removed - sphere controlled by IMU only */}
            </Canvas>
        </div>
    );
};
