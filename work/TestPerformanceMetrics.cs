using System;
using System.Globalization;
using System.IO;
class TestPerformanceMetrics {
    static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
    static void Main(string[] args) {
        CultureInfo.CurrentCulture=new CultureInfo("de-DE");
        var p=new PerformanceMetrics();
        string capture="SurfaceDesk PERF_CAPTURE window_s=2.1 target=45 frames=42 fps=20.00 changed_fps=0.00 capture_ms=5.5 copy_ms=1.2 wait_ms=3.3 work_max_ms=60 late=2 errors=0";
        string send="SurfaceDesk PERF_SEND state=active window_s=2.1 frames=20 fps=9.52 encode_ms=8.5 send_ms=2.5 send_max_ms=30.2 frame_max_ms=41.2 payload_bytes=100000 payload_mbps=0.381 area_pct=45.2 failed=0 other=0";
        Check(p.Accept(capture)&&p.CaptureText.Contains("5.5 ms"),"invariant parsing");
        Check(p.Accept(send)&&p.SendText.Contains("9.5 FPS")&&!p.SendClosed,"send metrics");
        string last=p.CaptureText;
        Check(!p.Accept(capture.Replace("fps=20.00","fps=NaN"))&&p.CaptureText==last,"NaN rejected without state mutation");
        Check(!p.Accept(capture.Replace("fps=20.00","fps=-1")),"negative rejected");
        Check(!p.Accept("SurfaceDesk PERF_SEND state=active"),"truncated rejected");
        Check(!p.Accept(send.Replace("state=active","state=unknown")),"unknown state rejected");
        Check(p.Accept(send.Replace("other=0","other=1"))&&p.SendText.Contains("범위 밖"),"other codec not false zero");
        Check(p.Accept(send.Replace("state=active","state=closed"))&&p.SendClosed&&p.SendText.Contains("연결 종료"),"disconnect");
        Check(!p.Accept("SurfaceDesk authentication: accepted"),"non-metric ignored");
        Check(p.Accept(capture+" backend=dxgi pipeline=latest replaced=12") && p.CaptureText.Contains("독립 캡처") && !p.CaptureText.Contains("화면 변경"),"new capture must not report invented changed FPS");
        string pipe="SurfaceDesk PERF_PIPELINE window_s=2.0 taken=40 changed=30 replaced=20 snapshot_ms=1.2 age_ms=9.0 age_max_ms=29.5 delivery_wait_ms=49.0 pending_limit=1";
        Check(p.Accept(pipe)&&p.PipelineText.Contains("교체 20장")&&p.PipelineText.Contains("9.0 ms"),"pipeline diagnostics");
        string saved=p.PipelineText;Check(!p.Accept(pipe.Replace("age_ms=9.0","age_ms=NaN"))&&p.PipelineText==saved,"invalid pipeline sample rejected");
        if(args.Length>0) {
            int n=0;foreach(string line in File.ReadAllLines(args[0]))
                if(line.StartsWith("SurfaceDesk PERF_")){Check(p.Accept(line),"real engine output parsing: "+line);n++;}
            Check(n>=4,"real telemetry samples");
        }
        Console.WriteLine("Performance metrics parser: passed");
    }
}
