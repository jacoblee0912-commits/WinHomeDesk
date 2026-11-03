// Regression tests for inbound clipboard timeouts and OLE format publication.
// No OS clipboard reads/writes.
#include "FreeRDP/server/shadow/shadow_clipboard.cpp"
#include <cassert>
using namespace whdclip;
static unsigned calls=0;
static std::vector<BYTE> textBytes={'O',0,'K',0,0,0},fileBytes;
static UINT respond(CliprdrServerContext* c,const CLIPRDR_FORMAT_DATA_REQUEST* q){++calls;auto& b=q->requestedFormatId==CF_UNICODETEXT?textBytes:fileBytes;CLIPRDR_FORMAT_DATA_RESPONSE r{};r.common.msgFlags=CB_RESPONSE_OK;r.common.dataLen=(UINT32)b.size();r.requestedFormatData=b.data();return onDataResponse(c,&r);}
static UINT blocked(CliprdrServerContext*,const CLIPRDR_FORMAT_DATA_REQUEST*){++calls;return 0;}
static UINT blockedFile(CliprdrServerContext*,const CLIPRDR_FILE_CONTENTS_REQUEST*){return 0;}
int main(){
 auto s=std::make_shared<State>();Bridge bridge{s};CliprdrServerContext c{};s->channel=&c;c.custom=&bridge;c.ServerFormatDataRequest=respond;c.ServerFileContentsRequest=blockedFile;s->active=true;s->responseTimeout=15;
 FILEDESCRIPTORW fd{};fd.dwFlags=FD_FILESIZE|FD_ATTRIBUTES;fd.nFileSizeLow=1;wcscpy_s(fd.cFileName,L"test.txt");fileBytes.resize(4+sizeof(fd));UINT32 count=1;memcpy(fileBytes.data(),&count,4);memcpy(fileBytes.data()+4,&fd,sizeof(fd));
 Formats f{};f.text=CF_UNICODETEXT;f.files=s->descriptor;std::vector<BYTE> text,files;
 assert(prepare(s,f,text,files)&&calls==2);auto* object=new RemoteData(s,f,text,files);
 c.ServerFormatDataRequest=blocked;unsigned before=calls;STGMEDIUM medium{};
 FORMATETC fmt{CF_UNICODETEXT,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};assert(SUCCEEDED(object->GetData(&fmt,&medium)));ReleaseStgMedium(&medium);
 fmt={(CLIPFORMAT)s->descriptor,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};assert(SUCCEEDED(object->GetData(&fmt,&medium)));ReleaseStgMedium(&medium);assert(calls==before);
 fmt={(CLIPFORMAT)s->contents,nullptr,DVASPECT_CONTENT,-1,TYMED_ISTREAM};assert(object->QueryGetData(&fmt)==S_OK);assert(object->GetData(&fmt,&medium)==DV_E_LINDEX);object->Release();
 puts("PASS received text and file descriptors render without network waits; Explorer -1 format probe accepted");
 c.ServerFormatDataRequest=respond;std::vector<BYTE> result;
 assert(!fetch(s,0,0,0,0,1,result)&&!s->poisoned);assert(fetch(s,0,CF_UNICODETEXT,0,0,0,result)&&result==textBytes);
 puts("PASS file timeout does not disable subsequent text paste");
 c.ServerFormatDataRequest=blocked;assert(!fetch(s,0,CF_UNICODETEXT,0,0,0,result)&&s->poisoned);
 c.ServerFormatDataRequest=respond;before=calls;assert(!fetch(s,0,CF_UNICODETEXT,0,0,0,result)&&calls==before);
 CLIPRDR_FORMAT_DATA_RESPONSE late{};late.common.msgFlags=CB_RESPONSE_OK;late.common.dataLen=4;BYTE old[4]={9,9,9,9};late.requestedFormatData=old;onDataResponse(&c,&late);
 assert(!s->poisoned&&fetch(s,0,CF_UNICODETEXT,0,0,0,result)&&result==textBytes);
 puts("PASS late format reply drained before allowing the next request; stale bytes not consumed");
 Window window{s};s->pendingOffer=1;auto gen=s->generation.load();advertise(window);assert(s->generation==gen);s->pendingOffer=0;window.applying=true;advertise(window);assert(s->generation==gen);
 puts("PASS incoming offer/apply guards suppress local clipboard echo");
 s->channel=nullptr;return 0;
}
