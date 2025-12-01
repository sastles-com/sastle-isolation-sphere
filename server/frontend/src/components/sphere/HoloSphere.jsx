import React, { useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Sphere } from '@react-three/drei';

const RotatingSphere = () => {
    const meshRef = useRef();

    useFrame((state, delta) => {
        if (meshRef.current) {
            meshRef.current.rotation.y += delta * 0.1;
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
                <RotatingSphere />
                <OrbitControls enableZoom={true} enablePan={false} minDistance={3} maxDistance={10} />
            </Canvas>
        </div>
    );
};
