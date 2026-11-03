// Tests run in a separate window station, NEVER in the user's clipboard.
#include "FreeRDP/server/shadow/shadow_clipboard.cpp"
#include <cassert>
#include <functional>
#include <thread>
using namespace whdclip;
static std::atomic<unsigned> lists{0};
static std::vector<BYTE> returned,remoteText,remoteDescriptors;
static UINT16 flag=0;static UINT64 lastOffset=0;
static UINT listOut(CliprdrServerContext*,const CLIPRDR_FORMAT_LIST*){++lists;return 0;}
static UINT ackOut(CliprdrServerContext*,const CLIPRDR_FORMAT_LIST_RESPONSE*){return 0;}
static UINT dataOut(CliprdrServerContext*,const CLIPRDR_FORMAT_DATA_RESPONSE* r){returned.clear();if(r->common.dataLen)returned.assign(r->requestedFormatData,r->requestedFormatData+r->common.dataLen);flag=r->common.msgFlags;return 0;}
static UINT fileOut(CliprdrServerContext*,const CLIPRDR_FILE_CONTENTS_RESPONSE* r){returned.clear();if(r->cbRequested)returned.assign(r->requestedData,r->requestedData+r->cbRequested);flag=r->common.msgFlags;return 0;}
static UINT dataIn(CliprdrServerContext* c,const CLIPRDR_FORMAT_DATA_REQUEST* q){auto& bytes=q->requestedFormatId==CF_UNICODETEXT?remoteText:remoteDescriptors;CLIPRDR_FORMAT_DATA_RESPONSE r{};r.common.msgFlags=CB_RESPONSE_OK;r.common.dataLen=(UINT32)bytes.size();r.requestedFormatData=bytes.data();return onDataResponse(c,&r);}
static UINT fileIn(CliprdrServerContext* c,const CLIPRDR_FILE_CONTENTS_REQUEST* q){lastOffset=((UINT64)q->nPositionHigh<<32)|q->nPositionLow;std::vector<BYTE> bytes(q->cbRequested);for(size_t i=0;i<bytes.size();++i)bytes[i]=(BYTE)((lastOffset+i)%251);CLIPRDR_FILE_CONTENTS_RESPONSE r{};r.common.msgFlags=CB_RESPONSE_OK;r.streamId=q->streamId;r.cbRequested=(UINT32)bytes.size();r.requestedData=bytes.data();return onFileResponse(c,&r);}
static bool until(const std::function<bool()>& f){for(int i=0;i<300;++i){if(f())return true;Sleep(10);}return false;}
static std::vector<BYTE> unicode(const wchar_t* t){auto p=(const BYTE*)t;return {p,p+(wcslen(t)+1)*2};}
int wmain(int argc,wchar_t** argv){
 assert(argc==2);std::wstring root=argv[1];
 wchar_t name[80];swprintf_s(name,L"WHDClipboardTest-%lu",GetCurrentProcessId());
 HWINSTA station=CreateWindowStationW(name,0,WINSTA_ACCESSCLIPBOARD|WINSTA_CREATEDESKTOP|WINSTA_READATTRIBUTES|WINSTA_WRITEATTRIBUTES|WINSTA_ENUMDESKTOPS|WINSTA_ACCESSGLOBALATOMS,nullptr);bool isolated=station&&SetProcessWindowStation(station);if(!isolated)printf("SKIP OS clipboard: isolated window station unavailable (%lu); user clipboard untouched\n",GetLastError());
 HDESK desktop=isolated?CreateDesktopW(L"ClipboardTest",nullptr,nullptr,0,GENERIC_ALL,nullptr):nullptr;if(isolated)assert(desktop&&SetThreadDesktop(desktop));assert(SUCCEEDED(OleInitialize(nullptr)));
 auto s=std::make_shared<State>();Bridge bridge{s};CliprdrServerContext c{};c.custom=&bridge;c.streamFileClipEnabled=TRUE;
 c.ServerFormatList=listOut;c.ServerFormatListResponse=ackOut;c.ServerFormatDataRequest=dataIn;c.ServerFormatDataResponse=dataOut;c.ServerFileContentsRequest=fileIn;c.ServerFileContentsResponse=fileOut;s->channel=&c;s->active=true;
 if(isolated){s->thread=CreateThread(nullptr,0,clipboardThread,&bridge.state,0,nullptr);assert(s->thread&&WaitForSingleObject(s->ready,5000)==WAIT_OBJECT_0&&s->window);}
 HWND owner=isolated?CreateWindowExW(0,L"STATIC",L"",0,0,0,1,1,HWND_MESSAGE,nullptr,nullptr,nullptr):nullptr;if(isolated)assert(owner);
 auto allocation=unicode(L"visible");allocation.resize(128,0x5a);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,allocation.size());assert(memory);void* raw=GlobalLock(memory);memcpy(raw,allocation.data(),allocation.size());GlobalUnlock(memory);assert(readText(memory)==unicode(L"visible"));GlobalFree(memory);puts("PASS clipboard allocation slack is never transmitted");
 auto text=unicode(L"\ud55c\uae00 \ubcf5\uc0ac\r\nEnglish \U0001F600");STGMEDIUM medium{};
 if(isolated){assert(OpenClipboard(owner)&&EmptyClipboard());assert(SUCCEEDED(globalMedium(text.data(),text.size(),&medium)));assert(SetClipboardData(CF_UNICODETEXT,medium.u.hGlobal)&&CloseClipboard());assert(until([]{return lists.load()>0;}));}else{s->local->text=text;}
 CLIPRDR_FORMAT_DATA_REQUEST data{};data.requestedFormatId=CF_UNICODETEXT;onDataRequest(&c,&data);assert(flag==CB_RESPONSE_OK&&returned.size()>=text.size()&&!memcmp(returned.data(),text.data(),text.size()));puts("PASS outgoing Unicode protocol response");
 CreateDirectoryW(root.c_str(),nullptr);CreateDirectoryW((root+L"\\folder").c_str(),nullptr);
 HANDLE f=CreateFileW((root+L"\\folder\\\ud55c\uae00.bin").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(f!=INVALID_HANDLE_VALUE);
 std::vector<BYTE> content(CHUNK+123);for(size_t i=0;i<content.size();++i)content[i]=(BYTE)(i%251);DWORD written=0;assert(WriteFile(f,content.data(),(DWORD)content.size(),&written,nullptr)&&written==content.size());CloseHandle(f);
 f=CreateFileW((root+L"\\folder\\empty.txt").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(f!=INVALID_HANDLE_VALUE);CloseHandle(f);
 auto snap=std::make_shared<Snapshot>();assert(addFile(*snap,root+L"\\folder",L"folder",0));assert(snap->files.size()==3);remoteDescriptors=descriptors(*snap);assert(validDescriptors(remoteDescriptors));
 UINT index=0;for(UINT i=0;i<snap->files.size();++i)if(wcsstr(snap->files[i]->descriptor.cFileName,L".bin"))index=i;
 {std::lock_guard<std::mutex> lock(s->data);s->local=snap;}
 CLIPRDR_LOCK_CLIPBOARD_DATA hold{};hold.clipDataId=52;onLock(&c,&hold);{std::lock_guard<std::mutex> lock(s->data);s->local=std::make_shared<Snapshot>();}
 CLIPRDR_FILE_CONTENTS_REQUEST q{};q.haveClipDataId=TRUE;q.clipDataId=52;q.listIndex=index;q.streamId=41;q.dwFlags=FILECONTENTS_RANGE;q.cbRequested=123;q.nPositionLow=CHUNK;
 onFileRequest(&c,&q);assert(flag==CB_RESPONSE_OK&&returned.size()==123&&!memcmp(returned.data(),content.data()+CHUNK,123));q.dwFlags=FILECONTENTS_SIZE;onFileRequest(&c,&q);UINT64 size=0;memcpy(&size,returned.data(),8);assert(size==content.size());
 q.dwFlags=FILECONTENTS_RANGE;q.cbRequested=CHUNK+1;onFileRequest(&c,&q);assert(flag==CB_RESPONSE_FAIL);CLIPRDR_UNLOCK_CLIPBOARD_DATA unlock{};unlock.clipDataId=52;onUnlock(&c,&unlock);onFileRequest(&c,&q);assert(flag==CB_RESPONSE_FAIL);
 puts("PASS nested folder, Unicode/empty files, locked snapshots, file sizes, bounded chunks");
 remoteText=unicode(L"\uc0c1\ub300 PC -> \uc11c\ubc84 \ubd99\uc5ec\ub123\uae30 \U0001F600");CLIPRDR_FORMAT formats[1]={{CF_UNICODETEXT,nullptr}};CLIPRDR_FORMAT_LIST list{};list.numFormats=1;list.formats=formats;if(isolated){onList(&c,&list);Sleep(150);}
 IDataObject* object=nullptr;if(isolated){assert(SUCCEEDED(OleGetClipboard(&object)));}else{Formats f{};f.text=CF_UNICODETEXT;f.generation=s->generation;object=new RemoteData(s,f);}FORMATETC fmt{CF_UNICODETEXT,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};medium={};HRESULT hr=object->GetData(&fmt,&medium);printf("OLE GetData hr=%08lx\n",(unsigned long)hr);assert(SUCCEEDED(hr));
 void* ptr=GlobalLock(medium.u.hGlobal);assert(ptr&&!memcmp(ptr,remoteText.data(),remoteText.size()));GlobalUnlock(medium.u.hGlobal);ReleaseStgMedium(&medium);object->Release();puts("PASS incoming Unicode IDataObject paste");
 Formats offer{};offer.files=s->descriptor;offer.generation=s->generation;object=new RemoteData(s,offer);fmt={(CLIPFORMAT)s->descriptor,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};assert(SUCCEEDED(object->GetData(&fmt,&medium)));ReleaseStgMedium(&medium);
 fmt={(CLIPFORMAT)s->contents,nullptr,DVASPECT_CONTENT,(LONG)index,TYMED_ISTREAM};assert(SUCCEEDED(object->GetData(&fmt,&medium)));std::vector<BYTE> received(content.size());ULONG got=0;assert(medium.u.pstm->Read(received.data(),(ULONG)received.size(),&got)==S_OK&&got==received.size()&&received==content);BYTE extra=0;assert(medium.u.pstm->Read(&extra,1,&got)==S_FALSE&&got==0);ReleaseStgMedium(&medium);object->Release();
 auto* large=new RemoteStream(s,s->generation,0,UINT64(6)*1024*1024*1024);LARGE_INTEGER offset{};offset.QuadPart=UINT64(5)*1024*1024*1024;assert(SUCCEEDED(large->Seek(offset,STREAM_SEEK_SET,nullptr)));assert(large->Read(&extra,1,&got)==S_OK&&lastOffset==(UINT64)offset.QuadPart);large->Release();puts("PASS multi-chunk virtual file paste, EOF, 64-bit offsets (synthetic stream)");
 for(const wchar_t* unsafe:{L"..\\escape",L"C:\\escape",L"\\\\host\\share",L"dir\\..\\escape",L"CON.txt",L"x:stream",L"dir\\",L"x.\\y"})assert(!safeName(unsafe,wcslen(unsafe)+1));
 auto bad=remoteDescriptors;FILEDESCRIPTORW d{};memcpy(&d,bad.data()+4,sizeof(d));wcscpy_s(d.cFileName,L"..\\escape");memcpy(bad.data()+4,&d,sizeof(d));assert(!validDescriptors(bad));
 ++s->generation;std::vector<BYTE> result;assert(!fetch(s,offer.generation,CF_UNICODETEXT,0,0,0,result));s->active=false;SetEvent(s->stopped);if(isolated){PostMessageW(s->window,WM_CLOSE,0,0);assert(WaitForSingleObject(s->thread,5000)==WAIT_OBJECT_0);CloseHandle(s->thread);}s->channel=nullptr;DestroyWindow(owner);OleUninitialize();puts("PASS unsafe paths and stale generation rejection");
 // Exercise the real STA listener lifecycle without ever enabling clipboard sync.
 auto life=std::make_shared<State>();life->thread=CreateThread(nullptr,0,clipboardThread,&life,0,nullptr);
 assert(life->thread&&WaitForSingleObject(life->ready,5000)==WAIT_OBJECT_0&&life->window);
 PostMessageW(life->window,WM_CLOSE,0,0);assert(WaitForSingleObject(life->thread,5000)==WAIT_OBJECT_0);CloseHandle(life->thread);
 puts("PASS real hidden clipboard listener startup/shutdown (sync disabled)");
 auto cancel=std::make_shared<State>();Bridge cancelBridge{cancel};CliprdrServerContext blocked{};blocked.custom=&cancelBridge;
 blocked.ServerFormatDataRequest=[](CliprdrServerContext*,const CLIPRDR_FORMAT_DATA_REQUEST*)->UINT{return 0;};cancel->channel=&blocked;cancel->active=true;
 bool fetched=true;std::thread waiting([&]{std::vector<BYTE> bytes;fetched=fetch(cancel,0,CF_UNICODETEXT,0,0,0,bytes);});
 assert(until([&]{std::lock_guard<std::mutex> lock(cancel->data);return cancel->pending==1;}));
 auto start=GetTickCount64();SetEvent(cancel->stopped);waiting.join();assert(!fetched&&GetTickCount64()-start<2000);
 puts("PASS disconnect cancels an in-flight paste without waiting for the timeout");return 0;
}
