/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include "shadow.h"

#include "shadow_channels.h"

#ifdef _WIN32
extern BOOL whd_clipboard_start(rdpShadowClient* client);
extern void whd_clipboard_stop(rdpShadowClient* client);
#endif

UINT shadow_client_channels_post_connect(rdpShadowClient* client)
{
	if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, ENCOMSP_SVC_CHANNEL_NAME))
	{
		if (shadow_client_encomsp_init(client) < 0)
			return ERROR_NOT_READY;
	}

	if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, REMDESK_SVC_CHANNEL_NAME))
	{
		if (shadow_client_remdesk_init(client) < 0)
			return ERROR_NOT_READY;
	}

    const char* audio=getenv("SURFACE_DESK_AUDIO");
	if (audio && strcmp(audio,"1")==0 &&
        freerdp_settings_get_bool(client->context.settings,FreeRDP_AudioPlayback) &&
        !freerdp_settings_get_bool(client->context.settings,FreeRDP_RemoteConsoleAudio) &&
        WTSVirtualChannelManagerIsChannelJoined(client->vcm, RDPSND_CHANNEL_NAME))
	{
		if (shadow_client_rdpsnd_init(client) <= 0) {
            shadow_client_rdpsnd_uninit(client);
            fprintf(stderr,"SurfaceDesk AUDIO_CHANNEL state=init-error\n");fflush(stderr);
        }
	}

	else if(audio && strcmp(audio,"1")==0) {
        fprintf(stderr,"SurfaceDesk AUDIO_CHANNEL state=client-disabled\n");fflush(stderr);
    }
    /* This subsystem implements system playback only, never microphone redirection. */
	if (client->subsystem->AudinServerReceiveSamples && !shadow_client_audin_init(client))
		return ERROR_NOT_READY;

	if (shadow_client_rdpgfx_init(client) < 0)
		return ERROR_NOT_READY;


#ifdef _WIN32
    if (!whd_clipboard_start(client)) { fprintf(stderr,"WinHomeDesk CLIPBOARD init-error\n"); }
#endif
	return CHANNEL_RC_OK;
}

void shadow_client_channels_free(rdpShadowClient* client)
{
#ifdef _WIN32
    whd_clipboard_stop(client);
#endif
	shadow_client_rdpgfx_uninit(client);
	shadow_client_audin_uninit(client);
	shadow_client_rdpsnd_uninit(client);
	shadow_client_remdesk_uninit(client);
	shadow_client_encomsp_uninit(client);
}
