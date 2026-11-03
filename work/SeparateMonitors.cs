using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

public sealed class MonitorConnection {
    public string Device;
    public int Port;
    public bool Audio;
    public static List<MonitorConnection> Plan(MonitorReport report,bool separate,int basePort,bool audio) {
        int count=separate?report.Count:1;
        if(count<1 || count>16 || basePort<1 || basePort>65536-count)throw new InvalidDataException(L.T("모니터별 포트 범위가 1~65535를 벗어납니다."));
        if(separate && (!report.SelectorCompatible || report.Devices.Length!=count))throw new InvalidDataException(L.T("모니터별 연결은 0.3.2 엔진이 필요합니다. ZIP 전체를 새 폴더에 압축 해제하세요."));
        var result=new List<MonitorConnection>();
        for(int i=0;i<count;i++)result.Add(new MonitorConnection {Device=separate?report.Devices[i]:null,Port=basePort+i,Audio=audio && i==0});
        return result;
    }
    public static List<SurfaceEntry> Entries(List<MonitorConnection> plan,string host) {
        var result=new List<SurfaceEntry>();
        foreach(var endpoint in plan)result.Add(new SurfaceEntry(Environment.MachineName,Environment.MachineName+(endpoint.Device==null?"":" — "+endpoint.Device),host,endpoint.Port));
        SurfaceRdg.Validate(result);return result;
    }
}

