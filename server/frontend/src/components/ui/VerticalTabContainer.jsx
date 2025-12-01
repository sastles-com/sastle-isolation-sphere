import React, { useState } from 'react';
import { Box } from '@mui/material';
import { useSwipeable } from 'react-swipeable';

/**
 * VerticalTabContainer - Handles vertical swipe navigation within a main tab
 * Uses velocity-based detection to distinguish between scrolling and tab switching
 * @param {Array} subTabs - Array of sub-tab configurations
 * @param {Function} renderContent - Function to render content for each sub-tab
 */
export const VerticalTabContainer = ({ subTabs, renderContent }) => {
    const [currentSubTab, setCurrentSubTab] = useState(0);

    const swipeHandlers = useSwipeable({
        onSwiping: (eventData) => {
            // This prevents the swipe from being too sensitive during slow scrolling
            // We only want fast flicks to change tabs
        },
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
        preventScrollOnSwipe: false, // Allow normal scrolling
        delta: 80, // Minimum distance
        trackTouch: true,
    });

    return (
        <Box
            {...swipeHandlers}
            sx={{
                width: '100%',
                height: '100%',
                position: 'relative',
                overflow: 'hidden',
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
                        }}
                    >
                        {renderContent(subTab, index)}
                    </Box>
                ))}
            </Box>

            {/* Sub-tab Indicator (dots) */}
            <Box
                sx={{
                    position: 'absolute',
                    top: 10,
                    right: 10,
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 0.5,
                    zIndex: 2,
                }}
            >
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
        </Box>
    );
};
