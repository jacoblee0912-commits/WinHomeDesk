using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

static class Launcher {
    static string Quote(string s){return "\""+s.Replace("\"","\\\"")+"\"";}
    [STAThread] static int Main(string[] args) {
        Application.EnableVisualStyles();
        try {
            string dir=Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string hook=Path.Combine(dir,"WinHomeDesk.ImeHook.dll");
            if(!File.Exists(hook))throw new Exception("ZIP 전체를 풀어 EXE와 WinHomeDesk.ImeHook.dll을 같은 폴더에 두세요.");
            string settings=Path.Combine(dir,"RDCMan-path.txt");
            string target=null;int first=0;
            if(args.Length>=2 && args[0]=="--rdcman"){target=Path.GetFullPath(args[1]);first=2;}
            if(target==null && File.Exists(settings))target=File.ReadAllText(settings).Trim();
            if(target==null && File.Exists(Path.Combine(dir,"RDCMan.exe")))target=Path.Combine(dir,"RDCMan.exe");
            if(target==null || !File.Exists(target)) {
                using(var picker=new OpenFileDialog {Title="사용 중인 RDCMan 3.22의 RDCMan.exe 선택",Filter="RDCMan.exe|RDCMan.exe",CheckFileExists=true}){
                    if(picker.ShowDialog()!=DialogResult.OK)return 1;
                    target=picker.FileName;
                }
            }
            var version=FileVersionInfo.GetVersionInfo(target);
            if(!String.Equals(Path.GetFileName(target),"RDCMan.exe",StringComparison.OrdinalIgnoreCase) || version.FileMajorPart!=3 || version.FileMinorPart!=22)
                throw new Exception("이 보정 도구는 RDCMan 3.22 전용입니다. 해당 버전의 RDCMan.exe를 선택하세요.");
            foreach(var existing in Process.GetProcessesByName("RDCMan")) {
                using(existing) {
                    try {if(String.Equals(existing.MainModule.FileName,target,StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("실행 중인 RDCMan 창을 모두 닫은 뒤 이 파일을 다시 실행하세요. 연결을 자동으로 종료하지 않습니다.");}
                    catch(System.ComponentModel.Win32Exception){throw new Exception("실행 중인 RDCMan을 확인하지 못했습니다. RDCMan을 종료한 뒤 다시 실행하세요.");}
                }
            }
            string forwarded="";
            for(int i=first;i<args.Length;i++) {
                if(!File.Exists(args[i]) || !String.Equals(Path.GetExtension(args[i]),".rdg",StringComparison.OrdinalIgnoreCase))throw new Exception("추가 인수는 열려는 RDG 파일만 지원합니다.");
                forwarded+=" "+Quote(Path.GetFullPath(args[i]));
            }
            // Only this launched process inherits the hook. No system environment/registry changes.
            var start=new ProcessStartInfo(target,"-noconnect"+forwarded) {UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(target)};
            string previous=start.EnvironmentVariables["DOTNET_STARTUP_HOOKS"];
            start.EnvironmentVariables["WINHOME_DESK_PREVIOUS_HOOKS"]=previous??"";
            start.EnvironmentVariables["DOTNET_STARTUP_HOOKS"]=String.IsNullOrEmpty(previous)?hook:previous+Path.PathSeparator+hook;
            start.EnvironmentVariables["WINHOME_DESK_IME_LOG"]=Path.Combine(dir,"WinHomeDesk-IME.log");
            File.WriteAllText(settings,target);
            Process.Start(start);
            return 0;
        }catch(Exception e){MessageBox.Show(e.Message,"RDCMan 3.22 한글 입력",MessageBoxButtons.OK,MessageBoxIcon.Information);return 1;}
    }
}
