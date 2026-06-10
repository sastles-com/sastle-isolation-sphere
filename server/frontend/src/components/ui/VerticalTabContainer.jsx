import React, { useState } from 'react';
import { Box } from '@mui/material';
import { useSwipeable } from 'react-swipeable';
import KeyboardArrowUpIcon from '@mui/icons-material/KeyboardArrowUp';
import KeyboardArrowDownIcon from '@mui/icons-material/KeyboardArrowDown';

/**
 * NavArrowButton - Up/Down navigation button within a swipe rail
 */
const NavArrowButton = ({ direction, enabled, onClick }) => {
    const Icon = direction === 'up' ? KeyboardArrowUpIcon : KeyboardArrowDownIcon;
    return (
        <Box
            onClick={() => enabled && onClick()}
            sx={{
                width: 32,
                height: 32,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                borderRadius: '50%',
                border: '1px solid',
                borderColor: enabled ? 'primary.main' : 'rgba(255, 255, 255, 0.2)',
                bgcolor: enabled ? 'rgba(0, 229, 255, 0.1)' : 'transparent',
                cursor: enabled ? 'pointer' : 'default',
                opacity: enabled ? 1 : 0.3,
                pointerEvents: 'auto',
                transition: 'all 0.3s',
                '&:active': enabled ? {
                    transform: 'scale(0.9)',
                    bgcolor: 'rgba(0, 229, 255, 0.2)',
                } : {},
            }}
        >
            <Icon
                sx={{
                    color: enabled ? 'primary.main' : 'rgba(255, 255, 255, 0.3)',
                    fontSize: '1.2rem',
                }}
            />
        </Box>
    );
};

/**
 * SwipeNavRail - Vertical swipe area with up/down buttons and indicator dots
 * @param {'left' | 'right'} side - Which side of the container the rail sits on
 */
const SwipeNavRail = ({ side, swipeHandlers, subTabs, currentSubTab, setCurrentSubTab }) => (
    <Box
        {...swipeHandlers}
        sx={{
            width: '40px',
            flexShrink: 0,
            background: side === 'left'
                ? 'linear-gradient(to right, rgba(0, 229, 255, 0.05), transparent)'
                : 'linear-gradient(to left, rgba(0, 229, 255, 0.05), transparent)',
            ...(side === 'left'
                ? { borderRight: '1px solid rgba(0, 229, 255, 0.1)' }
                : { borderLeft: '1px solid rgba(0, 229, 255, 0.1)' }),
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'space-between',
            py: 1,
            position: 'relative',
            userSelect: 'none',
            touchAction: 'none',
        }}
    >
        <NavArrowButton
            direction="up"
            enabled={currentSubTab > 0}
            onClick={() => setCurrentSubTab(prev => prev - 1)}
        />

        {/* Sub-tab Indicator Dots */}
        <Box sx={{ display: 'flex', flexDirection: 'column', gap: 0.5 }}>
            {subTabs.map((_, index) => (
                <Box
                    key={index}
                    sx={{
                        width: 6,
                        height: 6,
                        borderRadius: '50%',
                        bgcolor: index === currentSubTab ? 'primary.main' : 'rgba(255, 255, 255, 0.3)',
                        boxShadow: index === currentSubTab ? '0 0 8px rgba(0, 229, 255, 0.8)' : 'none',
                        transition: 'all 0.3s',
                    }}
                />
            ))}
        </Box>

        <NavArrowButton
            direction="down"
            enabled={currentSubTab < subTabs.length - 1}
            onClick={() => setCurrentSubTab(prev => prev + 1)}
        />
    </Box>
);

/**
 * VerticalTabContainer - Handles vertical swipe navigation within a main tab
 * Uses side swipe areas to distinguish between scrolling and tab switching
 * @param {Array} subTabs - Array of sub-tab configurations
 * @param {Function} renderContent - Function to render content for each sub-tab
 */
export const VerticalTabContainer = ({ subTabs, renderContent }) => {
    const [currentSubTab, setCurrentSubTab] = useState(0);

    const verticalSwipeHandlers = useSwipeable({
        onSwipedUp: (eventData) => {
            // Only trigger if it was a fast swipe (velocity check)
            const velocity = Math.abs(eventData.velocity);
            if (velocity > 0.5) { // Fast swipe threshold
                setCurrentSubTab((prev) => Math.min(prev + 1, subTabs.length - 1));
            }
        },
        onSwipedDown: (eventData) => {
            const velocity = Math.abs(eventData.velocity);
            if (velocity > 0.5) {
                setCurrentSubTab((prev) => Math.max(prev - 1, 0));
            }
        },
        trackMouse: true,
        preventScrollOnSwipe: true, // Prevent scroll in swipe areas
        delta: 60, // Minimum distance
        trackTouch: true,
    });

    return (
        <Box
            sx={{
                width: '100%',
                height: '100%',
                display: 'flex',
                overflow: 'hidden',
            }}
        >
            {/* Left Swipe Area - Vertical Tab Navigation */}
            <SwipeNavRail
                side="left"
                swipeHandlers={verticalSwipeHandlers}
                subTabs={subTabs}
                currentSubTab={currentSubTab}
                setCurrentSubTab={setCurrentSubTab}
            />

            {/* Center Content Area - Scroll Only */}
            <Box
                sx={{
                    flex: 1,
                    height: '100%',
                    overflow: 'hidden',
                    position: 'relative',
                }}
            >
                {/* Sliding Container */}
                <Box
                    sx={{
                        display: 'flex',
                        flexDirection: 'column',
                        width: '100%',
                        height: `${subTabs.length * 100}%`,
                        transform: `translateY(-${currentSubTab * (100 / subTabs.length)}%)`,
                        transition: 'transform 0.4s cubic-bezier(0.4, 0, 0.2, 1)',
                    }}
                >
                    {subTabs.map((subTab, index) => (
                        <Box
                            key={subTab.id}
                            sx={{
                                width: '100%',
                                height: `${100 / subTabs.length}%`,
                                flexShrink: 0,
                                overflowY: 'auto',
                                overflowX: 'hidden',
                            }}
                        >
                            {renderContent(subTab, index)}
                        </Box>
                    ))}
                </Box>
            </Box>

            {/* Right Swipe Area - Vertical Tab Navigation (Mirror of Left) */}
            <SwipeNavRail
                side="right"
                swipeHandlers={verticalSwipeHandlers}
                subTabs={subTabs}
                currentSubTab={currentSubTab}
                setCurrentSubTab={setCurrentSubTab}
            />
        </Box>
    );
};
