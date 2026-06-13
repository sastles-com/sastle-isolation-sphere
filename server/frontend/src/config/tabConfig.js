export const TAB_CONFIG = [
    {
        id: 'sphere',
        name: 'SPHERE',
        angle: 0,
        subTabs: [] // No vertical tabs - single integrated dashboard
    },
    {
        id: 'playlist',
        name: 'PLAYLIST',
        angle: 90,
        subTabs: [
            {
                id: 'playlists',
                name: 'Playlists',
                description: 'Manage playlists'
            },
            {
                id: 'videos',
                name: 'Videos',
                description: 'Manage video library'
            }
        ]
    },
    {
        id: 'params',
        name: 'PARAMS',
        angle: 180,
        subTabs: [
            {
                id: 'config',
                name: 'Config',
                description: 'System configuration'
            },
            {
                id: 'parameters',
                name: 'Parameters',
                description: 'Operational parameters'
            }
        ]
    },
    {
        id: 'control',
        name: 'CONTROL',
        angle: 270,
        subTabs: [
            {
                id: 'sphere_control',
                name: 'Sphere',
                description: 'Hardware control'
            },
            {
                id: 'pattern_control',
                name: 'Pattern',
                description: 'Pattern control'
            },
            {
                id: 'debug_log',
                name: 'Logs',
                description: 'Device debug log'
            }
        ]
    }
];
