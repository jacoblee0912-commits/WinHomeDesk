using System;
using System.Collections.Generic;
using System.Globalization;

// Parse only our numeric diagnostic protocol; never credentials or general engine output.
public sealed class PerformanceMetrics {
    public string CaptureText = "캡처 계측: 서버 시작 후 약 2초부터 표시됩니다.";
    public string SendText = "전송 계측: 원격 PC 연결 후 표시됩니다.";
    public DateTime CaptureAt = DateTime.MinValue, SendAt = DateTime.MinValue, PipelineAt=DateTime.MinValue;
    public bool SendClosed;
    public string PipelineText="최신 화면 우선 처리 · 대기 화면은 최대 1장입니다.\n프레임 교체 횟수는 누적 지연을 줄이기 위해 생략한 화면 수입니다.";
    static string F(double n) { return n.ToString("0.0",CultureInfo.InvariantCulture); }
    static bool Read(Dictionary<string,string> fields,string key,out double n) {
        string value;n=0;
        return fields.TryGetValue(key,out value) && Double.TryParse(value,NumberStyles.Float,
            CultureInfo.InvariantCulture,out n) && !Double.IsNaN(n) && !Double.IsInfinity(n) && n>=0;
    }
    public bool Accept(string line) {
        bool capture=line.StartsWith("SurfaceDesk PERF_CAPTURE ",StringComparison.Ordinal);
        bool send=line.StartsWith("SurfaceDesk PERF_SEND ",StringComparison.Ordinal);
        bool pipeline=line.StartsWith("SurfaceDesk PERF_PIPELINE ",StringComparison.Ordinal);
        if(!capture && !send && !pipeline)return false;
        var fields=new Dictionary<string,string>(StringComparer.Ordinal);
        foreach(string token in line.Split(' ')) {
            int at=token.IndexOf('=');if(at>0)fields[token.Substring(0,at)]=token.Substring(at+1);
        }
        if(pipeline) {
            double age,maxAge,replaced,wait,limit,taken;
            if(!Read(fields,"age_ms",out age)||!Read(fields,"age_max_ms",out maxAge)||
               !Read(fields,"replaced",out replaced)||!Read(fields,"delivery_wait_ms",out wait)||
               !Read(fields,"pending_limit",out limit)||!Read(fields,"taken",out taken)||limit!=1)return false;
            PipelineText="최신 화면 우선 · 교체 "+replaced+"장 / 대기 최대 1장 · 처리 시작 시 화면 나이 "+F(age)+" ms (최대 "+F(maxAge)+")\n"+
                "전달 단계 대기 "+F(wait)+" ms · 캡처는 독립 실행 / 이 시간은 원격 화면 표시 지연이 아닙니다.";
            if(taken==0)PipelineText="최신 화면 우선 · 이 구간에 새로 받은 화면 없음 / 대기 최대 1장\n전달 단계 대기 "+F(wait)+" ms";
            PipelineAt=DateTime.UtcNow;
            return true;
        }
        string[] keys=capture ? new[]{"window_s","target","frames","fps","changed_fps","capture_ms","copy_ms","wait_ms","work_max_ms","late","errors"} :
            new[]{"window_s","frames","fps","encode_ms","send_ms","send_max_ms","frame_max_ms","payload_bytes","payload_mbps","area_pct","failed","other"};
        var v=new Dictionary<string,double>();
        foreach(string key in keys) {double n;if(!Read(fields,key,out n))return false;v[key]=n;}
        if(v["window_s"]<=0)return false;
        if(capture) {
            string backend;
            fields.TryGetValue("backend",out backend);
            string caption=backend=="dxgi"?"DirectX 새 프레임 ":backend=="gdi"?"GDI 캡처 ":"캡처 ";
            CaptureText=caption+F(v["fps"])+" FPS / 목표 "+F(v["target"])+"  |  화면 변경 "+F(v["changed_fps"])+" FPS  |  구간 "+F(v["window_s"])+"초\n"+
                "평균 캡처·축소 "+F(v["capture_ms"])+" ms / 비교·복사 "+F(v["copy_ms"])+" ms / 처리 대기 "+F(v["wait_ms"])+" ms\n"+
                "최대 작업 "+F(v["work_max_ms"])+" ms  |  캡처 일정 초과 "+v["late"]+"회 / 오류 "+v["errors"]+"회";
            string mode;double replaced;
            if(fields.TryGetValue("pipeline",out mode)&&mode=="latest"&&Read(fields,"replaced",out replaced)) {
                CaptureText=caption+F(v["fps"])+" FPS / 목표 "+F(v["target"])+" · 독립 캡처 / 구간 "+F(v["window_s"])+"초\n"+
                    "평균 캡처·축소 "+F(v["capture_ms"])+" ms / 최신 화면 제출 "+F(v["copy_ms"])+" ms / 대기 화면 교체 "+replaced+"장\n"+
                    "최대 캡처 작업 "+F(v["work_max_ms"])+" ms · 일정 초과 "+v["late"]+"회 / 오류 "+v["errors"]+"회";
            }
            CaptureAt=DateTime.UtcNow;
        } else {
            string state;if(!fields.TryGetValue("state",out state) || (state!="active" && state!="closed"))return false;
            SendClosed=state=="closed";
            string prefix=SendClosed?"연결 종료 · 마지막 구간: ":"서버 전송: ";
            if(v["other"]>0)SendText=prefix+"지원 계측 외 전송 방식 감지 — 이 구간의 전송 FPS·시간은 계측 범위 밖입니다.";
            else SendText=prefix+F(v["fps"])+" FPS  |  평균 압축·준비 "+F(v["encode_ms"])+" ms / 전송 호출 "+F(v["send_ms"])+" ms\n"+
                "최대 압축·전송 "+F(v["frame_max_ms"])+" ms / 전송 호출 "+F(v["send_max_ms"])+" ms  |  실패 "+v["failed"]+"회\n"+
                "압축 영상 데이터 "+F(v["payload_mbps"])+" Mbps  |  평균 갱신 영역 "+F(v["area_pct"])+"%  |  구간 "+F(v["window_s"])+"초";
            SendAt=DateTime.UtcNow;
        }
        return true;
    }
}
