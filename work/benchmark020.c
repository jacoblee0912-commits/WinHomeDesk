/* Repeatable moving desktop-like workload; timestamps exclude scene generation. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/color.h>
static double clockms(void) {LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return n.QuadPart*1000.0/f.QuadPart;}
static int compare(const void* a,const void* b){double x=*(double*)a,y=*(double*)b;return (x>y)-(x<y);}
int main(void) {
 const UINT32 width=1440,height=960,stride=width*4,N=90;
 BYTE* src=malloc((size_t)stride*height);H264_CONTEXT* enc=h264_context_new(TRUE);
 if(!src||!enc||!h264_context_reset(enc,width,height)||
 !h264_context_set_option(enc,H264_CONTEXT_OPTION_RATECONTROL,H264_RATECONTROL_VBR)||
 !h264_context_set_option(enc,H264_CONTEXT_OPTION_BITRATE,10000000)||
 !h264_context_set_option(enc,H264_CONTEXT_OPTION_FRAMERATE,30))return 1;
 RECTANGLE_16 full={0,0,width,height};double durations[90],sum=0;UINT64 bytes=0;unsigned encoded=0;
 for(unsigned frame=0;frame<N;frame++) {
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++) {
   unsigned sy=y+frame*9;BYTE* p=src+y*stride+x*4;
   BOOL text=(sy%32<3 && x%280>16 && x%280<235);
   p[0]=text?40:(BYTE)(170+(x/120+sy/80)%5*15);
   p[1]=text?40:(BYTE)(175+(x/80+sy/120)%4*15);
   p[2]=text?40:(BYTE)(180+(x/180+sy/140)%5*12);p[3]=255;
  }
  BYTE* data=NULL;UINT32 length=0;RDPGFX_H264_METABLOCK meta={0};double start=clockms();
  int rc=avc420_compress(enc,src,PIXEL_FORMAT_BGRX32,stride,width,height,&full,&data,&length,&meta);
  durations[frame]=clockms()-start;if(rc<0)return 2;
  sum+=durations[frame];bytes+=length;if(rc>0&&length)encoded++;
  free_h264_metablock(&meta);
 }
 qsort(durations,N,sizeof(double),compare);
 printf("BENCH frames=%u encoded=%u mean_ms=%.3f p50_ms=%.3f p95_ms=%.3f max_ms=%.3f bytes=%llu\n",N,encoded,sum/N,durations[N/2],durations[85],durations[89],(unsigned long long)bytes);
 h264_context_free(enc);free(src);return encoded>=60?0:3;
}
