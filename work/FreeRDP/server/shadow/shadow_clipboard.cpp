// WinHomeDesk RDP clipboard bridge. Apache-2.0.
// Clipboard/OLE work runs on a dedicated STA. Channel callbacks never call OLE.
#include <freerdp/config.h>
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <freerdp/server/shadow.h>
#include <freerdp/server/cliprdr.h>
#include <freerdp/channels/cliprdr.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdio>

namespace whdclip {
constexpr UINT APPLY = WM_APP + 71;
constexpr size_t MAX_TEXT = 16 * 1024 * 1024, MAX_FILES = 4096, CHUNK = 1024 * 1024;
struct File {
    FILEDESCRIPTORW descriptor{};
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
struct Snapshot { std::vector<BYTE> text; std::vector<std::shared_ptr<File>> files; };
struct FileLease;
struct Formats { UINT32 text=0, files=0; UINT64 generation=0; DWORD sequence=0; std::shared_ptr<FileLease> lease; };
struct State {
    CliprdrServerContext* channel=nullptr;
    std::mutex send, data;
    std::atomic<bool> requesting{false};
    UINT32 leaseSerial=0;
    HANDLE stopped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HANDLE reply=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HWND window=nullptr;
    HANDLE thread=nullptr;
    std::atomic<bool> active{false}, poisoned{false};
    std::atomic<UINT64> generation{0}, pendingOffer{0};
    DWORD responseTimeout=15000;
    UINT descriptor=RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
    UINT contents=RegisterClipboardFormatW(CFSTR_FILECONTENTS);
    UINT effect=RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
    UINT32 serial=0, expectedStream=0;
    int pending=0;
    bool responseOK=false;
    std::vector<BYTE> response;
    std::shared_ptr<Snapshot> local=std::make_shared<Snapshot>();
    std::map<UINT32,std::shared_ptr<Snapshot>> locked;
    ~State(){CloseHandle(stopped);CloseHandle(reply);CloseHandle(ready);}
};
struct Bridge { std::shared_ptr<State> state; };
static State& state(CliprdrServerContext* c){return *static_cast<Bridge*>(c->custom)->state;}
static void log(const char* status) { fprintf(stderr,"WinHomeDesk CLIPBOARD %s\n",status);fflush(stderr); }

// Keep the peer's file selection alive from descriptor retrieval until the
// OLE data object and all streams have been released. No lock on legacy peers.
struct FileLease {
    std::shared_ptr<State> s; UINT32 id=0;
    explicit FileLease(std::shared_ptr<State> state):s(state){}
    ~FileLease(){
        std::lock_guard<std::mutex> guard(s->send);
        if(id&&s->channel&&s->active){CLIPRDR_UNLOCK_CLIPBOARD_DATA q{};q.common.msgType=CB_UNLOCK_CLIPDATA;q.clipDataId=id;
            UINT rc=s->channel->ServerUnlockClipboardData(s->channel,&q);
            fprintf(stderr,"WinHomeDesk CLIPBOARD file-unlock id=%u send=%u\n",id,rc);fflush(stderr);}
    }
};
static bool lockFiles(const std::shared_ptr<State>& s,Formats& f){
    if(f.lease)return true;
    auto lease=std::make_shared<FileLease>(s);
    {std::lock_guard<std::mutex> guard(s->send);
     if(!s->channel||!s->active||f.generation!=s->generation)return false;
     if(!s->channel->canLockClipData)return true;
     if(!s->channel->ServerLockClipboardData||!s->channel->ServerUnlockClipboardData)return false;
     UINT32 id=++s->leaseSerial;if(!id)id=++s->leaseSerial;
     CLIPRDR_LOCK_CLIPBOARD_DATA q{};q.common.msgType=CB_LOCK_CLIPDATA;q.clipDataId=id;
     UINT rc=s->channel->ServerLockClipboardData(s->channel,&q);
     fprintf(stderr,"WinHomeDesk CLIPBOARD file-lock id=%u send=%u\n",id,rc);fflush(stderr);
     if(rc)return false;lease->id=id;}
    f.lease=std::move(lease);return true;
}
static DWORD waitReply(const std::shared_ptr<State>& s){
    HANDLE handles[]={s->stopped,s->reply};DWORD index=0;
    // Read is called through Explorer's OLE proxy in an STA. Keep COM calls
    // serviceable during the wait; never dispatch arbitrary application messages.
    HRESULT hr=CoWaitForMultipleHandles(0,s->responseTimeout,2,handles,&index);
    if(hr==S_OK)return WAIT_OBJECT_0+index;
    if(hr==RPC_S_CALLPENDING)return WAIT_TIMEOUT;
    if(hr==CO_E_NOTINITIALIZED)return WaitForMultipleObjects(2,handles,FALSE,s->responseTimeout);
    fprintf(stderr,"WinHomeDesk CLIPBOARD wait-failed hr=0x%08lx\n",(unsigned long)hr);fflush(stderr);return WAIT_FAILED;
}
struct RequestGuard {
    std::atomic<bool>& busy; bool acquired;
    explicit RequestGuard(std::atomic<bool>& value):busy(value),acquired(!value.exchange(true)){}
    ~RequestGuard(){if(acquired)busy=false;}
};

// File names received over the wire are relative names, never paths to local files.
static bool safeName(const wchar_t* raw, size_t max) {
    size_t n=0;while(n<max && raw[n])++n;
    if(!n || n==max || raw[0]==L'\\' || raw[0]==L'/')return false;
    std::wstring name(raw,n);size_t pos=0;
    while(pos<name.size()) {
        size_t end=name.find(L'\\',pos);if(end==std::wstring::npos)end=name.size();
        std::wstring part=name.substr(pos,end-pos);
        if(part.empty()||part==L"."||part==L".."||part.back()==L'.'||part.back()==L' ')return false;
        for(wchar_t ch:part)if(ch<32 || wcschr(L"/:*?\"<>|",ch))return false;
        std::wstring base=part.substr(0,part.find(L'.'));for(auto& ch:base)ch=towupper(ch);
        if(base==L"CON"||base==L"PRN"||base==L"AUX"||base==L"NUL"||
           (base.size()==4&&(base.substr(0,3)==L"COM"||base.substr(0,3)==L"LPT")&&base[3]>=L'0'&&base[3]<=L'9'))return false;
        if(end==name.size())return true;
        pos=end+1;if(pos==name.size())return false;
    }
    return false;
}
// Copy only the terminated text, never unused bytes in a clipboard allocation.
static std::vector<BYTE> readText(HGLOBAL handle) {
    std::vector<BYTE> result;size_t size=handle?GlobalSize(handle):0;
    if(size<2||size>MAX_TEXT)return result;
    auto* text=static_cast<const wchar_t*>(GlobalLock(handle));if(!text)return result;
    size_t count=0;while(count<size/2&&text[count])++count;
    if(count*2+2<=MAX_TEXT){result.resize(count*2+2,0);memcpy(result.data(),text,count*2);}
    GlobalUnlock(handle);return result;
}
static bool addFile(Snapshot& snap,const std::wstring& path,const std::wstring& relative,unsigned depth,HANDLE stop=nullptr) {
    if(stop&&WaitForSingleObject(stop,0)==WAIT_OBJECT_0)return false;
    if(depth>32||snap.files.size()>=MAX_FILES||!safeName(relative.c_str(),relative.size()+1)||relative.size()>=MAX_PATH)return false;
    auto f=std::make_shared<File>();
    f->handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(f->handle==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(f->handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
    auto& d=f->descriptor;d.dwFlags=FD_ATTRIBUTES|FD_FILESIZE|FD_WRITESTIME|FD_UNICODE;
    d.dwFileAttributes=info.dwFileAttributes;d.nFileSizeHigh=info.nFileSizeHigh;d.nFileSizeLow=info.nFileSizeLow;
    d.ftLastWriteTime=info.ftLastWriteTime;wcscpy_s(d.cFileName,relative.c_str());snap.files.push_back(f);
    if(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATAW entry{};HANDLE find=FindFirstFileW((path+L"\\*").c_str(),&entry);
        if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
        bool ok=true;
        do { if(!wcscmp(entry.cFileName,L".")||!wcscmp(entry.cFileName,L".."))continue;
            if(!addFile(snap,path+L"\\"+entry.cFileName,relative+L"\\"+entry.cFileName,depth+1,stop)){ok=false;break;}
        }while(FindNextFileW(find,&entry));
        if(ok && GetLastError()!=ERROR_NO_MORE_FILES)ok=false;
        FindClose(find);return ok;
    }
    return true;
}
static std::vector<BYTE> descriptors(const Snapshot& snap) {
    std::vector<BYTE> bytes(4+snap.files.size()*sizeof(FILEDESCRIPTORW));
    UINT32 count=(UINT32)snap.files.size();memcpy(bytes.data(),&count,4);
    for(size_t i=0;i<snap.files.size();++i)memcpy(bytes.data()+4+i*sizeof(FILEDESCRIPTORW),&snap.files[i]->descriptor,sizeof(FILEDESCRIPTORW));
    return bytes;
}
static bool validDescriptors(const std::vector<BYTE>& bytes) {
    if(bytes.size()<4)return false;UINT32 count=0;memcpy(&count,bytes.data(),4);
    if(!count||count>MAX_FILES||bytes.size()!=4+count*sizeof(FILEDESCRIPTORW))return false;
    std::set<std::wstring> names;
    for(UINT32 i=0;i<count;++i) {
        FILEDESCRIPTORW d{};memcpy(&d,bytes.data()+4+i*sizeof(d),sizeof(d));
        if(!safeName(d.cFileName,MAX_PATH)||((d.dwFlags&FD_ATTRIBUTES)&&(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)))return false;
        std::wstring name=d.cFileName;for(auto& ch:name)ch=towlower(ch);
        if(!names.insert(name).second)return false;
    }
    return true;
}
static bool fetch(const std::shared_ptr<State>& s,UINT64 generation,UINT format,UINT index,
                  UINT64 offset,UINT amount,std::vector<BYTE>& result,UINT fileFlags=FILECONTENTS_RANGE,const std::shared_ptr<FileLease>& lease={}) {
    RequestGuard single(s->requesting);
    if(!single.acquired){log("request-busy; paste again");return false;}
    if((format&&s->poisoned)||!s->active||generation!=s->generation){log("request-rejected stale/disconnected or waiting for late format reply");return false;}
    ResetEvent(s->reply);
    {std::lock_guard<std::mutex> lock(s->data);s->pending=format?1:2;s->expectedStream=++s->serial;s->response.clear();s->responseOK=false;}
    UINT rc=ERROR_OPERATION_ABORTED;
    { std::lock_guard<std::mutex> lock(s->send);
      if(s->channel && WaitForSingleObject(s->stopped,0)!=WAIT_OBJECT_0) {
        if(format){CLIPRDR_FORMAT_DATA_REQUEST q{};q.common.msgType=CB_FORMAT_DATA_REQUEST;q.requestedFormatId=format;rc=s->channel->ServerFormatDataRequest(s->channel,&q);}
        else {CLIPRDR_FILE_CONTENTS_REQUEST q{};q.common.msgType=CB_FILECONTENTS_REQUEST;q.streamId=s->expectedStream;q.listIndex=index;
            q.dwFlags=fileFlags;q.nPositionLow=(UINT32)offset;q.nPositionHigh=(UINT32)(offset>>32);q.cbRequested=amount;q.haveClipDataId=lease&&lease->id;q.clipDataId=lease?lease->id:0;
            rc=s->channel->ServerFileContentsRequest(s->channel,&q);}
      }
    }
    if(format) {fprintf(stderr,"WinHomeDesk CLIPBOARD request format=%u send=%u\n",format,rc);fflush(stderr);}
    else if(offset==0||rc!=0){fprintf(stderr,"WinHomeDesk CLIPBOARD file-request index=%u kind=%s stream=%u offset=%llu bytes=%u send=%u\n",index,fileFlags==FILECONTENTS_SIZE?"size":"range",s->expectedStream,(unsigned long long)offset,amount,rc);fflush(stderr);}
    DWORD wait=rc==0?waitReply(s):WAIT_FAILED;
    std::lock_guard<std::mutex> lock(s->data);s->pending=0;
    // A reply can arrive exactly as the timeout expires, before this lock.
    if(wait==WAIT_TIMEOUT&&WaitForSingleObject(s->reply,0)==WAIT_OBJECT_0)wait=WAIT_OBJECT_0+1;
    if(wait==WAIT_TIMEOUT){if(format)s->poisoned=true;log(format?"format-timeout; waiting for late response, reconnect if it persists":"file-timeout; other clipboard requests remain enabled");}
    if(wait!=WAIT_OBJECT_0+1||!s->responseOK||generation!=s->generation) {fprintf(stderr,"WinHomeDesk CLIPBOARD request-failed kind=%s wait=%lu response=%u stale=%u\n",format?"format":"file",(unsigned long)wait,s->responseOK?1u:0u,generation!=s->generation?1u:0u);fflush(stderr);return false;}
    result.swap(s->response);if(format){fprintf(stderr,"WinHomeDesk CLIPBOARD received format=%u bytes=%zu\n",format,result.size());fflush(stderr);}return true;
}
class RemoteStream final:public IStream {
    std::atomic<ULONG> refs{1};std::shared_ptr<State> s;UINT64 generation,pos=0,size;UINT index;std::shared_ptr<FileLease> lease;
public:
    RemoteStream(std::shared_ptr<State> state,UINT64 gen,UINT ix,UINT64 length,std::shared_ptr<FileLease> lock={}):s(state),generation(gen),size(length),index(ix),lease(std::move(lock)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_ISequentialStream||iid==IID_IStream){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Read(void* out,ULONG count,ULONG* got) override {
        if(got)*got=0;if(count&&!out)return STG_E_INVALIDPOINTER;ULONG total=0;
        while(total<count&&pos<size) {
            UINT want=(UINT)std::min<UINT64>(std::min<size_t>(count-total,CHUNK),size-pos);std::vector<BYTE> bytes;
            if(!fetch(s,generation,0,index,pos,want,bytes,FILECONTENTS_RANGE,lease)||bytes.empty()||bytes.size()>want){fprintf(stderr,"WinHomeDesk CLIPBOARD file-read-failed index=%u offset=%llu requested=%u received=%zu\n",index,(unsigned long long)pos,want,bytes.size());fflush(stderr);return STG_E_READFAULT;}
            memcpy(static_cast<BYTE*>(out)+total,bytes.data(),bytes.size());pos+=bytes.size();total+=(ULONG)bytes.size();if(got)*got=total;
        }
        return total==count?S_OK:S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move,DWORD origin,ULARGE_INTEGER* result) override {
        UINT64 base=origin==STREAM_SEEK_SET?0:origin==STREAM_SEEK_CUR?pos:origin==STREAM_SEEK_END?size:UINT64_MAX;
        if(base==UINT64_MAX||(move.QuadPart<0 && UINT64(-(move.QuadPart+1))+1>base)||
           (move.QuadPart>=0 && UINT64(move.QuadPart)>UINT64_MAX-base))return STG_E_INVALIDFUNCTION;
        pos=base+move.QuadPart;if(result)result->QuadPart=pos;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* stat,DWORD) override{if(!stat)return E_POINTER;*stat={};stat->type=STGTY_STREAM;stat->cbSize.QuadPart=size;stat->grfMode=STGM_READ;return S_OK;}
    HRESULT STDMETHODCALLTYPE Clone(IStream** out) override {if(!out)return E_POINTER;auto* other=new RemoteStream(s,generation,index,size,lease);other->pos=pos;*out=other;return S_OK;}
    HRESULT STDMETHODCALLTYPE CopyTo(IStream* dest,ULARGE_INTEGER count,ULARGE_INTEGER* read,ULARGE_INTEGER* written) override {
        if(read)read->QuadPart=0;if(written)written->QuadPart=0;if(!dest)return E_POINTER;
        std::vector<BYTE> buffer(CHUNK);UINT64 total=0;
        while(total<count.QuadPart) {ULONG got=0,put=0;HRESULT hr=Read(buffer.data(),(ULONG)std::min<UINT64>(CHUNK,count.QuadPart-total),&got);
            if(FAILED(hr))return hr;if(!got)return S_FALSE;hr=dest->Write(buffer.data(),got,&put);if(FAILED(hr))return hr;
            total+=got;if(read)read->QuadPart=total;if(written)written->QuadPart+=put;if(put!=got)return STG_E_WRITEFAULT;
        }return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Write(const void*,ULONG,ULONG*)override{return STG_E_ACCESSDENIED;}
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER)override{return STG_E_ACCESSDENIED;}
    HRESULT STDMETHODCALLTYPE Commit(DWORD)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE Revert()override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD)override{return STG_E_INVALIDFUNCTION;}
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD)override{return STG_E_INVALIDFUNCTION;}
};
static HRESULT globalMedium(const void* bytes,size_t length,STGMEDIUM* medium) {
    HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,length?length:1);if(!h)return E_OUTOFMEMORY;
    void* ptr=GlobalLock(h);if(!ptr){GlobalFree(h);return E_OUTOFMEMORY;}if(length)memcpy(ptr,bytes,length);GlobalUnlock(h);
    medium->tymed=TYMED_HGLOBAL;medium->u.hGlobal=h;medium->pUnkForRelease=nullptr;return S_OK;
}
class RemoteData final:public IDataObject {
    std::atomic<ULONG> refs{1};std::shared_ptr<State> s;Formats formats;std::vector<BYTE> fileList,textData;
public:
    RemoteData(std::shared_ptr<State> state,Formats f,std::vector<BYTE> text={},std::vector<BYTE> files={}):s(state),formats(f),fileList(std::move(files)),textData(std::move(text)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override {if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IDataObject){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f)override {
        if(!f)return E_POINTER;if(f->dwAspect!=DVASPECT_CONTENT)return DV_E_DVASPECT;
        if(f->cfFormat==s->contents&&formats.files)return(f->tymed&TYMED_ISTREAM)&&f->lindex>=-1?S_OK:DV_E_FORMATETC;
        if((f->cfFormat==CF_UNICODETEXT&&formats.text)||(formats.files&&(f->cfFormat==s->descriptor||f->cfFormat==s->effect)))return(f->tymed&TYMED_HGLOBAL)?S_OK:DV_E_TYMED;
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f,STGMEDIUM* medium)override {
        if(!medium)return E_POINTER;*medium={};HRESULT hr=QueryGetData(f);if(FAILED(hr))return hr;
        if(!s->active||formats.generation!=s->generation)return DV_E_FORMATETC;
        if(f->cfFormat==CF_UNICODETEXT){if(textData.empty()&&!fetch(s,formats.generation,formats.text,0,0,0,textData))return E_FAIL;
            if(textData.size()<2||textData.size()%2||textData[textData.size()-1]||textData[textData.size()-2])return E_FAIL;return globalMedium(textData.data(),textData.size(),medium);}
        if(f->cfFormat==s->effect){DWORD effect=DROPEFFECT_COPY;return globalMedium(&effect,4,medium);}
        if(fileList.empty()){if(!fetch(s,formats.generation,formats.files,0,0,0,fileList)||!validDescriptors(fileList)){fileList.clear();return E_FAIL;}}
        if(f->cfFormat==s->descriptor)return globalMedium(fileList.data(),fileList.size(),medium);
        UINT32 count=0;memcpy(&count,fileList.data(),4);if(f->lindex<0||(UINT32)f->lindex>=count)return DV_E_LINDEX;
        FILEDESCRIPTORW d{};memcpy(&d,fileList.data()+4+f->lindex*sizeof(d),sizeof(d));
        const bool directory=(d.dwFlags&FD_ATTRIBUTES)&&(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
        UINT64 size=directory?0:((UINT64)d.nFileSizeHigh<<32)|d.nFileSizeLow;
        // FILEDESCRIPTOR fields are optional. A missing FD_FILESIZE requires
        // a protocol size query, not rejection of a valid Explorer file.
        if(!directory&&!(d.dwFlags&FD_FILESIZE)){
            std::vector<BYTE> bytes;
            if(!fetch(s,formats.generation,0,(UINT)f->lindex,0,8,bytes,FILECONTENTS_SIZE,formats.lease)||bytes.size()!=8){log("file-size unavailable or malformed");return STG_E_READFAULT;}
            memcpy(&size,bytes.data(),8);
        }
        fprintf(stderr,"WinHomeDesk CLIPBOARD file-open index=%ld flags=0x%08lx size=%llu directory=%u\n",(long)f->lindex,(unsigned long)d.dwFlags,(unsigned long long)size,directory?1u:0u);fflush(stderr);
        medium->tymed=TYMED_ISTREAM;medium->u.pstm=new RemoteStream(s,formats.generation,(UINT)f->lindex,size,formats.lease);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,IEnumFORMATETC** out)override {
        if(!out)return E_POINTER;*out=nullptr;if(direction!=DATADIR_GET)return E_NOTIMPL;
        FORMATETC list[4]{};UINT n=0;
        auto add=[&](UINT format,DWORD medium,LONG index){list[n++]={(CLIPFORMAT)format,nullptr,DVASPECT_CONTENT,index,medium};};
        if(formats.text)add(CF_UNICODETEXT,TYMED_HGLOBAL,-1);
        if(formats.files){add(s->descriptor,TYMED_HGLOBAL,-1);add(s->contents,TYMED_ISTREAM,-1);add(s->effect,TYMED_HGLOBAL,-1);}
        return SHCreateStdEnumFmtEtc(n,list,out);
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*,STGMEDIUM*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,FORMATETC* out)override{if(out)out->ptd=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*,STGMEDIUM*,BOOL)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*,DWORD,IAdviseSink*,DWORD*)override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD)override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**)override{return OLE_E_ADVISENOTSUPPORTED;}
};
// Resolve small format data BEFORE publishing a delayed-rendering OLE object.
// A paste callback must not be the first place that discovers a missing response.
static bool prepare(const std::shared_ptr<State>& s,Formats& f,std::vector<BYTE>& text,std::vector<BYTE>& files) {
    bool offered=f.text||f.files;
    if(f.text&&(!fetch(s,f.generation,f.text,0,0,0,text)||text.size()<2||text.size()%2||text[text.size()-1]||text[text.size()-2])){f.text=0;text.clear();}
    if(f.files&&(!lockFiles(s,f)||!fetch(s,f.generation,f.files,0,0,0,files)||!validDescriptors(files))){f.files=0;files.clear();f.lease.reset();}
    return !offered||f.text||f.files;
}
struct Window { std::shared_ptr<State> s;IDataObject* remote=nullptr;DWORD sequence=0;bool applying=false; };
static void advertise(Window& w) {
    auto s=w.s;if(!s->active||s->pendingOffer.load()!=0||w.applying)return;
    if(w.remote&&OleIsCurrentClipboard(w.remote)==S_OK)return;
    DWORD sequence=GetClipboardSequenceNumber();if(sequence==w.sequence)return;
    if(!OpenClipboard(s->window)){SetTimer(s->window,1,100,nullptr);return;}
    auto snapshot=std::make_shared<Snapshot>();std::vector<std::wstring> paths;
    if(IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        snapshot->text=readText(GetClipboardData(CF_UNICODETEXT));
    }
    if(IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP drop=(HDROP)GetClipboardData(CF_HDROP);UINT count=drop?DragQueryFileW(drop,0xFFFFFFFF,nullptr,0):0;
        if(count<=MAX_FILES)for(UINT i=0;i<count;++i){UINT n=DragQueryFileW(drop,i,nullptr,0);if(n==0||n>32760)continue;std::vector<wchar_t> path(n+1);DragQueryFileW(drop,i,path.data(),n+1);paths.emplace_back(path.data());}
    }
    CloseClipboard();
    for(const auto& path:paths){size_t last=path.find_last_of(L"\\/");if(!addFile(*snapshot,path,path.substr(last==std::wstring::npos?0:last+1),0,s->stopped)){snapshot->files.clear();log("file snapshot unavailable (limit, access or reparse point)");break;}}
    if(!snapshot->files.empty()&&!validDescriptors(descriptors(*snapshot))){snapshot->files.clear();log("file list has conflicting names");}
    if(sequence!=GetClipboardSequenceNumber()){SetTimer(s->window,1,100,nullptr);return;}
    ++s->generation;w.sequence=sequence;
    if(w.remote){w.remote->Release();w.remote=nullptr;}
    {std::lock_guard<std::mutex> lock(s->data);s->local=snapshot;}
    CLIPRDR_FORMAT formats[3]{};UINT n=0;
    if(!snapshot->text.empty())formats[n++].formatId=CF_UNICODETEXT;
    std::lock_guard<std::mutex> lock(s->send);if(!s->channel)return;
    if(!snapshot->files.empty()&&s->channel->streamFileClipEnabled){formats[n++]={s->descriptor,const_cast<char*>("FileGroupDescriptorW")};formats[n++]={s->contents,const_cast<char*>("FileContents")};}
    fprintf(stderr,"WinHomeDesk CLIPBOARD local text=%u files=%zu\n",snapshot->text.empty()?0u:1u,snapshot->files.size());fflush(stderr);
    CLIPRDR_FORMAT_LIST list{};list.common.msgType=CB_FORMAT_LIST;list.numFormats=n;list.formats=formats;(void)s->channel->ServerFormatList(s->channel,&list);
}
static LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* w=(Window*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE){w=(Window*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)w);}
    if(!w)return DefWindowProcW(hwnd,msg,wp,lp);
    if(msg==WM_CLIPBOARDUPDATE||msg==WM_TIMER){KillTimer(hwnd,1);advertise(*w);return 0;}
    if(msg==APPLY){
        std::unique_ptr<Formats> f((Formats*)lp);auto s=w->s;
        if(!s->active||f->generation!=s->generation)return 0;
        std::vector<BYTE> text,files;bool prepared=prepare(s,*f,text,files);
        UINT64 expected=f->generation;s->pendingOffer.compare_exchange_strong(expected,0);
        if(!s->active||f->generation!=s->generation)return 0;
        if(GetClipboardSequenceNumber()!=f->sequence){log("local copy changed while receiving; keeping newer clipboard");advertise(*w);return 0;}
        if(!prepared){log("incoming offer unavailable; clipboard not replaced");return 0;}
        auto* object=new RemoteData(s,*f,std::move(text),std::move(files));
        w->applying=true;HRESULT hr=OleSetClipboard(object);w->applying=false;
        if(SUCCEEDED(hr)){if(w->remote)w->remote->Release();w->remote=object;w->sequence=GetClipboardSequenceNumber();log("incoming clipboard published (format data ready)");}
        else{object->Release();fprintf(stderr,"WinHomeDesk CLIPBOARD publish-failed hr=0x%08lx\n",(unsigned long)hr);fflush(stderr);}
        return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(hwnd);return 0;}
    if(msg==WM_DESTROY){RemoveClipboardFormatListener(hwnd);PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static DWORD WINAPI clipboardThread(void* arg) {
    auto s=*static_cast<std::shared_ptr<State>*>(arg);Window w{s};
    HRESULT hr=OleInitialize(nullptr);if(FAILED(hr)){SetEvent(s->ready);return 1;}
    WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"WinHomeDeskClipboard037";
    RegisterClassW(&cls);s->window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,&w);
    w.sequence=GetClipboardSequenceNumber();
    if(s->window&&!AddClipboardFormatListener(s->window)){DestroyWindow(s->window);s->window=nullptr;}SetEvent(s->ready);
    MSG msg{};if(s->window&&WaitForSingleObject(s->stopped,0)==WAIT_OBJECT_0)DestroyWindow(s->window);
    if(s->window)while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    // Do not flush remote files on disconnect; pending streams fail through stopped.
    if(w.remote){if(OleIsCurrentClipboard(w.remote)==S_OK)OleSetClipboard(nullptr);w.remote->Release();}
    while(PeekMessageW(&msg,nullptr,APPLY,APPLY,PM_REMOVE))delete (Formats*)msg.lParam;
    OleUninitialize();return 0;
}
static UINT onCapabilities(CliprdrServerContext* c,const CLIPRDR_CAPABILITIES*) {
    auto& s=state(c);s.active=true;fprintf(stderr,"WinHomeDesk CLIPBOARD ready text=Unicode files=streamed-copy lock=%u\n",c->canLockClipData?1u:0u);fflush(stderr);return 0;
}
static UINT onList(CliprdrServerContext* c,const CLIPRDR_FORMAT_LIST* list) {
    auto& s=state(c);auto f=std::make_unique<Formats>();
    for(UINT32 i=0;i<list->numFormats;++i){auto& x=list->formats[i];if(x.formatId==CF_UNICODETEXT)f->text=x.formatId;
        if(c->streamFileClipEnabled&&x.formatName&&!strcmp(x.formatName,"FileGroupDescriptorW"))f->files=x.formatId;}
    fprintf(stderr,"WinHomeDesk CLIPBOARD remote text=%u files=%u\n",f->text?1u:0u,f->files?1u:0u);fflush(stderr);
    f->generation=++s.generation;f->sequence=GetClipboardSequenceNumber();s.pendingOffer=f->generation;
    {std::lock_guard<std::mutex> lock(s.data);s.local=std::make_shared<Snapshot>();}
    CLIPRDR_FORMAT_LIST_RESPONSE response{};response.common.msgType=CB_FORMAT_LIST_RESPONSE;response.common.msgFlags=CB_RESPONSE_OK;
    {std::lock_guard<std::mutex> lock(s.send);(void)c->ServerFormatListResponse(c,&response);}
    if(PostMessageW(s.window,APPLY,0,(LPARAM)f.get()))f.release();else{s.pendingOffer=0;log("incoming clipboard queue failed");}return 0;
}
static UINT onDataRequest(CliprdrServerContext* c,const CLIPRDR_FORMAT_DATA_REQUEST* request) {
    auto& s=state(c);std::vector<BYTE> bytes;
    {std::lock_guard<std::mutex> lock(s.data);if(request->requestedFormatId==CF_UNICODETEXT)bytes=s.local->text;
        else if(request->requestedFormatId==s.descriptor&&!s.local->files.empty())bytes=descriptors(*s.local);}
    CLIPRDR_FORMAT_DATA_RESPONSE response{};response.common.msgType=CB_FORMAT_DATA_RESPONSE;response.common.msgFlags=bytes.empty()?CB_RESPONSE_FAIL:CB_RESPONSE_OK;
    response.common.dataLen=(UINT32)bytes.size();response.requestedFormatData=bytes.data();
    std::lock_guard<std::mutex> lock(s.send);return c->ServerFormatDataResponse(c,&response);
}
static UINT onDataResponse(CliprdrServerContext* c,const CLIPRDR_FORMAT_DATA_RESPONSE* response) {
    auto& s=state(c);std::lock_guard<std::mutex> lock(s.data);
    if(s.pending!=1){if(s.poisoned.exchange(false))log("late format reply drained; copy again");return 0;}s.responseOK=(response->common.msgFlags&CB_RESPONSE_OK)&&response->common.dataLen<=MAX_TEXT;
    if(s.responseOK&&response->common.dataLen)s.response.assign(response->requestedFormatData,response->requestedFormatData+response->common.dataLen);
    SetEvent(s.reply);return 0;
}
static UINT onFileRequest(CliprdrServerContext* c,const CLIPRDR_FILE_CONTENTS_REQUEST* request) {
    auto& s=state(c);std::shared_ptr<Snapshot> snap;
    {std::lock_guard<std::mutex> lock(s.data);if(request->haveClipDataId){auto it=s.locked.find(request->clipDataId);if(it!=s.locked.end())snap=it->second;}else snap=s.local;}
    std::vector<BYTE> bytes;bool ok=false;
    if(snap&&request->listIndex<snap->files.size()) {
        auto f=snap->files[request->listIndex];UINT64 size=((UINT64)f->descriptor.nFileSizeHigh<<32)|f->descriptor.nFileSizeLow;
        if(request->dwFlags==FILECONTENTS_SIZE){bytes.resize(8);memcpy(bytes.data(),&size,8);ok=true;}
        else if(request->dwFlags==FILECONTENTS_RANGE&&request->cbRequested<=CHUNK&&!(f->descriptor.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
            UINT64 offset=((UINT64)request->nPositionHigh<<32)|request->nPositionLow;
            if(offset<=size&&offset<=INT64_MAX){LARGE_INTEGER pos{};pos.QuadPart=(LONGLONG)offset;DWORD got=0;
                bytes.resize((size_t)std::min<UINT64>(size-offset,request->cbRequested));
                ok=bytes.empty()||(SetFilePointerEx(f->handle,pos,nullptr,FILE_BEGIN)&&ReadFile(f->handle,bytes.data(),(DWORD)bytes.size(),&got,nullptr));bytes.resize(got);}
        }
    }
    CLIPRDR_FILE_CONTENTS_RESPONSE response{};response.common.msgType=CB_FILECONTENTS_RESPONSE;response.common.msgFlags=ok?CB_RESPONSE_OK:CB_RESPONSE_FAIL;
    response.streamId=request->streamId;response.cbRequested=(UINT32)bytes.size();response.requestedData=bytes.data();
    std::lock_guard<std::mutex> lock(s.send);return c->ServerFileContentsResponse(c,&response);
}
static UINT onFileResponse(CliprdrServerContext* c,const CLIPRDR_FILE_CONTENTS_RESPONSE* response) {
    auto& s=state(c);std::lock_guard<std::mutex> lock(s.data);if(s.pending!=2||s.expectedStream!=response->streamId){fprintf(stderr,"WinHomeDesk CLIPBOARD file-response-unmatched stream=%u expected=%u pending=%d\n",response->streamId,s.expectedStream,s.pending);fflush(stderr);return 0;}
    s.responseOK=(response->common.msgFlags&CB_RESPONSE_OK)&&response->cbRequested<=CHUNK;
    if(!s.responseOK){fprintf(stderr,"WinHomeDesk CLIPBOARD file-response-rejected stream=%u flags=0x%04x bytes=%u\n",response->streamId,response->common.msgFlags,response->cbRequested);fflush(stderr);}
    if(s.responseOK&&response->cbRequested)s.response.assign(response->requestedData,response->requestedData+response->cbRequested);
    SetEvent(s.reply);return 0;
}
static UINT onLock(CliprdrServerContext* c,const CLIPRDR_LOCK_CLIPBOARD_DATA* request) {
    auto& s=state(c);std::lock_guard<std::mutex> lock(s.data);if(s.locked.size()<8)s.locked.emplace(request->clipDataId,s.local);return 0;
}
static UINT onUnlock(CliprdrServerContext* c,const CLIPRDR_UNLOCK_CLIPBOARD_DATA* request) {
    auto& s=state(c);std::lock_guard<std::mutex> lock(s.data);s.locked.erase(request->clipDataId);return 0;
}
} // namespace
extern "C" void whd_clipboard_stop(rdpShadowClient* client) {
    using namespace whdclip;auto* bridge=(Bridge*)client->surfaceClipboard;if(!bridge)return;client->surfaceClipboard=nullptr;
    auto s=bridge->state;s->active=false;SetEvent(s->stopped);
    CliprdrServerContext* channel=nullptr;{std::lock_guard<std::mutex> lock(s->send);channel=s->channel;s->channel=nullptr;}
    if(channel){channel->Stop(channel);cliprdr_server_context_free(channel);}
    if(s->thread){if(s->window)PostMessageW(s->window,WM_CLOSE,0,0);WaitForSingleObject(s->thread,INFINITE);CloseHandle(s->thread);}
    delete bridge;
}
extern "C" BOOL whd_clipboard_start(rdpShadowClient* client) {
    using namespace whdclip;if(client->surfaceClipboard)return TRUE;
    const char* enabled=getenv("WINHOME_DESK_CLIPBOARD");
    if(!enabled||strcmp(enabled,"1")||!client->surfaceAppAuthenticated||!client->mayView||!client->mayInteract)return TRUE;
    if(!WTSVirtualChannelManagerIsChannelJoined(client->vcm,CLIPRDR_SVC_CHANNEL_NAME)){log("client-disabled");return TRUE;}
    auto* bridge=new Bridge{std::make_shared<State>()};client->surfaceClipboard=bridge;auto s=bridge->state;
    s->thread=CreateThread(nullptr,0,clipboardThread,&bridge->state,0,nullptr);
    if(!s->stopped||!s->reply||!s->ready||!s->thread||WaitForSingleObject(s->ready,10000)!=WAIT_OBJECT_0||!s->window){whd_clipboard_stop(client);return FALSE;}
    auto* c=cliprdr_server_context_new(client->vcm);if(!c){whd_clipboard_stop(client);return FALSE;}
    s->channel=c;c->custom=bridge;c->rdpcontext=&client->context;c->autoInitializationSequence=TRUE;
    c->useLongFormatNames=TRUE;c->streamFileClipEnabled=TRUE;c->fileClipNoFilePaths=TRUE;c->canLockClipData=TRUE;c->hasHugeFileSupport=TRUE;
    c->ClientCapabilities=onCapabilities;c->ClientFormatList=onList;c->ClientFormatDataRequest=onDataRequest;c->ClientFormatDataResponse=onDataResponse;
    c->ClientFileContentsRequest=onFileRequest;c->ClientFileContentsResponse=onFileResponse;c->ClientLockClipboardData=onLock;c->ClientUnlockClipboardData=onUnlock;
    if(c->Start(c)!=0){whd_clipboard_stop(client);return FALSE;}return TRUE;
}
