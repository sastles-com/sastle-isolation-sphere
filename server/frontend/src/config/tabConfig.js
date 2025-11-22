export const TAB_CONFIG = [
    {
        id: 'sphere',
        name: 'SPHERE',
        angle: 0,
        subTabs: [
            {
                id: 'view',
                name: 'View',
                description: 'Sphere visualization'
            },
            {
                id: 'settings',
                name: 'Settings',
                description: 'Sphere configuration'
            }
        ]
    },
    {
        id: 'params',
        name: 'PARAMS',
        angle: 120,
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
        angle: 240,
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
