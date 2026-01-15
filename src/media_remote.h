/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - Media Remote Control Header
 */

#ifndef MEDIA_REMOTE_H__
#define MEDIA_REMOTE_H__

#include <stdint.h>
#include <stdbool.h>

/**@brief Media control commands */
typedef enum
{
    MEDIA_REMOTE_PLAY_PAUSE,
    MEDIA_REMOTE_NEXT,
    MEDIA_REMOTE_PREVIOUS,
    MEDIA_REMOTE_VOLUME_UP,
    MEDIA_REMOTE_VOLUME_DOWN,
} media_remote_cmd_t;

/**@brief Function for initializing the media remote. */
uint32_t media_remote_init(void);

/**@brief Function for sending a media control command. */
uint32_t media_remote_send_cmd(media_remote_cmd_t cmd);

#endif // MEDIA_REMOTE_H__
