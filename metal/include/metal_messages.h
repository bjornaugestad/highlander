#ifndef METAL_MESSAGES_H
#define METAL_MESSAGES_H

// MM_ is the general prefix for Metal Messages.
// MM_USER is the offset for user defined messages
// MM_AUDIO_ and MM_VIDEO_ prefix domain specific messages.
//
#define MM_BASE 1000 // Randomly chosen value

#define MM_VIDEO_BASE (MM_BASE + 1000)
#define MM_AUDIO_BASE (MM_BASE + 2000)
#define MM_USER_BASE  (MM_BASE + 3000)
#define MM_SYS_BASE   (MM_BASE + 4000)


// Some system messages
#define MM_EXIT (MM_SYS_BASE + 1)



#endif
