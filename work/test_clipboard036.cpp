// Incoming file interoperability. No access to the user's clipboard.
#include "FreeRDP/server/shadow/shadow_clipboard.cpp"
#include <cassert>
using namespace whdclip;
static UINT64 length=CHUNK+17,lastOffset=0;
static unsigned sizeCalls=0,rangeCalls=0;
static bool malformed=false,reject=false,empty=false;
static UINT respond(CliprdrServerContext* c,const CLIPRDR_FILE_CONTENTS_REQUEST* q){
 std::vector<BYTE> bytes;
 if(q->dwFlags==FILECONTENTS_SIZE){++sizeCalls;assert(q->cbRequested==8&&q->nPositionLow==0&&q->nPositionHigh==0);bytes.resize(malformed?7:8);memcpy(bytes.data(),&length,bytes.size());}
 else {assert(q->dwFlags==FILECONTENTS_RANGE);++rangeCalls;lastOffset=((UINT64)q->nPositionHigh<<32)|q->nPositionLow;assert(lastOffset<=length);bytes.resize(empty?0:(size_t)std::min<UINT64>(q->cbRequested,length-lastOffset));for(size_t i=0;i<bytes.size();++i)bytes[i]=(BYTE)((lastOffset+i)%251);}
 CLIPRDR_FILE_CONTENTS_RESPONSE r{};r.streamId=q->streamId;r.common.msgFlags=reject?CB_RESPONSE_FAIL:CB_RESPONSE_OK;r.cbRequested=(UINT32)bytes.size();r.requestedData=bytes.data();return onFileResponse(c,&r);
}
int main(){
 auto s=std::make_shared<State>();Bridge bridge{s};CliprdrServerContext c{};c.custom=&bridge;c.ServerFileContentsRequest=respond;s->channel=&c;s->active=true;
 auto object=[&](DWORD flags,DWORD attrs,UINT64 known=0){FILEDESCRIPTORW d{};d.dwFlags=flags;d.dwFileAttributes=attrs;d.nFileSizeHigh=(UINT32)(known>>32);d.nFileSizeLow=(UINT32)known;wcscpy_s(d.cFileName,L"test.txt");std::vector<BYTE> list(4+sizeof(d));UINT32 count=1;memcpy(list.data(),&count,4);memcpy(list.data()+4,&d,sizeof(d));assert(validDescriptors(list));Formats f{};f.files=s->descriptor;return new RemoteData(s,f,{},list);};
 FORMATETC f{(CLIPFORMAT)s->contents,nullptr,DVASPECT_CONTENT,0,TYMED_ISTREAM};STGMEDIUM m{};
 // Unspecified attributes must not accidentally turn a file into a directory/reparse point.
 auto* data=object(0,FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT);
 assert(data->GetData(&f,&m)==S_OK&&sizeCalls==1);STATSTG stat{};assert(m.u.pstm->Stat(&stat,STATFLAG_NONAME)==S_OK&&stat.cbSize.QuadPart==length);
 std::vector<BYTE> out((size_t)length);ULONG got=0;assert(m.u.pstm->Read(out.data(),(ULONG)out.size(),&got)==S_OK&&got==length&&rangeCalls==2);for(size_t i=0;i<out.size();++i)assert(out[i]==i%251);
 assert(m.u.pstm->Read(out.data(),1,&got)==S_FALSE&&got==0);ReleaseStgMedium(&m);data->Release();puts("PASS missing descriptor size: query 8-byte size, multi-chunk contents, byte equality, EOF; ignore unspecified attributes");
 length=UINT64(5)*1024*1024*1024+9;data=object(FD_ATTRIBUTES,FILE_ATTRIBUTE_NORMAL);assert(data->GetData(&f,&m)==S_OK);assert(m.u.pstm->Stat(&stat,STATFLAG_NONAME)==S_OK&&stat.cbSize.QuadPart==length);LARGE_INTEGER seek{};seek.QuadPart=(LONGLONG)length-9;assert(m.u.pstm->Seek(seek,STREAM_SEEK_SET,nullptr)==S_OK);assert(m.u.pstm->Read(out.data(),9,&got)==S_OK&&got==9&&lastOffset==length-9);ReleaseStgMedium(&m);data->Release();puts("PASS queried 64-bit size and read beyond 4 GiB");
 length=0;data=object(0,0);assert(data->GetData(&f,&m)==S_OK);unsigned before=rangeCalls;assert(m.u.pstm->Read(out.data(),1,&got)==S_FALSE&&got==0&&rangeCalls==before);ReleaseStgMedium(&m);data->Release();
 before=sizeCalls;data=object(FD_ATTRIBUTES,FILE_ATTRIBUTE_DIRECTORY);assert(data->GetData(&f,&m)==S_OK&&sizeCalls==before);assert(m.u.pstm->Read(out.data(),1,&got)==S_FALSE&&got==0);ReleaseStgMedium(&m);data->Release();
 data=object(FD_FILESIZE,0,4);assert(data->GetData(&f,&m)==S_OK&&sizeCalls==before);ReleaseStgMedium(&m);data->Release();puts("PASS empty file, directory stream and known-size fast path");
 malformed=true;data=object(0,0);assert(data->GetData(&f,&m)==STG_E_READFAULT&&m.tymed==TYMED_NULL);data->Release();malformed=false;reject=true;data=object(0,0);assert(data->GetData(&f,&m)==STG_E_READFAULT);data->Release();reject=false;
 length=4;data=object(FD_FILESIZE,0,4);assert(data->GetData(&f,&m)==S_OK);empty=true;assert(m.u.pstm->Read(out.data(),4,&got)==STG_E_READFAULT&&got==0);empty=false;assert(m.u.pstm->Read(out.data(),4,&got)==S_OK&&got==4);ReleaseStgMedium(&m);data->Release();puts("PASS malformed/refused size and premature EOF fail safely; retry succeeds");s->channel=nullptr;
}
