import React from 'react';

// v2 アイコンセット — インライン SVG (stroke 1.5px の線画で統一, MUI icons 置換)
// 外部アイコン CDN は使用禁止 (オフライン LAN 配信のため)

const Svg = ({ size = 20, children, ...rest }) => (
    <svg
        width={size}
        height={size}
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.5"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
        {...rest}
    >
        {children}
    </svg>
);

export const IconPlay = (p) => (
    <Svg {...p}><path d="M7 4.5v15l12-7.5L7 4.5Z" fill="currentColor" stroke="none" /></Svg>
);

export const IconPause = (p) => (
    <Svg {...p}>
        <rect x="6" y="5" width="4" height="14" rx="1" fill="currentColor" stroke="none" />
        <rect x="14" y="5" width="4" height="14" rx="1" fill="currentColor" stroke="none" />
    </Svg>
);

export const IconStop = (p) => (
    <Svg {...p}><rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor" stroke="none" /></Svg>
);

export const IconSkipNext = (p) => (
    <Svg {...p}>
        <path d="M5 5.5v13l9-6.5-9-6.5Z" fill="currentColor" stroke="none" />
        <path d="M18 5v14" />
    </Svg>
);

export const IconSkipPrev = (p) => (
    <Svg {...p}>
        <path d="M19 5.5v13l-9-6.5 9-6.5Z" fill="currentColor" stroke="none" />
        <path d="M6 5v14" />
    </Svg>
);

export const IconLoop = (p) => (
    <Svg {...p}>
        <path d="M17 3.5 20 6.5l-3 3" />
        <path d="M20 6.5H8a4.5 4.5 0 0 0-4.5 4.5v.5" />
        <path d="M7 20.5 4 17.5l3-3" />
        <path d="M4 17.5h12a4.5 4.5 0 0 0 4.5-4.5v-.5" />
    </Svg>
);

export const IconGear = (p) => (
    <Svg {...p}>
        <circle cx="12" cy="12" r="3" />
        <path d="M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.7 1.7 0 0 0-1.87-.34 1.7 1.7 0 0 0-1.03 1.56V21a2 2 0 1 1-4 0v-.09a1.7 1.7 0 0 0-1.11-1.56 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.7 1.7 0 0 0 .34-1.87 1.7 1.7 0 0 0-1.56-1.03H3a2 2 0 1 1 0-4h.09A1.7 1.7 0 0 0 4.65 8.9a1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.7 1.7 0 0 0 1.87.34h.08A1.7 1.7 0 0 0 10.12 3V3a2 2 0 1 1 4 0v.09a1.7 1.7 0 0 0 1.03 1.56 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.7 1.7 0 0 0-.34 1.87v.08a1.7 1.7 0 0 0 1.56 1.03H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.56 1.03Z" />
    </Svg>
);

export const IconChevronDown = (p) => (
    <Svg {...p}><path d="m6 9 6 6 6-6" /></Svg>
);

export const IconChevronUp = (p) => (
    <Svg {...p}><path d="m6 15 6-6 6 6" /></Svg>
);

export const IconUpload = (p) => (
    <Svg {...p}>
        <path d="M12 16V4" />
        <path d="m7 9 5-5 5 5" />
        <path d="M4 16v3a1.5 1.5 0 0 0 1.5 1.5h13A1.5 1.5 0 0 0 20 19v-3" />
    </Svg>
);

export const IconTrash = (p) => (
    <Svg {...p}>
        <path d="M4 7h16" />
        <path d="M9 7V5a1.5 1.5 0 0 1 1.5-1.5h3A1.5 1.5 0 0 1 15 5v2" />
        <path d="M6.5 7 7.4 19a1.5 1.5 0 0 0 1.5 1.4h6.2a1.5 1.5 0 0 0 1.5-1.4L17.5 7" />
    </Svg>
);
