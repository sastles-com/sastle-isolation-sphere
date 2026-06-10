// Mock Data (モバイルテック感)
// TODO: バックエンドのデータベース統合後に API 取得へ置き換える
export const INITIAL_VIDEOS = [
    {
        id: 1,
        uuid: 'v_001',
        title: 'Cyber City',
        description: 'Neon-lit cityscape at night',
        thumbnail_path: null,
        duration_ms: 320000, // 5:20
        size_bytes: 29360128, // 28MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['cyberpunk', 'neon', 'city'],
        uploaded_at: '2025-12-02T10:30:00Z'
    },
    {
        id: 2,
        uuid: 'v_002',
        title: 'Ambient Rain',
        description: 'Calming rain sounds with visuals',
        thumbnail_path: null,
        duration_ms: 600000, // 10:00
        size_bytes: 47185920, // 45MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['ambient', 'nature', 'rain'],
        uploaded_at: '2025-12-01T15:20:00Z'
    },
    {
        id: 3,
        uuid: 'v_003',
        title: 'Sunrise',
        description: 'Morning sunrise timelapse',
        thumbnail_path: null,
        duration_ms: 150000, // 2:30
        size_bytes: 15728640, // 15MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['nature', 'sunrise', 'timelapse'],
        uploaded_at: '2025-12-01T08:00:00Z'
    },
    {
        id: 4,
        uuid: 'v_004',
        title: 'Coffee Brewing',
        description: 'Coffee making process',
        thumbnail_path: null,
        duration_ms: 225000, // 3:45
        size_bytes: 20971520, // 20MB
        width: 320,
        height: 160,
        fps: 30,
        tags: ['coffee', 'morning', 'process'],
        uploaded_at: '2025-11-30T09:15:00Z'
    }
];
