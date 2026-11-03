using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

// The engine owns physical pixels, DPI, rotation and virtual-desktop coordinates.
// Never derive the capture rectangle from DPI-virtualized WinForms Screen bounds.
public sealed class MonitorReport {
    public int Count, Width, Height;
    public bool CanvasCompatible;
    public bool SelectorCompatible;
    public string[] Devices=new string[0];
    public string Details;
    public static MonitorReport Parse(string text,int exitCode) {
        if(exitCode!=0 && exitCode!=-2001)throw new InvalidDataException("Monitor enumeration failed ("+exitCode+").");
        var matches=Regex.Matches(text,@"(?m)^SurfaceDesk MONITOR_LIST count=(\d+) width=(\d+) height=(\d+)\s*$");
        if(matches.Count!=1)throw new InvalidDataException("This engine did not return a valid MONITOR_LIST. Install the complete multi-monitor engine package.");
        var m=matches[0];int n,w,h;
        if(!Int32.TryParse(m.Groups[1].Value,out n)||!Int32.TryParse(m.Groups[2].Value,out w)||!Int32.TryParse(m.Groups[3].Value,out h)||n<1||n>16||w<1||w>8192||h<1||h>8192)
            throw new InvalidDataException("Invalid monitor count or desktop bounds (maximum 8192 x 8192).");
        bool canvas=Regex.IsMatch(text,@"(?m)^SurfaceDesk MONITOR_CANVAS protocol_monitors=1 physical_monitors="+n+@" origin=0,0\s*$");
        var devices=new List<string>();
        foreach(Match device in Regex.Matches(text,@"(?m)^SurfaceDesk MONITOR index=\d+ device=(DISPLAY\d+) primary=[01] source=-?\d+,-?\d+,-?\d+,-?\d+ stream=\d+,\d+,\d+,\d+\s*$")) {
            if(devices.Contains(device.Groups[1].Value))throw new InvalidDataException("Duplicate monitor device.");
            devices.Add(device.Groups[1].Value);
        }
        bool selector=Regex.IsMatch(text,@"(?m)^SurfaceDesk MONITOR_SELECT version=1\s*$") && devices.Count==n;
        return new MonitorReport {Count=n,Width=w,Height=h,Details=text,CanvasCompatible=canvas,SelectorCompatible=selector,Devices=devices.ToArray()};
    }
}
public static class MonitorSharing {
    public static bool Resolve(string scope,string inherited,bool flag) {
        if(scope=="all" || scope=="separate")return true;
        if(scope=="primary")return false;
        if(scope!="legacy")throw new InvalidDataException("Invalid monitor sharing scope.");
        if(inherited=="1")return true;
        if(inherited=="0")return false;
        return flag;
    }
    public static void Configure(ProcessStartInfo info,bool all,int quality) {
        info.EnvironmentVariables.Remove("WINHOME_DESK_MONITOR_DEVICE");
        info.EnvironmentVariables["SURFACE_DESK_ALL_MONITORS"]=all?"1":"0";
        info.EnvironmentVariables["SURFACE_DESK_MAX_EDGE"]=quality==1?"1440":quality==2?"0":"1920";
    }
    public static MonitorReport Probe(string engine,bool all,int quality,int timeoutMs) {
        var info=new ProcessStartInfo(engine,"/monitors") {UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=Path.GetDirectoryName(engine),RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
        Configure(info,all,quality);
        using(var p=new Process {StartInfo=info}) {
            p.Start();var output=p.StandardOutput.ReadToEndAsync();var errors=p.StandardError.ReadToEndAsync();
            if(!p.WaitForExit(timeoutMs)) {try{p.Kill();p.WaitForExit(2000);}catch(InvalidOperationException){}throw new TimeoutException("Monitor enumeration timed out.");}
            if(!Task.WaitAll(new Task[]{output,errors},2000))throw new TimeoutException("Monitor output timed out.");
            var report=MonitorReport.Parse(output.Result+"\n"+errors.Result,p.ExitCode);
            if(all&&!report.CanvasCompatible)throw new InvalidDataException("This engine does not support the Windows H.264 monitor canvas. Extract the complete Windows 0.3.1 package, including all engine DLLs.");
            return report;
        }
    }
}
public sealed partial class SurfaceDesk {
    readonly ComboBox monitorScope=new ComboBox();
    readonly Label monitorSummary=new Label();
    readonly Button monitorDetails=new Button();
    bool changingMonitorScope,monitorBusy;
    MonitorReport monitorReport;
    string runtimeMonitorDetails="";
    readonly System.Collections.Generic.HashSet<int> runtimeMonitorIds=new System.Collections.Generic.HashSet<int>();
    bool runtimeMonitorsConfirmed;
    void ResetRuntimeMonitors() {runtimeMonitorDetails="";runtimeMonitorIds.Clear();runtimeMonitorsConfirmed=false;}
    void AcceptRuntimeMonitorLine(string line) {
        var device=Regex.Match(line,@"^SurfaceDesk MONITOR index=(\d+) device=\S+ primary=[01] source=-?\d+,-?\d+,-?\d+,-?\d+ stream=\d+,\d+,\d+,\d+$");
        if(device.Success) {
            int id;if(Int32.TryParse(device.Groups[1].Value,out id)&&id>=0&&id<=16&&runtimeMonitorIds.Add(id))runtimeMonitorDetails+=line+Environment.NewLine;
        }
        var size=Regex.Match(line,@"^SurfaceDesk (?:CAPTURE backend=\S+|screen:) source=\d+x\d+ stream=(\d+)x(\d+)");
        if(size.Success && runtimeMonitorIds.Count>0) {
            int width,height;
            if(Int32.TryParse(size.Groups[1].Value,out width)&&Int32.TryParse(size.Groups[2].Value,out height)&&width>0&&width<=8192&&height>0&&height<=8192) {
                monitorReport=new MonitorReport {Count=runtimeMonitorIds.Count,Width=width,Height=height,Details=runtimeMonitorDetails};runtimeMonitorsConfirmed=true;RefreshMonitorText();
            }
        }
    }
    bool SelectedAllMonitors { get {return monitorScope.SelectedIndex>=1;} }
    bool SeparateMonitors { get {return monitorScope.SelectedIndex==2;} }
    void InitializeMonitorSharing() {
        Controls.Add(new Label {Text=L.T("공유 모니터"),AutoSize=true,Location=new Point(22,620)});
        monitorScope.Name="MonitorScope";monitorScope.SetBounds(112,615,365,30);monitorScope.DropDownStyle=ComboBoxStyle.DropDownList;
        monitorScope.Items.AddRange(new object[]{L.T("주 모니터만 공유"),L.T("모든 모니터 공유"),L.T("모니터별 개별 연결 (RDCMan)")});Controls.Add(monitorScope);
        bool all=MonitorSharing.Resolve(preferences.MonitorScope,Environment.GetEnvironmentVariable("SURFACE_DESK_ALL_MONITORS"),File.Exists(Path.Combine(root,"engine","WinHomeDesk-all-monitors.flag")));
        monitorScope.SelectedIndex=preferences.MonitorScope=="separate"?2:all?1:0;
        monitorDetails.Text=L.T("모니터 확인 / RDCMan 안내");monitorDetails.SetBounds(494,615,274,30);Controls.Add(monitorDetails);
        monitorSummary.SetBounds(22,650,746,32);monitorSummary.AutoEllipsis=true;monitorSummary.Font=new Font(Font.FontFamily,9);Controls.Add(monitorSummary);
        captureMetrics.Top=686;sendMetrics.Top=753;pipelineMetrics.Top=822;log.Top=869;log.Height=89;
        monitorScope.SelectedIndexChanged+=delegate {
            if(changingMonitorScope)return;
            string next=SeparateMonitors?"separate":SelectedAllMonitors?"all":"primary";string old=preferences.MonitorScope;
            try {
                // Only persist scope here. Do not change credentials or startup registration.
                if(!testMode){var saved=preferenceStore.Load();saved.MonitorScope=next;preferenceStore.Save(saved);}
                preferences.MonitorScope=next;monitorReport=null;consent.Checked=false;RefreshMonitorText();SetRunning(IsEngineRunning());
            }catch(Exception ex) {
                changingMonitorScope=true;monitorScope.SelectedIndex=old=="separate"?2:MonitorSharing.Resolve(old,Environment.GetEnvironmentVariable("SURFACE_DESK_ALL_MONITORS"),File.Exists(Path.Combine(root,"engine","WinHomeDesk-all-monitors.flag")))?1:0;changingMonitorScope=false;
                preferences.MonitorScope=old;MessageBox.Show(this,ex.Message,"WinHomeDesk");
            }
        };
        quality.SelectedIndexChanged+=delegate{if(changingMonitorScope)return;monitorReport=null;RefreshMonitorText();};
        monitorDetails.Click+=async delegate {
            if(monitorBusy)return;
            monitorBusy=true;SetRunning(IsEngineRunning());
            try {
                string exe=Path.Combine(root,"engine","WinHomeDesk-engine.exe");bool selected=SelectedAllMonitors;int q=quality.SelectedIndex;
                var reports=await Task.Run(()=>new[]{MonitorSharing.Probe(exe,true,q,8000),MonitorSharing.Probe(exe,selected,q,8000)});
                if(IsDisposed)return;if(!runtimeMonitorsConfirmed)monitorReport=reports[1];RefreshMonitorText();
                MessageBox.Show(this,L.T("감지된 모니터: ")+reports[0].Count+"\n"+MonitorCaption()+"\n\n"+(runtimeMonitorsConfirmed?runtimeMonitorDetails:reports[0].Details)+"\n"+(SeparateMonitors?L.T("개별 모드에서는 모니터마다 별도 포트로 접속합니다.\n연결 파일 저장 또는 RDCMan 그룹 관리에서 모니터별 RDG를 만드세요.\nRDCMan에서 각 항목을 연결한 뒤 Undock으로 개별 창을 여세요.\n소리는 첫 번째 항목만 전송합니다. 모든 창은 같은 Windows 세션을 공유합니다.\n방화벽에서 사용한 모든 TCP 포트의 접속을 허용해야 합니다.\n모니터 배치가 바뀌면 서버를 중지하고 다시 시작하세요."):L.T("RDCMan에서는 모든 원격 모니터를 한 화면으로 봅니다.\n새 RDG 파일은 창 크기에 맞춰 전체 화면을 축소합니다.\n기존 항목: Properties > Display Settings > Scale docked/undocked remote desktop to fit window.\n모니터 배치가 바뀌면 서버를 중지하고 다시 시작하세요.")),"WinHomeDesk");
            }catch(Exception ex){if(!IsDisposed){monitorReport=null;RefreshMonitorText();MessageBox.Show(this,L.T("모니터 확인 실패: ")+ex.Message,"WinHomeDesk");}}
            finally{monitorBusy=false;if(!IsDisposed)SetRunning(IsEngineRunning());}
        };
        RefreshMonitorText();
        SetRunning(false);
    }
    string MonitorCaption() {
        if(SeparateMonitors && monitorReport!=null)return L.T("개별 연결: ")+monitorReport.Count+L.T("개 / 포트 ")+port.Value+"–"+((int)port.Value+monitorReport.Count-1)+L.T(" · 소리는 첫 번째 연결만 / 성능 수치는 첫 연결 기준");
        return monitorReport==null?L.T("공유 범위 선택됨 · 엔진 모니터 목록은 아직 확인하지 않았습니다."):
            (runtimeMonitorsConfirmed?L.T("실제 공유: "):L.T("시작 전 예상: "))+monitorReport.Count+L.T("개 / 전송 영역 ")+monitorReport.Width+" × "+monitorReport.Height;
    }
    void RefreshMonitorText() {
        monitorSummary.Text=MonitorCaption();
        rdp.Text=SeparateMonitors?L.T("모니터 RDG 저장"):L.T("연결 파일 저장");
        consent.Text=SelectedAllMonitors?L.T("모든 모니터 화면 공유에 동의합니다. 주변 사람도 화면을 볼 수 있습니다."):L.T("주 모니터 화면 공유에 동의합니다. 주변 사람도 화면을 볼 수 있습니다.");
    }
}
