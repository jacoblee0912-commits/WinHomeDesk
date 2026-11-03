using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

public sealed partial class SurfaceDesk {
    readonly ComboBox captureMode=new ComboBox();
    readonly Label captureBackend=new Label();
    NotifyIcon tray;
    ToolStripMenuItem trayState,trayStart,trayStop;
    bool exitRequested, testMode, startupLaunch, trayTipShown;
    DesktopOptions preferences=new DesktopOptions();
    DesktopOptionsStore preferenceStore;
    public EventWaitHandle ShowRequest;
    readonly System.Windows.Forms.Timer startupTimer=new System.Windows.Forms.Timer();
    int startupTicks;

    void InitializeLifecycle(bool startup,bool test) {
        testMode=test;startupLaunch=startup;
        try{Icon=System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath);}catch{}
        Button options=new Button {Text=L.T("환경 설정"),Location=new Point(580,14),Size=new Size(122,32)};
        options.Click+=delegate{ShowOptions();};Controls.Add(options);
        Controls.Add(new Label {Text=L.T("캡처 방식"),AutoSize=true,Location=new Point(22,459)});
        captureMode.SetBounds(112,454,365,30);captureMode.DropDownStyle=ComboBoxStyle.DropDownList;
        captureMode.Items.AddRange(new object[]{L.T("자동 — DirectX 우선 / 실패 시 GDI"),L.T("GDI — 기존 캡처 방식")});
        captureMode.SelectedIndex=0;Controls.Add(captureMode);
        captureBackend.Text=L.T("캡처 대기");captureBackend.SetBounds(494,458,274,26);Controls.Add(captureBackend);
        if(!testMode) {
            preferenceStore=new DesktopOptionsStore(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SurfaceDesk","preferences.xml"));
            try {
                preferences=preferenceStore.Load();
                graphicsMode.SelectedIndex=preferences.GraphicsMode;audioEnabled.Checked=preferences.AudioEnabled;encoderMode.SelectedIndex=preferences.EncoderMode=="cpu"?1:0;
                quality.SelectedIndex=preferences.Quality;frameRate.SelectedIndex=preferences.FrameRate;
                captureMode.SelectedIndex=preferences.CaptureMode=="gdi"?1:0;port.Value=preferences.Port;
                if(addresses.Items.Contains(preferences.Host))addresses.SelectedItem=preferences.Host;
                if(preferences.StartServerAtLogin) {
                    string saved=preferences.ReadPassword();
                    if(saved.Length<12)throw new InvalidDataException(L.T("저장된 암호가 유효하지 않습니다."));
                    password.Text=saved;
                }
            }catch(Exception ex){preferences=new DesktopOptions();AddLog(L.T("설정 읽기 실패 — 자동 서버 시작 안 함: ")+ex.Message);}
            InitializeTray();
        }
        FormClosing+=HandleWindowClosing;
        FormClosed+=delegate {
            startupTimer.Dispose();
            if(tray!=null){tray.Visible=false;var menu=tray.ContextMenuStrip;tray.Dispose();if(menu!=null)menu.Dispose();}
        };
        Shown+=delegate {
            if(startupLaunch && !testMode) {
                HideToTray(false);
                if(preferences.StartServerAtLogin){startupTimer.Interval=1000;startupTimer.Tick+=StartupTick;startupTimer.Start();}
            }
        };
    }
    void InitializeTray() {
            tray=new NotifyIcon {Icon=Icon,Text=L.T("WinHomeDesk — 서버 중지"),Visible=true};
            var menu=new ContextMenuStrip();
            trayState=new ToolStripMenuItem(L.T("서버 중지")) {Enabled=false};menu.Items.Add(trayState);
            menu.Items.Add(L.T("창 열기"),null,delegate{RestoreWindow();});
            trayStart=new ToolStripMenuItem(L.T("서버 시작"),null,delegate{RestoreWindow();StartServer(this,EventArgs.Empty);});menu.Items.Add(trayStart);
            trayStop=new ToolStripMenuItem(L.T("서버 중지"),null,delegate{StopServer();});menu.Items.Add(trayStop);
            menu.Items.Add(L.T("환경 설정"),null,delegate{RestoreWindow();ShowOptions();});
            menu.Items.Add(L.T("RDCMan 실행 (한글 보정)"),null,delegate{LaunchRdcManClient();});
            menu.Items.Add(new ToolStripSeparator());menu.Items.Add(L.T("종료 (서버도 종료)"),null,delegate{ExitApplication();});
            tray.ContextMenuStrip=menu;tray.DoubleClick+=delegate{RestoreWindow();};
    }
    public static bool CloseToTray(CloseReason reason,bool exiting) {
        return !exiting && reason==CloseReason.UserClosing;
    }
    void HandleWindowClosing(object sender,FormClosingEventArgs e) {
        if(CloseToTray(e.CloseReason,exitRequested)){e.Cancel=true;HideToTray(true);return;}
        StopServer();
    }
    void HideToTray(bool explain) {
        Hide();ShowInTaskbar=false;
        if(explain && !trayTipShown && tray!=null){trayTipShown=true;tray.ShowBalloonTip(2500,"WinHomeDesk",L.T("트레이에서 계속 실행됩니다. 완전히 종료하려면 트레이 메뉴의 '종료'를 누르세요."),ToolTipIcon.Info);}
    }
    public void RestoreWindow() {ShowInTaskbar=true;Show();WindowState=FormWindowState.Normal;Activate();}
    public void ExitApplication(){exitRequested=true;Close();}
    bool IsEngineRunning() {
        try{return engine!=null && !engine.HasExited;}catch(InvalidOperationException){return false;}
    }
    void RefreshTray() {
        if(ShowRequest!=null && ShowRequest.WaitOne(0))RestoreWindow();
        if(tray==null)return;
        bool running=IsEngineRunning();
        tray.Text="WinHomeDesk — "+(running?(listenerReported?L.T("서버 실행 중"):L.T("서버 시작 중")):L.T("서버 중지"));
        trayState.Text=tray.Text;trayStart.Enabled=!running;trayStop.Enabled=running;
    }
    DesktopOptions CurrentOptions(bool autoServer) {
        var result=new DesktopOptions {Host=Convert.ToString(addresses.SelectedItem),Port=(int)port.Value,
            Quality=quality.SelectedIndex,FrameRate=frameRate.SelectedIndex,GraphicsMode=graphicsMode.SelectedIndex,AudioEnabled=audioEnabled.Checked,EncoderMode=encoderMode.SelectedIndex==0?"auto":"cpu",
            MonitorScope=preferences.MonitorScope,Language=L.Language,StartupElevated=preferences.StartupElevated,ClipboardEnabled=preferences.ClipboardEnabled,RightAltHangul=preferences.RightAltHangul,RdcManPath=preferences.RdcManPath,CaptureMode=captureMode.SelectedIndex==1?"gdi":"auto",StartServerAtLogin=autoServer};
        if(autoServer) {
            if(!consent.Checked)throw new InvalidOperationException(L.T("먼저 현재 화면 공유 확인란을 선택하세요."));
            if(password.Text.Length<12)throw new InvalidOperationException(L.T("전용 암호를 12자 이상 입력하세요."));
            result.ProtectPassword(password.Text);
        }
        return result;
    }
    void SaveConnectionPreferences() {
        if(testMode || preferenceStore==null)return;
        var next=CurrentOptions(preferences.StartServerAtLogin);
        preferenceStore.Save(next);preferences=next;
    }
    void StartupTick(object sender,EventArgs args) {
        startupTicks++;
        if(startupTicks<2)return;
        // Wait for this exact saved address; never silently bind a different network.
        if(HasLocalAddress(preferences.Host)) {
            startupTimer.Stop();
            if(!addresses.Items.Contains(preferences.Host))addresses.Items.Add(preferences.Host);
            addresses.SelectedItem=preferences.Host;consent.Checked=true;
            StartServer(this,EventArgs.Empty);
        } else if(startupTicks>=60) {
            startupTimer.Stop();AddLog(L.T("자동 서버 시작 보류: 저장한 IP가 없습니다. 창에서 연결 주소를 확인하세요."));
            if(tray!=null)tray.ShowBalloonTip(4000,"WinHomeDesk",L.T("저장한 IP를 찾지 못해 서버를 시작하지 않았습니다. 트레이에서 창을 열어 확인하세요."),ToolTipIcon.Warning);
        }
    }
    static bool HasLocalAddress(string host) {
        if(host=="127.0.0.1")return true;
        foreach(var network in System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces()) {
            if(network.OperationalStatus!=System.Net.NetworkInformation.OperationalStatus.Up)continue;
            foreach(var address in network.GetIPProperties().UnicastAddresses)if(address.Address.ToString()==host)return true;
        }
        return false;
    }
    void ShowOptions() {
        if(testMode)return;
        try {using(var dialog=CreateOptionsDialog(new ManagedStartupRegistration()))dialog.ShowDialog(this);}
        catch(Exception ex){AddLog("Options error: "+ex.GetType().Name+" HRESULT=0x"+ex.HResult.ToString("X8")+" "+ex.StackTrace);MessageBox.Show(this,L.T("환경 설정을 열지 못했습니다.\n")+ex.Message);}
    }
    Form CreateOptionsDialog(IStartupRegistration registration) {
        string startupCommand=registration.Read();
        bool loginEnabled=startupCommand!=null;
        var dialog=new Form {Text=L.T("WinHomeDesk — 환경 설정"),Icon=Icon,ClientSize=new Size(590,585),Font=Font,
            FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false,StartPosition=FormStartPosition.CenterParent};
            dialog.Controls.Add(new Label {Text=L.T("창의 X 버튼은 트레이로 숨깁니다. 완전 종료는 트레이 메뉴에서 합니다."),AutoSize=true,Location=new Point(18,18)});
            var login=new CheckBox {Name="LoginStartup",Text=L.T("Windows 로그인 시 트레이에서 자동 실행"),AutoSize=true,Location=new Point(20,61)};
            var server=new CheckBox {Name="ServerStartup",Text=L.T("로그인 시 원격 서버도 시작 (현재 설정·암호 저장)"),AutoSize=true,Location=new Point(20,99)};
            var admin=new CheckBox {Name="ElevatedStartup",Text=L.T("관리자 권한으로 자동 실행 (작업 스케줄러 / 저장 시 UAC 승인)"),AutoSize=true,Location=new Point(20,131)};
            admin.Checked=StartupTask.IsTask(startupCommand);admin.Enabled=loginEnabled;dialog.Controls.Add(admin);
            login.Checked=loginEnabled;
            server.Checked=login.Checked&&preferences.StartServerAtLogin;server.Enabled=login.Checked;
            login.CheckedChanged+=delegate{server.Enabled=login.Checked;admin.Enabled=login.Checked;if(!login.Checked){server.Checked=false;admin.Checked=false;}};
            dialog.Controls.Add(login);dialog.Controls.Add(server);
            dialog.Controls.Add(new Label {Text=L.T("서버 자동 시작을 선택하면 현재 창의 주소·화질·FPS·캡처 방식과\n전용 암호를 저장합니다. 암호는 현재 Windows 계정으로 암호화됩니다.\n관리자 자동 실행은 같은 관리자 계정의 다음 로그인부터 적용됩니다.\n현재 권한은 바뀌지 않습니다. 로그인 전 접속은 지원하지 않습니다."),Location=new Point(20,177),Size=new Size(550,77),Font=new Font(Font.FontFamily,9)});
            var rightAlt=new CheckBox {Name="RightAltHangul",Text=L.T("오른쪽 Alt를 한영키로 사용"),Checked=preferences.RightAltHangul,AutoSize=true,Location=new Point(20,274)};
            dialog.Controls.Add(rightAlt);
            dialog.Controls.Add(new Label {Text=L.T("기본 켜짐 · 변경 후 서버를 다시 시작하세요.\n일반 오른쪽 Alt / AltGr가 필요하면 해제하세요."),Location=new Point(20,304),Size=new Size(550,42),Font=new Font(Font.FontFamily,9)});
            string clientPath=preferences.RdcManPath;
            dialog.Controls.Add(new Label {Text=L.T("접속용 RDCMan 3.22 · 한글 입력 보정 포함"),AutoSize=true,Location=new Point(20,351)});
            var pathLabel=new Label {Name="RdcManPath",Text=String.IsNullOrEmpty(clientPath)?L.T("처음 실행할 때 RDCMan.exe를 선택하세요."):clientPath,AutoEllipsis=true,Location=new Point(20,384),Size=new Size(397,30)};
            dialog.Controls.Add(pathLabel);
            var chooseClient=new Button {Text=L.T("경로 변경"),Location=new Point(430,377),Size=new Size(140,32)};
            chooseClient.Click+=delegate{try{string selected=RdcManClient.SelectExecutable(dialog,clientPath);if(selected!=null){clientPath=selected;pathLabel.Text=selected;}}catch(Exception ex){MessageBox.Show(dialog,ex.Message);}};
            dialog.Controls.Add(chooseClient);
            var clientLog=new Button {Text=L.T("한글 보정 로그 열기"),Location=new Point(20,422),Size=new Size(190,32)};
            clientLog.Click+=delegate{OpenClientLog();};dialog.Controls.Add(clientLog);
            var clipboard=new CheckBox {Name="ClipboardEnabled",Text=L.T("텍스트·파일 복사/붙여넣기 허용 (서버 재시작 후 적용)"),Checked=preferences.ClipboardEnabled,AutoSize=true,Location=new Point(20,465)};
            dialog.Controls.Add(clipboard);
            dialog.Controls.Add(new Label {Text=L.T("모니터별 연결에서는 첫 번째 모니터 연결을 유지하세요."),Location=new Point(20,497),Size=new Size(550,30)});
            var save=new Button {Text=L.T("설정 저장"),Location=new Point(340,540),Size=new Size(110,32)};
            var cancel=new Button {Text=L.T("취소"),DialogResult=DialogResult.Cancel,Location=new Point(462,540),Size=new Size(110,32)};
            save.Click+=delegate {
                try {
                    var next=CurrentOptions(server.Checked);next.StartupElevated=login.Checked&&admin.Checked;next.RightAltHangul=rightAlt.Checked;next.ClipboardEnabled=clipboard.Checked;next.RdcManPath=clientPath;
                    DesktopOptionsCommit.Save(preferenceStore,registration,next,login.Checked,Application.ExecutablePath);
                    preferences=next;AddLog(L.T("자동 시작 권한: ")+(next.StartupElevated?L.T("관리자 (다음 로그인부터)"):L.T("일반"))+L.T(" / 현재 권한: ")+(StartupTask.IsElevated?L.T("관리자"):L.T("일반")));if(!next.StartServerAtLogin)startupTimer.Stop();AddLog(L.T("환경 설정 저장: 로그인 자동 실행 ")+(login.Checked?L.T("켜짐"):L.T("꺼짐"))+L.T(", 서버 자동 시작 ")+(server.Checked?L.T("켜짐"):L.T("꺼짐")));
                    AddLog(L.T("오른쪽 Alt 한영 변환: ")+(next.RightAltHangul?L.T("켜짐"):L.T("꺼짐"))+L.T(" — 서버 재시작 후 적용"));
                    dialog.DialogResult=DialogResult.OK;dialog.Close();
                }catch(Exception ex){MessageBox.Show(dialog,L.T("설정을 저장하지 못했습니다.\n")+ex.Message);}
            };
            dialog.Controls.Add(save);dialog.Controls.Add(cancel);dialog.AcceptButton=save;dialog.CancelButton=cancel;
        return dialog;
    }
}
