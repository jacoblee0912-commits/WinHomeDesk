// Real cliprdr DLL wire-format test. No clipboard access or network connections.
#include "FreeRDP/server/shadow/shadow_clipboard.cpp"
#include <cassert>
#include <deque>
using namespace whdclip;
static HANDLE eventHandle;
static std::vector<BYTE> sent;
static std::deque<std::vector<BYTE>> packets;
static UINT32 u32(const BYTE* p){UINT32 v;memcpy(&v,p,4);return v;}
static void append32(std::vector<BYTE>& p,UINT32 v){auto* b=(BYTE*)&v;p.insert(p.end(),b,b+4);}
static BOOL WINAPI session(HANDLE,DWORD,WTS_INFO_CLASS,LPSTR* out,DWORD* count){*out=(LPSTR)malloc(4);**(DWORD**)out=1;*count=4;return TRUE;}
static HANDLE WINAPI openChannel(DWORD,LPSTR,DWORD){return (HANDLE)1;}
static BOOL WINAPI closeChannel(HANDLE){return TRUE;}
static BOOL WINAPI query(HANDLE,WTS_VIRTUAL_CLASS,PVOID* out,DWORD* count){*out=malloc(sizeof(HANDLE));**(HANDLE**)out=eventHandle;*count=sizeof(HANDLE);return TRUE;}
static BOOL WINAPI writeChannel(HANDLE,PCHAR p,ULONG n,PULONG written){sent.assign((BYTE*)p,(BYTE*)p+n);*written=n;return TRUE;}
static BOOL WINAPI readChannel(HANDLE,ULONG,PCHAR p,ULONG n,PULONG read){*read=0;if(packets.empty())return TRUE;auto bytes=packets.front();packets.pop_front();assert(bytes.size()<=n);memcpy(p,bytes.data(),bytes.size());*read=(ULONG)bytes.size();return TRUE;}
static void WINAPI freeMem(void* p){free(p);}
static void receive(CliprdrServerContext* c,UINT16 type,const std::vector<BYTE>& bytes){
 std::vector<BYTE> packet;packet.push_back((BYTE)type);packet.push_back((BYTE)(type>>8));packet.push_back(CB_RESPONSE_OK);packet.push_back(0);append32(packet,(UINT32)bytes.size());packet.insert(packet.end(),bytes.begin(),bytes.end());
 for(size_t offset=0;offset<packet.size();offset+=1600){UINT32 n=(UINT32)std::min<size_t>(1600,packet.size()-offset);std::vector<BYTE> frame;append32(frame,n);append32(frame,(offset==0?1:0)|(offset+n==packet.size()?2:0));frame.insert(frame.end(),packet.begin()+offset,packet.begin()+offset+n);packets.push_back(frame);assert(c->CheckEventHandle(c)==0);}
}
int main(){
 eventHandle=CreateEventW(nullptr,TRUE,FALSE,nullptr);WtsApiFunctionTable table{};table.pQuerySessionInformationA=session;table.pVirtualChannelOpenEx=openChannel;table.pVirtualChannelClose=closeChannel;table.pVirtualChannelQuery=query;table.pVirtualChannelWrite=writeChannel;table.pVirtualChannelRead=readChannel;table.pFreeMemory=freeMem;assert(WTSRegisterWtsApiFunctionTable(&table));
 auto s=std::make_shared<State>();Bridge bridge{s};auto* c=cliprdr_server_context_new((HANDLE)1);assert(c);c->custom=&bridge;c->ClientFormatDataResponse=onDataResponse;c->ClientFileContentsResponse=onFileResponse;assert(c->Open(c)==0);
 CLIPRDR_FORMAT_DATA_REQUEST req{};req.common.msgType=CB_FORMAT_DATA_REQUEST;req.requestedFormatId=CF_UNICODETEXT;assert(c->ServerFormatDataRequest(c,&req)==0);
 printf("format request wire: bytes=%zu type=%u payload=%u format=%u\n",sent.size(),sent[0],u32(sent.data()+4),sent.size()>=12?u32(sent.data()+8):999);
 assert(sent.size()==12&&sent[0]==CB_FORMAT_DATA_REQUEST&&u32(sent.data()+4)==4&&u32(sent.data()+8)==13);
 s->pending=1;std::vector<BYTE> text={'h',0,'i',0,0,0};receive(c,CB_FORMAT_DATA_RESPONSE,text);assert(s->responseOK&&s->response==text);
 s->pending=1;std::vector<BYTE> descriptor(8000,0x5a);receive(c,CB_FORMAT_DATA_RESPONSE,descriptor);assert(s->responseOK&&s->response==descriptor);
 CLIPRDR_FILE_CONTENTS_REQUEST file{};file.common.msgType=CB_FILECONTENTS_REQUEST;file.streamId=77;file.dwFlags=FILECONTENTS_RANGE;file.cbRequested=2000;assert(c->ServerFileContentsRequest(c,&file)==0);assert(sent.size()==32&&u32(sent.data()+4)==24);
 s->pending=2;s->expectedStream=77;std::vector<BYTE> fileBytes;append32(fileBytes,77);fileBytes.resize(2004,0x42);receive(c,CB_FILECONTENTS_RESPONSE,fileBytes);assert(s->responseOK&&s->response.size()==2000);
 assert(c->Close(c)==0);cliprdr_server_context_free(c);CloseHandle(eventHandle);puts("PASS real DLL format/file request wire headers and fragmented incoming responses");
}
