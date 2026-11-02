/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/log.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/server/server-common.h>

#include "shadow.h"

#include "shadow_rdpsnd.h"
#include "surface_audio_timing.h"

#define TAG SERVER_TAG("shadow")

static UINT surface_audio_confirm(RdpsndServerContext* context,BYTE block,UINT16 stamp) {
    rdpShadowClient* client=context->data;
    surface_audio_timing_confirm(client->surfaceAudioTiming,block,stamp,GetTickCount64());return CHANNEL_RC_OK;
}
static void rdpsnd_activated(RdpsndServerContext* context)
{
	WINPR_ASSERT(context);
    rdpShadowClient* client=context->data;
	for (size_t i = 0; i < context->num_client_formats; i++)
	{
		for (size_t j = 0; j < context->num_server_formats; j++)
		{
			if (audio_format_compatible(&context->server_formats[j], &context->client_formats[i]) &&
                context->client_formats[i].nBlockAlign==4 &&
                context->client_formats[i].nAvgBytesPerSec==context->client_formats[i].nSamplesPerSec*4)
			{
				const UINT rc = context->SelectFormat(context, WINPR_ASSERTING_INT_CAST(UINT16, i));
				if (rc != CHANNEL_RC_OK)
					WLog_WARN(TAG, "SelectFormat failed with %" PRIu32, rc);
                else {
                    fprintf(stderr,"SurfaceDesk AUDIO_FORMAT rate=%lu avg_bytes=%lu align=%u channels=%u bits=%u version=%u\n",(unsigned long)context->client_formats[i].nSamplesPerSec,(unsigned long)context->client_formats[i].nAvgBytesPerSec,context->client_formats[i].nBlockAlign,context->client_formats[i].nChannels,context->client_formats[i].wBitsPerSample,context->clientVersion);fflush(stderr);
                    InterlockedExchange(&client->surfaceAudioReady,1);
                    fprintf(stderr,"SurfaceDesk AUDIO_CHANNEL state=ready rate=%lu channels=%u bits=%u\n",
                        (unsigned long)context->client_formats[i].nSamplesPerSec,
                        context->client_formats[i].nChannels,context->client_formats[i].wBitsPerSample);
                    fflush(stderr);
                }
				return;
			}
		}
	}

	WLog_ERR(TAG, "Could not agree on a audio format with the server\n");
    fprintf(stderr,"SurfaceDesk AUDIO_CHANNEL state=unsupported\n");fflush(stderr);
}

int shadow_client_rdpsnd_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	RdpsndServerContext* rdpsnd = client->rdpsnd = rdpsnd_server_context_new(client->vcm);

	if (!rdpsnd)
	{
		return 0;
	}

	rdpsnd->data = client;
    client->surfaceAudioTiming=surface_audio_timing_new();
    if(!client->surfaceAudioTiming)return -1;
    rdpsnd->ConfirmBlock=surface_audio_confirm;
    rdpsnd->latency=10;

	if (client->subsystem->rdpsndFormats)
	{
		rdpsnd->num_server_formats = client->subsystem->nRdpsndFormats;
        /* The rdpsnd context frees its format array. The subsystem's PCM
         * descriptor is shared/static and must never be freed by a client. */
        rdpsnd->server_formats = audio_formats_new(rdpsnd->num_server_formats);
        if (!rdpsnd->server_formats) return -1;
        memcpy(rdpsnd->server_formats,client->subsystem->rdpsndFormats,
               rdpsnd->num_server_formats*sizeof(AUDIO_FORMAT));
	}
	else
	{
		rdpsnd->num_server_formats = server_rdpsnd_get_formats(&rdpsnd->server_formats);
	}

	if (rdpsnd->num_server_formats > 0)
		rdpsnd->src_format = &rdpsnd->server_formats[0];

	rdpsnd->Activated = rdpsnd_activated;

	const UINT error = rdpsnd->Initialize(rdpsnd, TRUE);
	if (error != CHANNEL_RC_OK)
		return -1;
	return 1;
}

void shadow_client_rdpsnd_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	if (client->rdpsnd)
	{
        InterlockedExchange(&client->surfaceAudioReady,0);
		client->rdpsnd->Stop(client->rdpsnd);
        InterlockedExchange(&client->surfaceAudioReady,0);
		rdpsnd_server_context_free(client->rdpsnd);
		client->rdpsnd = nullptr;
        surface_audio_timing_free(client->surfaceAudioTiming);client->surfaceAudioTiming=NULL;
	}
}