public sealed partial class SurfaceDesk {
    sealed class MonitorSession {
        public Process Process;
        public MonitorConnection Connection;
        public bool Ready;
    }
    readonly List<MonitorSession> monitorSessions=new List<MonitorSession>();
    List<MonitorConnection> activeMonitorPlan;
    readonly Timer listenerTimer=new Timer();
    DateTime listenerDeadline;
    static string MonitorDataDirectory(MonitorConnection connection) {
        string data=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SurfaceDesk");
        return connection.Device==null?data:Path.Combine(data,"monitors","port-"+connection.Port);
    }
    string MonitorFingerprints() {
        var plan=activeMonitorPlan;
        if(plan==null) {
            if(SeparateMonitors)throw new InvalidOperationException(L.T("서버를 한 번 시작한 후 확인하세요.\n"));
            plan=new List<MonitorConnection>{new MonitorConnection {Port=(int)port.Value}};
        }
        var result=new StringBuilder();
        foreach(var endpoint in plan) {
            string path=Path.Combine(MonitorDataDirectory(endpoint),"shadow","shadow.crt");
            string pem=File.ReadAllText(path).Replace("-----BEGIN CERTIFICATE-----","").Replace("-----END CERTIFICATE-----","");
            using(var certificate=new X509Certificate2(Convert.FromBase64String(pem)))result.AppendLine((endpoint.Device??"desktop")+" : "+endpoint.Port+"\n"+certificate.Thumbprint+"\n");
        }
        return result.ToString();
    }
    void InitializeMonitorSessions() {
        listenerTimer.Interval=1000;
        listenerTimer.Tick+=delegate {
            if(monitorSessions.Count==0){listenerTimer.Stop();return;}
            bool ready=true;foreach(var session in monitorSessions)ready &= session.Ready;
            if(ready){listenerTimer.Stop();return;}
            if(DateTime.UtcNow>=listenerDeadline){AddLog(L.T("모든 모니터 포트가 준비되지 않아 서버를 중지합니다. 로그와 포트 충돌을 확인하세요."));StopServer();}
        };
        FormClosed+=delegate{listenerTimer.Dispose();};
    }
    void QueueMonitorOutput(MonitorSession session,string line) {
        if(line==null || IsDisposed)return;
        try{BeginInvoke(new Action(delegate {
            if(!monitorSessions.Contains(session))return;
            if(IsListeningLine(line))session.Ready=true;
            if(Object.ReferenceEquals(engine,session.Process))EngineOutput(session.Process,line);
            else AddLog("["+session.Connection.Device+":"+session.Connection.Port+"] "+line.Replace("SurfaceDesk ","WinHomeDesk "));
            bool ready=true;foreach(var s in monitorSessions)ready &= s.Ready;
            listenerReported=ready;
        }));}catch(InvalidOperationException){}
    }
    void LaunchMonitorSessions(string exe,bool all,int selectedQuality,int effectiveGraphics,List<MonitorConnection> plan) {
        byte[] salt=new byte[16];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(salt);
        byte[] hash;using(var derive=new Rfc2898DeriveBytes(password.Text,salt,200000))hash=derive.GetBytes(32);
        string saltText=BitConverter.ToString(salt).Replace("-","");
        string hashText=BitConverter.ToString(hash).Replace("-","");Array.Clear(hash,0,hash.Length);
        string ntHash=AppCredentials.NtlmHash(password.Text);
        activeMonitorPlan=plan;listenerReported=false;
        foreach(var connection in plan) {
            string args="/bind-address:"+addresses.SelectedItem+" /port:"+connection.Port+" /sec:tls +auth +may-view +may-interact /max-connections:1 /log-level:INFO"+GraphicsMode.Arguments(effectiveGraphics);
            var p=new Process {StartInfo=new ProcessStartInfo(exe,args) {UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=root,RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8},EnableRaisingEvents=true};
            var session=new MonitorSession {Process=p,Connection=connection};
            var env=p.StartInfo.EnvironmentVariables;
            env["SURFACE_DESK_SALT"]=saltText;env["SURFACE_DESK_HASH"]=hashText;env["WINHOME_DESK_NT_HASH"]=ntHash;
            // Separate listeners never race to generate or update the same TLS certificate.
            env["SURFACE_DESK_DATA"]=MonitorDataDirectory(connection);
            env["OPENSSL_MODULES"]=Path.Combine(root,"engine","ossl-modules");
            MonitorSharing.Configure(p.StartInfo,all,selectedQuality);
            if(connection.Device!=null)env["WINHOME_DESK_MONITOR_DEVICE"]=connection.Device;
            env["SURFACE_DESK_GPU"]=encoderMode.SelectedIndex==0?"1":"0";
            env["WINHOME_DESK_RALT_HANGUL"]=preferences.RightAltHangul?"1":"0";
            env["SURFACE_DESK_AUDIO"]=connection.Audio?"1":"0";
            // One clipboard owner per desktop prevents feedback between monitor sessions.
            env["WINHOME_DESK_CLIPBOARD"]=(preferences.ClipboardEnabled && Object.ReferenceEquals(connection,plan[0]))?"1":"0";
            env["SURFACE_DESK_CAPTURE"]=captureMode.SelectedIndex==1?"gdi":"auto";
            env["SURFACE_DESK_FPS"]=frameRate.SelectedIndex==2?"45":frameRate.SelectedIndex==1?"30":"20";
            p.OutputDataReceived+=delegate(object o,DataReceivedEventArgs e){QueueMonitorOutput(session,e.Data);};
            p.ErrorDataReceived+=delegate(object o,DataReceivedEventArgs e){QueueMonitorOutput(session,e.Data);};
            p.Exited+=delegate {
                if(!IsDisposed)try{BeginInvoke(new Action(delegate {
                    if(!monitorSessions.Contains(session))return;
                    AddLog("["+(connection.Device??"desktop")+":"+connection.Port+"] "+L.T("RDP 엔진 종료. 잠금·화면 변경 또는 위 오류를 확인하세요."));
                    StopServer();
                    if(!exitRequested && !Visible && tray!=null)tray.ShowBalloonTip(4000,"WinHomeDesk",L.T("서버가 중지되었습니다. 트레이에서 창을 열어 로그를 확인하세요."),ToolTipIcon.Warning);
                }));}catch(InvalidOperationException){}
            };
            monitorSessions.Add(session);if(engine==null)engine=p;
            try {p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();}
            finally {env.Remove("WINHOME_DESK_NT_HASH");env.Remove("SURFACE_DESK_HASH");}
            AddLog("["+(connection.Device??"desktop")+"] "+addresses.SelectedItem+":"+connection.Port);
        }
        listenerDeadline=DateTime.UtcNow.AddSeconds(30);listenerTimer.Start();
    }
    void StopMonitorSessions() {
        listenerTimer.Stop();
        // Also handle the original single-engine lifecycle (including partially
        // initialized sessions) without leaving an orphan listener behind.
        bool found=false;foreach(var session in monitorSessions)if(Object.ReferenceEquals(session.Process,engine))found=true;
        if(engine!=null && !found)monitorSessions.Add(new MonitorSession {Process=engine});
        var sessions=monitorSessions.ToArray();monitorSessions.Clear();activeMonitorPlan=null;
        engine=null;listenerReported=false;
        foreach(var session in sessions) {
            try {if(!session.Process.HasExited){session.Process.Kill();session.Process.WaitForExit(3000);}}
            catch(InvalidOperationException){}catch(Exception ex){AddLog(ex.Message);}
            finally{session.Process.Dispose();}
        }
    }
    async void OpenMonitorGroups(bool saveOnly) {
        if(monitorBusy)return;
        monitorBusy=true;SetRunning(IsEngineRunning());
        try {
            var plan=activeMonitorPlan;
            if(plan==null && !SeparateMonitors)plan=new List<MonitorConnection>{new MonitorConnection {Port=(int)port.Value}};
            if(plan==null) {
                bool separate=SeparateMonitors,all=SelectedAllMonitors;int q=quality.SelectedIndex,basePort=(int)port.Value;bool audio=audioEnabled.Checked;
                string exe=Path.Combine(root,"engine","WinHomeDesk-engine.exe");
                var report=await Task.Run(()=>MonitorSharing.Probe(exe,all,q,8000));
                if(IsDisposed || exitRequested)return;
                plan=MonitorConnection.Plan(report,separate,basePort,audio);
                monitorReport=report;RefreshMonitorText();
            }
            var entries=MonitorConnection.Entries(plan,Convert.ToString(addresses.SelectedItem));
            if(saveOnly) {
                using(var d=new SaveFileDialog {Filter=L.T("RDCMan 그룹 파일|*.rdg"),FileName="WinHomeDesk-Monitors.rdg"}) {
                    if(d.ShowDialog(this)!=DialogResult.OK)return;
                    SurfaceRdg.Save(d.FileName,entries,"WinHomeDesk");AddLog(L.T("저장: ")+d.FileName);
                }
            } else using(var manager=new RdgManagerForm(Convert.ToString(addresses.SelectedItem),(int)port.Value,entries)){manager.Icon=Icon;manager.ShowDialog(this);}
        }catch(Exception ex){if(!IsDisposed)MessageBox.Show(this,ex.Message,"WinHomeDesk");}
        finally{monitorBusy=false;if(!IsDisposed)SetRunning(IsEngineRunning());}
    }
}
