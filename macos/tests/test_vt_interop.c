/* Decode Apple hardware H.264 with the independent Cisco CPU decoder. Synthetic
 * planar pixels only; checks RDP's coded padding, partial updates and IDRs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wels/codec_api.h>
#include "../../work/FreeRDP/libfreerdp/codec/surface_vt.h"
int main(void) {
    const unsigned sizes[][2]={{800,600},{1920,1080},{1440,812},{2560,1440},{802,602}};
    for(unsigned d=0;d<5;d++) {
        unsigned w=sizes[d][0],h=sizes[d][1]; LONG error=0;
        unsigned cw=(w+15)&~15u,ch=(h+15)&~15u;
        SurfaceVT* encoder=surface_vt_new(w,h,20,10000000,&error);fprintf(stderr,"encoder status=%d\n",error);assert(encoder);
        ISVCDecoder* decoder=NULL;assert(!WelsCreateDecoder(&decoder) && decoder);
        SDecodingParam params={0};params.eEcActiveIdc=ERROR_CON_DISABLE;params.sVideoProperty.eVideoBsType=VIDEO_BITSTREAM_AVC;
        assert(!(*decoder)->Initialize(decoder,&params));
        UINT32 strides[3]={w+32,w/2+16,w/2+24};
        BYTE *planes[3]={calloc(strides[0],h),calloc(strides[1],h/2),calloc(strides[2],h/2)};
        unsigned idrFrames=0;
        for(unsigned frame=0;frame<48;frame++) {
            for(unsigned p=0;p<3;p++) {
                unsigned pw=w/(p?2:1),ph=h/(p?2:1);
                for(unsigned y=0;y<ph;y++) for(unsigned x=0;x<pw;x++)
                    planes[p][y*strides[p]+x]=(BYTE)(32+((x/32+y/32+(frame<4?frame:3)+p*3)%10)*20);
                /* After the initial full-frame changes, move a small square.
                 * Continue past the 40-frame periodic IDR interval. */
                if(frame>=4) for(unsigned y=16;y<48;y++) for(unsigned x=16+(frame%8)*32;x<48+(frame%8)*32;x++)
                    planes[p][y*strides[p]+x]=(BYTE)(80+p*48);
            }
            BYTE* packet=NULL;UINT32 length=0;
            assert(surface_vt_encode(encoder,(const BYTE* const*)planes,strides,&packet,&length,&error)>0);
            BOOL idr=FALSE;
            for(unsigned n=0;n+4<length;n++)
                if(!memcmp(packet+n,"\0\0\0\1",4) && (packet[n+4]&31)==5) idr=TRUE;
            idrFrames+=idr?1:0;
            BYTE *decoded[3]={0};SBufferInfo info={0};
            DECODING_STATE status=(*decoder)->DecodeFrameNoDelay(decoder,packet,length,decoded,&info);
            fprintf(stderr,"decode %ux%u frame=%u status=0x%x output=%d bytes=%u\n",w,h,frame,status,info.iBufferStatus,length);
            assert(status==dsErrorFree && info.iBufferStatus==1);
            assert(info.UsrData.sSystemBuffer.iWidth==cw && info.UsrData.sSystemBuffer.iHeight==ch);
            double errorSum=0;unsigned count=0;
            for(unsigned p=0;p<3;p++) {
                unsigned pw=w/(p?2:1),ph=h/(p?2:1),pcw=cw/(p?2:1),pch=ch/(p?2:1),ds=info.UsrData.sSystemBuffer.iStride[p?1:0];
                for(unsigned y=8;y<pch;y+=16) for(unsigned x=8;x<pcw;x+=16) {
                    errorSum+=abs((int)decoded[p][y*ds+x]-(int)planes[p][MIN(y,ph-1)*strides[p]+MIN(x,pw-1)]);count++;
                }
                /* Include the final padded corner, even for thin padding. */
                errorSum+=abs((int)decoded[p][(pch-1)*ds+pcw-1]-(int)planes[p][(ph-1)*strides[p]+pw-1]);count++;
            }
            assert(errorSum/count<4);
            printf("PASS VT->OpenH264 %ux%u frame=%u mae=%.3f bytes=%u\n",w,h,frame,errorSum/count,length);
        }
        assert(idrFrames>=2);
        printf("PASS VT periodic IDR %ux%u count=%u\n",w,h,idrFrames);
        for(unsigned p=0;p<3;p++) free(planes[p]);
        (*decoder)->Uninitialize(decoder);WelsDestroyDecoder(decoder);surface_vt_free(encoder);
    }
}
