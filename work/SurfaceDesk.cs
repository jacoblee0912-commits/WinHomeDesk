using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Security.Principal;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("WinHomeDesk")]
[assembly: System.Reflection.AssemblyProduct("WinHomeDesk")]
[assembly: System.Reflection.AssemblyVersion("0.3.0.0")]
public sealed partial class SurfaceDesk : Form {
    readonly TextBox password = new TextBox();
    readonly ComboBox addresses = new ComboBox();
    readonly ComboBox quality = new ComboBox();
    readonly ComboBox frameRate = new ComboBox();
    readonly NumericUpDown port = new NumericUpDown();
    readonly Button start = new Button(), stop = new Button(), rdp = new Button();
    readonly TextBox log = new TextBox();
    readonly CheckBox consent = new CheckBox();
    readonly Label status = new Label();
    readonly Timer statusTimer = new Timer();
    readonly Label captureMetrics=new Label(), sendMetrics=new Label(), pipelineMetrics=new Label();
    PerformanceMetrics performance=new PerformanceMetrics();
    volatile bool listenerReported;
    Process engine;
    string root = AppDomain.CurrentDomain.BaseDirectory;
    public SurfaceDesk() : this(false,false) {}
    public SurfaceDesk(bool startup,bool test) {
        Text=L.T("WinHomeDesk — 독립 RDP 서버 0.3.0 · 실험 버전");
        ClientSize=new Size(790,980); MinimumSize=Size; Font=new Font("맑은 고딕",10);
        StartPosition=FormStartPosition.CenterScreen;
        Label heading=new Label {Text=L.T("내 서피스 화면을 RDCMan으로 연결"),Font=new Font(Font,FontStyle.Bold),AutoSize=true,Location=new Point(22,20)};
        Controls.Add(heading);
        Text += StartupTask.IsElevated?L.T(" · 관리자 권한"):L.T(" · 일반 권한");
        Button launchClient=new Button {Name="LaunchRdcMan",Text=L.T("RDCMan 실행 (한글 보정)"),Location=new Point(342,14),Size=new Size(228,32)};
        launchClient.Click+=delegate{LaunchRdcManClient();};Controls.Add(launchClient);
        Controls.Add(new Label {Text=L.T("Windows Home x64용 프로토타입 · FreeRDP 기반 · RDPWrap 불필요"),AutoSize=true,Location=new Point(22,53)});
        Controls.Add(new Label {Text=L.T("접속 사용자: surface     접속 암호 (이 프로그램 전용)"),AutoSize=true,Location=new Point(22,87)});
        byte[] random=new byte[18];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(random);
        password.Text=Convert.ToBase64String(random);password.UseSystemPasswordChar=true;password.SetBounds(22,112,390,27);Controls.Add(password);
        CheckBox show=new CheckBox {Text=L.T("암호 보기"),Location=new Point(430,112),AutoSize=true};show.CheckedChanged+=delegate{password.UseSystemPasswordChar=!show.Checked;};Controls.Add(show);
        Controls.Add(new Label {Text=L.T("연결 주소"),AutoSize=true,Location=new Point(22,151)});
        addresses.SetBounds(112,146,300,30);addresses.DropDownStyle=ComboBoxStyle.DropDownList;
        addresses.Items.Add("127.0.0.1");
        foreach(NetworkInterface n in NetworkInterface.GetAllNetworkInterfaces()) {
            if(n.OperationalStatus!=OperationalStatus.Up || n.NetworkInterfaceType==NetworkInterfaceType.Loopback)continue;
            foreach(UnicastIPAddressInformation a in n.GetIPProperties().UnicastAddresses)
                if(a.Address.AddressFamily==AddressFamily.InterNetwork && !a.Address.ToString().StartsWith("169.254."))
                    if(!addresses.Items.Contains(a.Address.ToString()))addresses.Items.Add(a.Address.ToString());
        }
        addresses.SelectedIndex=addresses.Items.Count>1?1:0;Controls.Add(addresses);
        Controls.Add(new Label {Text=L.T("포트"),AutoSize=true,Location=new Point(440,151)});
        port.Minimum=1024;port.Maximum=65535;port.Value=3390;port.SetBounds(487,146,110,30);Controls.Add(port);
        consent.Text=L.T("현재 화면 공유를 시작합니다. 주변 사람도 화면을 볼 수 있습니다.");
        consent.SetBounds(22,192,740,30);Controls.Add(consent);
        Controls.Add(new Label {Text=L.T("잠금·UAC 보안 화면·해상도 변경 시 중지됩니다. 로그인 전 접속은 지원하지 않습니다.\n창을 닫으면 트레이에서 계속 실행됩니다. 완전 종료는 트레이 메뉴에서 합니다."),AutoSize=true,Location=new Point(22,232)});
        start.Text=L.T("서버 시작");start.SetBounds(22,293,140,38);start.Click+=StartServer;Controls.Add(start);
        stop.Text=L.T("서버 중지");stop.SetBounds(176,293,140,38);stop.Enabled=false;stop.Click+=delegate{StopServer();};Controls.Add(stop);
        rdp.Text=L.T("연결 파일 저장");rdp.SetBounds(330,293,160,38);rdp.Click+=SaveConnection;Controls.Add(rdp);
        Button cert=new Button {Text=L.T("인증서 지문 확인"),Location=new Point(510,293),Size=new Size(190,38)};
        cert.Click+=delegate {
            try {
                string path=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SurfaceDesk","shadow","shadow.crt");
                string pem=File.ReadAllText(path).Replace("-----BEGIN CERTIFICATE-----","").Replace("-----END CERTIFICATE-----","");
                X509Certificate2 certificate=new X509Certificate2(Convert.FromBase64String(pem));
                MessageBox.Show(this,L.T("서버 인증서 SHA-1 지문:\n")+certificate.Thumbprint+L.T("\n\nRDCMan의 인증서 상세 정보와 비교하세요."),L.T("인증서 확인"));
            }catch(Exception ex){MessageBox.Show(this,L.T("서버를 한 번 시작한 후 확인하세요.\n")+ex.Message);}
        };Controls.Add(cert);
        status.SetBounds(22,337,746,28);status.AutoEllipsis=true;Controls.Add(status);
        Button diagnose=new Button {Text=L.T("상태 확인"),Location=new Point(22,370),Size=new Size(140,32)};
        diagnose.Click+=delegate{UpdateStatus();AddLog(status.Text);AddLog(L.T("외부 접속은 방화벽·같은 네트워크 여부도 확인해야 합니다."));};Controls.Add(diagnose);
        Button copy=new Button {Text=L.T("접속 주소 복사"),Location=new Point(176,370),Size=new Size(160,32)};
        copy.Click+=delegate{Clipboard.SetText(Endpoint());AddLog(L.T("복사한 주소를 원격 데스크톱의 컴퓨터 칸에 입력하세요: ")+Endpoint());};Controls.Add(copy);
        Button logs=new Button {Text=L.T("로그 저장"),Location=new Point(350,370),Size=new Size(140,32)};
        logs.Click+=delegate{using(SaveFileDialog d=new SaveFileDialog {Filter=L.T("진단 기록|*.txt"),FileName="WinHomeDesk-log.txt"}){if(d.ShowDialog(this)==DialogResult.OK)File.WriteAllText(d.FileName,status.Text+Environment.NewLine+captureMetrics.Text+Environment.NewLine+sendMetrics.Text+Environment.NewLine+pipelineMetrics.Text+Environment.NewLine+actualEncoder.Text+Environment.NewLine+audioStatus.Text+Environment.NewLine+log.Text,Encoding.UTF8);}};Controls.Add(logs);
        Button groups=new Button {Text=L.T("RDCMan 그룹 관리"),Location=new Point(510,370),Size=new Size(190,32)};
        groups.Click+=delegate{using(RdgManagerForm manager=new RdgManagerForm(Convert.ToString(addresses.SelectedItem),(int)port.Value)){manager.Icon=Icon;manager.ShowDialog(this);}};Controls.Add(groups);
        statusTimer.Interval=1000;statusTimer.Tick+=delegate{UpdateStatus();RefreshPerformance();RefreshTray();};statusTimer.Start();
        FormClosed+=delegate{statusTimer.Dispose();};
        addresses.SelectedIndexChanged+=delegate{UpdateStatus();};port.ValueChanged+=delegate{UpdateStatus();};
        Controls.Add(new Label {Text=L.T("전송 화질"),AutoSize=true,Location=new Point(22,420)});
        quality.DropDownStyle=ComboBoxStyle.DropDownList;quality.SetBounds(112,415,365,30);
        quality.Items.AddRange(new object[]{L.T("균형 — 긴 변 최대 1920px (기본)"),L.T("속도 우선 — 긴 변 최대 1440px"),L.T("원본 선명도 — 원본 해상도")});
        quality.SelectedIndex=0;Controls.Add(quality);
        Controls.Add(new Label {Text=L.T("목표 FPS"),AutoSize=true,Location=new Point(494,420)});
        frameRate.DropDownStyle=ComboBoxStyle.DropDownList;frameRate.SetBounds(580,415,122,30);
        frameRate.Items.AddRange(new object[]{"20 FPS","30 FPS","45 FPS"});frameRate.SelectedIndex=0;Controls.Add(frameRate);
        captureMetrics.SetBounds(22,616,746,66);captureMetrics.Font=new Font(Font.FontFamily,9);
        captureMetrics.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(captureMetrics);
        sendMetrics.SetBounds(22,683,746,66);sendMetrics.Font=new Font(Font.FontFamily,9);
        sendMetrics.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(sendMetrics);
        pipelineMetrics.SetBounds(22,752,746,39);pipelineMetrics.Font=new Font(Font.FontFamily,9);
        pipelineMetrics.ForeColor=Color.DimGray;Controls.Add(pipelineMetrics);
        RefreshPerformance();
        log.Multiline=true;log.ReadOnly=true;log.ScrollBars=ScrollBars.Vertical;log.SetBounds(22,799,746,159);log.Anchor=AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(log);

        AddLog(L.T("내부 IP 후보를 선택했습니다. 접속할 네트워크의 IP인지 확인하세요. 127.0.0.1은 같은 PC 전용입니다."));
        UpdateStatus();
        AddLog(L.T("RDCMan: Server name = 선택한 IP, Connection Settings의 포트 = 3390 (기본값)"));
        AddLog(L.T("이 버전은 TLS 인증서를 자동 생성합니다. 첫 연결 시 인증서 지문을 확인하세요."));
        AddLog(L.T("화질·캡처 방식은 서버 중지 후 변경하세요. 전송 화면만 축소하며 서피스의 실제 해상도는 유지합니다."));
        InitializeGraphics();
        InitializeAudio();
        InitializeEncoder();
        InitializeLifecycle(startup,test);
        InitializeLanguageSelector();
        AddLog(L.T("창의 X는 트레이로 숨김 / 완전 종료는 트레이 메뉴 / 자동 시작은 환경 설정에서 선택하세요."));
    }
    void AddLog(string s) {
        if(IsDisposed)return;
        if(InvokeRequired){try{BeginInvoke(new Action<string>(AddLog),s);}catch(InvalidOperationException){}return;}
        if(log.TextLength>240000)log.Text=L.T("[오래된 로그 일부 생략]")+Environment.NewLine+log.Text.Substring(log.TextLength-180000);
        log.AppendText(DateTime.Now.ToString("HH:mm:ss")+" "+s+Environment.NewLine);
    }
    void RefreshPerformance() {
        DateTime now=DateTime.UtcNow;
        string captureAge=performance.CaptureAt!=DateTime.MinValue && (now-performance.CaptureAt).TotalSeconds>6 ?
            "["+((int)(now-performance.CaptureAt).TotalSeconds)+L.T("초 전 측정] ") : "";
        string sendAge=!performance.SendClosed && performance.SendAt!=DateTime.MinValue && (now-performance.SendAt).TotalSeconds>6 ?
            "["+((int)(now-performance.SendAt).TotalSeconds)+L.T("초 전 측정] ") : "";
        captureMetrics.Text=captureAge+performance.CaptureText;
        sendMetrics.Text=sendAge+performance.SendText;
        string pipelineAge=performance.PipelineAt!=DateTime.MinValue && (now-performance.PipelineAt).TotalSeconds>6 ?
            "["+((int)(now-performance.PipelineAt).TotalSeconds)+L.T("초 전] ") : "";
        pipelineMetrics.Text=pipelineAge+performance.PipelineText;
    }
    void SetRunning(bool running) {
        start.Enabled=!running;stop.Enabled=running;addresses.Enabled=!running;port.Enabled=!running;password.Enabled=!running;quality.Enabled=!running;frameRate.Enabled=!running;captureMode.Enabled=!running;graphicsMode.Enabled=!running;audioEnabled.Enabled=!running;encoderMode.Enabled=!running;
    }
    string Endpoint() { return Convert.ToString(addresses.SelectedItem)+":"+port.Value; }
    public static bool IsListeningLine(string line) {
        return line.StartsWith("SurfaceDesk LISTENING port=",StringComparison.Ordinal);
    }
    void EngineOutput(Process source,string line) {
        if(!Object.ReferenceEquals(engine,source))return;
        if(IsListeningLine(line))listenerReported=true;
        string encoder=EncoderCaption(line);
        if(encoder!=null && !IsDisposed)try{BeginInvoke(new Action(delegate {
            if(Object.ReferenceEquals(engine,source))actualEncoder.Text=EncoderCaption(line);
        }));}catch(InvalidOperationException){}
        string audio=AudioCaption(line);
        if(audio!=null && !IsDisposed)try{BeginInvoke(new Action(delegate {
            if(Object.ReferenceEquals(engine,source)){lastAudioLine=line;audioStatus.Text=AudioCaption(line);}
        }));}catch(InvalidOperationException){}
        string codec=GraphicsMode.Caption(line);
        if(codec!=null && !IsDisposed)try{BeginInvoke(new Action(delegate {
            if(Object.ReferenceEquals(engine,source))actualCodec.Text=GraphicsMode.Caption(line);
        }));}catch(InvalidOperationException){}
        if(line.StartsWith("SurfaceDesk CAPTURE backend=",StringComparison.Ordinal)) {
            if(!IsDisposed)try{BeginInvoke(new Action(delegate {
                if(Object.ReferenceEquals(engine,source))captureBackend.Text=line.Contains("backend=dxgi")?L.T("사용 중: DirectX (DXGI)"):L.T("사용 중: GDI");
            }));}catch(InvalidOperationException){}
        }
        if(line.StartsWith("SurfaceDesk PERF_",StringComparison.Ordinal)) {
            if(!IsDisposed)try {BeginInvoke(new Action(delegate {
                if(Object.ReferenceEquals(engine,source) && performance.Accept(line)){RefreshPerformance();if(performance.SendClosed){actualCodec.Text=L.T("실제 전송: 연결 종료");actualEncoder.Text=L.T("인코더: 연결 대기");audioStatus.Text=audioEnabled.Checked?L.T("소리 전송: 연결 종료 / 다음 접속 대기"):L.T("소리 전송: 꺼짐");}}
            }));}catch(InvalidOperationException){}
        }
        AddLog(line.Replace("SurfaceDesk ","WinHomeDesk "));
        if(line.Contains("password not supplied by client"))AddLog(L.T("암호 미전달: 새 연결 파일을 사용하고, 다른 계정 사용에서 surface와 현재 앱 암호를 입력하세요."));
        else if(line.Contains("username missing") || line.Contains("username must be surface"))AddLog(L.T("계정 오류: 사용자 이름은 surface, RDCMan의 Domain은 빈칸으로 설정하세요."));
        else if(line.Contains("password mismatch or derivation failed"))AddLog(L.T("암호 확인 실패: Windows PIN 대신 현재 WinHomeDesk 창의 전용 암호를 사용하세요."));
    }
    void UpdateStatus() {
        try {
            bool running=IsEngineRunning();
            bool listening=listenerReported;
            string prefix=running?(listening?L.T("포트 수신 확인"):L.T("서버 시작 중 / 포트 수신 미확인")):L.T("서버 중지됨");
            string suffix=Convert.ToString(addresses.SelectedItem)=="127.0.0.1"?L.T(" — 같은 PC 전용"):L.T(" — 다른 PC에서 입력할 주소");
            status.Text=prefix+": "+Endpoint()+suffix;
            status.ForeColor=running&&listening?Color.DarkGreen:Color.DarkRed;
        }catch(Exception ex){status.Text=L.T("상태 확인 실패: ")+ex.Message;}
    }
    void StartServer(object sender,EventArgs e) {
        startupTimer.Stop();
        if(!consent.Checked){MessageBox.Show(this,L.T("현재 화면 공유 확인란을 선택하세요."));return;}
        if(password.Text.Length<12){MessageBox.Show(this,L.T("접속 암호는 12자 이상으로 설정하세요."));return;}
        string exe=Path.Combine(root,"engine","WinHomeDesk-engine.exe");
        if(!File.Exists(exe)){MessageBox.Show(this,L.T("RDP 엔진이 없습니다. ZIP 전체를 압축 해제하세요."));return;}
        try {
            SaveConnectionPreferences();
            actualCodec.Text=L.T("실제 전송: 연결 대기");
            actualEncoder.Text=L.T("인코더: 연결 대기");
            AddLog(L.T("전송 방식: ")+graphicsMode.SelectedItem+" / "+encoderMode.SelectedItem+L.T(". 실제 사용 인코더는 접속 후 표시됩니다. AVC444는 CPU를 사용합니다."));
            string args="/bind-address:"+addresses.SelectedItem+" /port:"+port.Value+" /sec:tls +auth +may-view +may-interact /max-connections:1 /log-level:INFO"+GraphicsMode.Arguments(graphicsMode.SelectedIndex);
            Process p=new Process(); p.StartInfo=new ProcessStartInfo(exe,args) {UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=root,RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
            byte[] salt=new byte[16];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(salt);
            byte[] hash;using(var derive=new Rfc2898DeriveBytes(password.Text,salt,200000))hash=derive.GetBytes(32);
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_SALT"]=BitConverter.ToString(salt).Replace("-","");
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_HASH"]=BitConverter.ToString(hash).Replace("-","");
            p.StartInfo.EnvironmentVariables["WINHOME_DESK_NT_HASH"]=AppCredentials.NtlmHash(password.Text);
            Array.Clear(hash,0,hash.Length);
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_DATA"]=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SurfaceDesk");
            p.StartInfo.EnvironmentVariables["OPENSSL_MODULES"]=Path.Combine(root,"engine","ossl-modules");
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_MAX_EDGE"]=quality.SelectedIndex==1?"1440":quality.SelectedIndex==2?"0":"1920";
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_GPU"]=encoderMode.SelectedIndex==0?"1":"0";
            p.StartInfo.EnvironmentVariables["WINHOME_DESK_RALT_HANGUL"]=preferences.RightAltHangul?"1":"0";
            AddLog(L.T("키보드: 오른쪽 Alt → ")+(preferences.RightAltHangul?L.T("한영 전환"):L.T("원래 키보드 배치")));
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_AUDIO"]=audioEnabled.Checked?"1":"0";
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_CAPTURE"]=captureMode.SelectedIndex==1?"gdi":"auto";
            p.StartInfo.EnvironmentVariables["SURFACE_DESK_FPS"]=frameRate.SelectedIndex==2?"45":frameRate.SelectedIndex==1?"30":"20";
            p.OutputDataReceived+=delegate(object o,DataReceivedEventArgs a){if(a.Data!=null)EngineOutput(p,a.Data);};
            p.ErrorDataReceived+=delegate(object o,DataReceivedEventArgs a){if(a.Data!=null)EngineOutput(p,a.Data);};
            p.EnableRaisingEvents=true;
            p.Exited+=delegate{AddLog(L.T("RDP 엔진 종료. 잠금·화면 변경 또는 위 오류를 확인하세요."));if(!IsDisposed)try{BeginInvoke(new Action(delegate{if(Object.ReferenceEquals(engine,p)) {
                SetRunning(false);captureBackend.Text=L.T("캡처 중지");actualCodec.Text=L.T("실제 전송: 서버 중지");actualEncoder.Text=L.T("인코더: 서버 중지");audioStatus.Text=L.T("소리 전송: 중지");
                if(!exitRequested && !Visible && tray!=null)tray.ShowBalloonTip(4000,"WinHomeDesk",L.T("서버가 중지되었습니다. 트레이에서 창을 열어 로그를 확인하세요."),ToolTipIcon.Warning);
            }}));}catch(InvalidOperationException){}};
            if(engine!=null)engine.Dispose();
            performance=new PerformanceMetrics();RefreshPerformance();
            captureBackend.Text=L.T("캡처 시작 중");audioStatus.Text=audioEnabled.Checked?L.T("소리 캡처 시작 중"):L.T("소리 전송: 꺼짐");
            listenerReported=false;engine=p;p.Start();
            p.StartInfo.EnvironmentVariables.Remove("WINHOME_DESK_NT_HASH");
            p.StartInfo.EnvironmentVariables.Remove("SURFACE_DESK_HASH");
            SetRunning(true);p.BeginOutputReadLine();p.BeginErrorReadLine();
            AddLog(L.T("인증: NLA + TLS 호환. RDCMan 3.22 새 엔진과 기존 엔진에서 surface / 현재 앱 전용 암호를 사용하세요."));
            AddLog(L.T("시작 요청: ")+addresses.SelectedItem+":"+port.Value+L.T(". LISTEN 로그로 실제 시작 여부를 확인하세요."));
            AddLog(L.T("전송 화질: ")+quality.SelectedItem+L.T(" / 목표 ")+frameRate.SelectedItem+L.T(". 실제 속도는 화면 변화량과 네트워크에 따라 달라집니다."));
        }catch(Exception ex){AddLog(L.T("서버 시작 실패: ")+ex.Message);StopServer();}
    }
    void StopServer() {
        lastAudioLine=null;
        startupTimer.Stop();
        Process p=engine;engine=null;listenerReported=false;
        if(p!=null){try{if(!p.HasExited){p.Kill();p.WaitForExit(3000);}}catch(Exception ex){AddLog(ex.Message);}finally{p.Dispose();}}
        if(!IsDisposed){SetRunning(false);captureBackend.Text=L.T("캡처 중지");actualCodec.Text=L.T("실제 전송: 서버 중지");actualEncoder.Text=L.T("인코더: 서버 중지");audioStatus.Text=L.T("소리 전송: 중지");UpdateStatus();RefreshTray();}
    }
    void SaveConnection(object sender,EventArgs e) {
        UpdateStatus();
        if(Convert.ToString(addresses.SelectedItem)=="127.0.0.1") {
            MessageBox.Show(this,L.T("현재 주소 127.0.0.1은 이 PC 전용입니다. 다른 PC로 옮겨 사용할 파일은 연결 주소에서 서피스의 내부 IP를 선택한 뒤 저장하세요."));return;
        }

        using(SaveFileDialog d=new SaveFileDialog {Filter=L.T("원격 데스크톱 연결|*.rdp"),FileName="WinHomeDesk.rdp"}) {
            if(d.ShowDialog(this)!=DialogResult.OK)return;
            string text="full address:s:"+addresses.SelectedItem+":"+port.Value+"\r\nusername:s:surface\r\ndomain:s:\r\nprompt for credentials:i:1\r\nprompt for credentials on client:i:1\r\nauthentication level:i:2\r\nenablecredsspsupport:i:1\r\nredirectclipboard:i:0\r\n";
            text+="session bpp:i:32\r\naudiomode:i:0\r\naudiocapturemode:i:0\r\n";
            File.WriteAllText(d.FileName,text,Encoding.Unicode);AddLog(L.T("저장: ")+d.FileName);
        }
    }
    [STAThread] public static void Main(string[] args) {
        L.LoadPreference();
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        if(ManagedStartupRegistration.HandleHelper(args))return;
        if(args.Length==2 && (args[0]=="--preview" || args[0]=="--preview-perf")) {
            using(SurfaceDesk f=new SurfaceDesk(false,true)) {
                if(args[0]=="--preview-perf") {
                    f.performance.Accept("SurfaceDesk PERF_CAPTURE window_s=2.010 target=45 frames=38 fps=18.91 changed_fps=17.91 capture_ms=12.250 copy_ms=2.300 wait_ms=35.500 work_max_ms=85.900 late=1 errors=0 backend=dxgi pipeline=latest replaced=8");
                    f.performance.Accept("SurfaceDesk PERF_SEND state=active window_s=2.010 frames=36 fps=17.91 encode_ms=19.250 send_ms=14.500 send_max_ms=41.500 frame_max_ms=68.500 payload_bytes=1532000 payload_mbps=6.100 area_pct=78.400 failed=0 other=0");
                    f.performance.Accept("SurfaceDesk PERF_PIPELINE window_s=2.010 taken=36 changed=36 replaced=8 snapshot_ms=1.0 age_ms=9.2 age_max_ms=25.1 delivery_wait_ms=22.0 pending_limit=1");
                    f.RefreshPerformance();
                }
                f.Show();Application.DoEvents();using(Bitmap bitmap=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(bitmap,new Rectangle(Point.Empty,f.Size));bitmap.Save(args[1]);}f.ExitApplication();
            }return;
        }
        if(args.Length==2 && args[0]=="--preview-rdg") {
            using(RdgManagerForm f=new RdgManagerForm("192.168.0.110",3390)) {
                f.Show();Application.DoEvents();using(Bitmap bitmap=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(bitmap,new Rectangle(Point.Empty,f.Size));bitmap.Save(args[1]);}f.Close();
            }return;
        }
        bool startup=Array.IndexOf(args,"--startup")>=0;
        string sid=System.Security.Principal.WindowsIdentity.GetCurrent().User.Value;
        string name=@"Local\SurfaceDesk-"+sid;
        bool first;
        using(var gate=new System.Threading.Mutex(true,name,out first))
        using(var signal=new System.Threading.EventWaitHandle(false,System.Threading.EventResetMode.AutoReset,name+"-Show")) {
            if(!first){if(!startup)signal.Set();return;}
            try {using(var form=new SurfaceDesk(startup,false)){form.ShowRequest=signal;Application.Run(form);}}
            finally{gate.ReleaseMutex();}
        }
    }
}
