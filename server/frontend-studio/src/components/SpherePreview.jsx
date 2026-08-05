import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';

/**
 * SpherePreview — 与えられた canvas を equirectangular テクスチャとして球にマップし、
 * ゆっくり自転させて実機の見え方を確認する。canvas の中身は親(App)の rAF が毎フレーム更新する。
 */
export const SpherePreview = ({ sourceCanvas, size = 260 }) => {
    const mountRef = useRef(null);

    useEffect(() => {
        if (!sourceCanvas || !mountRef.current) return;
        const mount = mountRef.current;

        const scene = new THREE.Scene();
        const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 100);
        camera.position.z = 3;

        const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setSize(size, size);
        mount.appendChild(renderer.domElement);

        const texture = new THREE.CanvasTexture(sourceCanvas);
        texture.colorSpace = THREE.SRGBColorSpace;
        // equirect マップは経度方向にラップ
        texture.wrapS = THREE.RepeatWrapping;

        const geo = new THREE.SphereGeometry(1, 64, 32);
        const mat = new THREE.MeshBasicMaterial({ map: texture });
        const sphere = new THREE.Mesh(geo, mat);
        scene.add(sphere);

        let raf;
        const animate = () => {
            sphere.rotation.y += 0.004;
            texture.needsUpdate = true; // canvas が更新され続けるため毎フレーム反映
            renderer.render(scene, camera);
            raf = requestAnimationFrame(animate);
        };
        animate();

        return () => {
            cancelAnimationFrame(raf);
            geo.dispose();
            mat.dispose();
            texture.dispose();
            renderer.dispose();
            if (renderer.domElement.parentNode === mount) mount.removeChild(renderer.domElement);
        };
    }, [sourceCanvas, size]);

    return <div ref={mountRef} style={{ width: size, height: size }} />;
};
