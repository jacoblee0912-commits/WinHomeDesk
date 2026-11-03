// Lock lifetime, negotiated legacy fallback, and STA COM wait regression.
// Does not access the OS clipboard, network or user's input devices.
#include "FreeRDP/server/shadow/shadow_clipboard.cpp"
#include <cassert>
#include <thread>
using namespace whdclip;
static unsigned locks=0,unlocks=0;static UINT32 lockedId=0;static std::vector<BYTE> descriptorBytes;
static UINT lockPeer(CliprdrServerContext*,const CLIPRDR_LOCK_CLIPBOARD_DATA* q){++locks;lockedId=q->clipDataId;return 0;}
static UINT unlockPeer(CliprdrServerContext*,const CLIPRDR_UNLOCK_CLIPBOARD_DATA* q){++unlocks;assert(q->clipDataId==lockedId);return 0;}
static UINT metadata(CliprdrServerContext* c,const CLIPRDR_FORMAT_DATA_REQUEST*){if(c->canLockClipData)assert(locks==1&&unlocks==0);CLIPRDR_FORMAT_DATA_RESPONSE r{};r.common.msgFlags=CB_RESPONSE_OK;r.common.dataLen=(UINT32)descriptorBytes.size();r.requestedFormatData=descriptorBytes.data();return onDataResponse(c,&r);}
static UINT contents(CliprdrServerContext* c,const CLIPRDR_FILE_CONTENTS_REQUEST* q){assert(bool(q->haveClipDataId)==bool(c->canLockClipData));if(q->haveClipDataId)assert(q->clipDataId==lockedId&&unlocks==0);BYTE bytes[4]={1,2,3,4};CLIPRDR_FILE_CONTENTS_RESPONSE r{};r.common.msgFlags=CB_RESPONSE_OK;r.streamId=q->streamId;r.cbRequested=4;r.requestedData=bytes;return onFileResponse(c,&r);}
// An OLE caller may make a second apartment call while a stream read waits.
// Real COM standard marshaling exercises that path, not a mocked wait function.
class Callback final:public IStream {
 std::atomic<ULONG> refs{1};HANDLE event;
public:
 explicit Callback(HANDLE e):event(e){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{*p=nullptr;if(id!=IID_IUnknown&&id!=IID_IStream&&id!=IID_ISequentialStream)return E_NOINTERFACE;*p=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Stat(STATSTG* p,DWORD)override{*p={};SetEvent(event);return S_OK;}
 HRESULT STDMETHODCALLTYPE Read(void*,ULONG,ULONG*)override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE Write(const void*,ULONG,ULONG*)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER,DWORD,ULARGE_INTEGER*)override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE CopyTo(IStream*,ULARGE_INTEGER,ULARGE_INTEGER*,ULARGE_INTEGER*)override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE Commit(DWORD)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Revert()override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD)override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE Clone(IStream**)override{return E_NOTIMPL;}
};
int main(){
 assert(SUCCEEDED(OleInitialize(nullptr)));auto s=std::make_shared<State>();Bridge b{s};CliprdrServerContext c{};s->channel=&c;c.custom=&b;s->active=true;c.canLockClipData=TRUE;c.ServerLockClipboardData=lockPeer;c.ServerUnlockClipboardData=unlockPeer;c.ServerFormatDataRequest=metadata;c.ServerFileContentsRequest=contents;
 FILEDESCRIPTORW fd{};fd.dwFlags=FD_FILESIZE;fd.nFileSizeLow=4;wcscpy_s(fd.cFileName,L"test.txt");descriptorBytes.resize(4+sizeof(fd));UINT32 count=1;memcpy(descriptorBytes.data(),&count,4);memcpy(descriptorBytes.data()+4,&fd,sizeof(fd));
 Formats f{};f.files=s->descriptor;std::vector<BYTE> text,files;assert(prepare(s,f,text,files)&&locks==1);auto* d=new RemoteData(s,f,text,files);f.lease.reset();FORMATETC fmt{(CLIPFORMAT)s->contents,nullptr,DVASPECT_CONTENT,0,TYMED_ISTREAM};STGMEDIUM m{};assert(d->GetData(&fmt,&m)==S_OK);IStream* clone=nullptr;assert(m.u.pstm->Clone(&clone)==S_OK);d->Release();assert(unlocks==0);BYTE buffer[4];ULONG got=0;assert(m.u.pstm->Read(buffer,4,&got)==S_OK&&got==4&&buffer[3]==4);ReleaseStgMedium(&m);assert(unlocks==0);clone->Release();assert(unlocks==1);puts("PASS lock before descriptor, tagged file read, unlock only after object and cloned streams release");
 c.canLockClipData=FALSE;assert(prepare(s,f,text,files)&&!f.lease&&locks==1);d=new RemoteData(s,f,text,files);assert(d->GetData(&fmt,&m)==S_OK);assert(m.u.pstm->Read(buffer,4,&got)==S_OK);ReleaseStgMedium(&m);d->Release();assert(unlocks==1);puts("PASS legacy peer without locking still receives untagged file requests");
 auto callback=new Callback(s->reply);IStream* marshaled=nullptr;assert(CoMarshalInterThreadInterfaceInStream(IID_IStream,callback,&marshaled)==S_OK);ResetEvent(s->reply);s->responseTimeout=3000;
 HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::thread worker([&]{assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));IStream* proxy=nullptr;assert(CoGetInterfaceAndReleaseStream(marshaled,IID_IStream,(void**)&proxy)==S_OK);STATSTG stat{};assert(proxy->Stat(&stat,STATFLAG_NONAME)==S_OK);proxy->Release();CoUninitialize();SetEvent(done);});
 HANDLE waits[]={s->stopped,s->reply};assert(WaitForMultipleObjects(2,waits,FALSE,50)==WAIT_TIMEOUT);puts("PASS old blocking wait cannot dispatch the pending COM callback");assert(waitReply(s)==WAIT_OBJECT_0+1);DWORD index=0;assert(CoWaitForMultipleHandles(0,3000,1,&done,&index)==S_OK);worker.join();CloseHandle(done);callback->Release();puts("PASS actual cross-apartment COM call completes while clipboard response wait is pending");
 s->responseTimeout=10;assert(waitReply(s)==WAIT_TIMEOUT);SetEvent(s->stopped);assert(waitReply(s)==WAIT_OBJECT_0);puts("PASS COM wait retains bounded timeout and disconnect cancellation");s->channel=nullptr;OleUninitialize();
}
