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
                id: 'controls',
                name: 'Controls',
                description: 'Parameter controls'
            },
            {
                id: 'presets',
                name: 'Presets',
                description: 'Saved presets'
            }
        ]
    },
    {
        id: 'control',
        name: 'CONTROL',
        angle: 270,
        subTabs: [
            {
                id: 'actions',
                name: 'Actions',
                description: 'Control actions'
            },
            {
                id: 'system',
                name: 'System',
                description: 'System information'
            }
        ]
    }
];
