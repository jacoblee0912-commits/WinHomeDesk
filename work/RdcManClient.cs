using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

public static class RdcManClient {
    public const string HookResource="WinHomeDesk.ImeHook.dll";
    public static void ValidateExecutable(string path) {
        if(String.IsNullOrWhiteSpace(path) || !Path.IsPathRooted(path) || !File.Exists(path))
            throw new FileNotFoundException(L.T("RDCMan 3.22 실행 파일을 선택하세요."));
        var version=FileVersionInfo.GetVersionInfo(path);
        if(!String.Equals(Path.GetFileName(path),"RDCMan.exe",StringComparison.OrdinalIgnoreCase) || version.FileMajorPart!=3 || version.FileMinorPart!=22)
            throw new InvalidOperationException(L.T("한글 입력 보정은 RDCMan 3.22용입니다. 해당 버전의 RDCMan.exe를 선택하세요."));
    }
    public static string SelectExecutable(IWin32Window owner,string previous) {
        using(var picker=new OpenFileDialog {Title=L.T("RDCMan 3.22의 RDCMan.exe 선택"),Filter="RDCMan.exe|RDCMan.exe",CheckFileExists=true}) {
            if(!String.IsNullOrEmpty(previous) && File.Exists(previous))picker.FileName=previous;
            if(picker.ShowDialog(owner)!=DialogResult.OK)return null;
            ValidateExecutable(picker.FileName);return picker.FileName;
        }
    }
    public static void CheckNotRunning(string target) {
        foreach(var existing in Process.GetProcessesByName("RDCMan"))using(existing) {
            try {
                if(String.Equals(existing.MainModule.FileName,target,StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException(L.T("실행 중인 RDCMan을 종료한 뒤 다시 눌러주세요. 한글 보정은 새로 실행할 때 적용됩니다."));
            }catch(System.ComponentModel.Win32Exception) {
                throw new InvalidOperationException(L.T("실행 중인 RDCMan을 확인하지 못했습니다. RDCMan을 종료한 뒤 다시 실행하세요."));
            }
        }
    }
    static string Hash(byte[] bytes) {
        using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();
    }
    public static byte[] HookBytes() {
        using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream(HookResource)) {
            if(resource==null)throw new InvalidOperationException(L.T("내장 한글 보정 모듈을 찾지 못했습니다. 배포 파일을 다시 풀어주세요."));
            using(var memory=new MemoryStream()){resource.CopyTo(memory);return memory.ToArray();}
        }
    }
    public static string ExtractHook(string clientData) {
        byte[] bytes=HookBytes();string hash=Hash(bytes);
        string folder=Path.Combine(clientData,"hooks",hash),path=Path.Combine(folder,HookResource);
        Directory.CreateDirectory(folder);
        if(File.Exists(path) && Hash(File.ReadAllBytes(path))==hash)return path;
        string temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            File.WriteAllBytes(temporary,bytes);
            if(File.Exists(path))File.Replace(temporary,path,null);else File.Move(temporary,path);
        }finally{if(File.Exists(temporary))File.Delete(temporary);}
        if(Hash(File.ReadAllBytes(path))!=hash)throw new IOException(L.T("한글 보정 모듈 확인에 실패했습니다."));
        return path;
    }
    public static ProcessStartInfo BuildStartInfo(string target,string hook,string log) {
        ValidateExecutable(target);
        var start=new ProcessStartInfo(target,"-noconnect") {UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(target)};
        string previous=start.EnvironmentVariables["DOTNET_STARTUP_HOOKS"];
        start.EnvironmentVariables["WINHOME_DESK_PREVIOUS_HOOKS"]=previous??"";
        start.EnvironmentVariables["DOTNET_STARTUP_HOOKS"]=String.IsNullOrEmpty(previous)?hook:previous+Path.PathSeparator+hook;
        start.EnvironmentVariables["WINHOME_DESK_IME_LOG"]=log;
        return start;
    }
    public static int Launch(string target,string clientData) {
        ValidateExecutable(target);CheckNotRunning(target);
        string hook=ExtractHook(clientData),log=Path.Combine(clientData,"WinHomeDesk-IME.log");
        using(var process=Process.Start(BuildStartInfo(target,hook,log))) {
            if(process==null)throw new IOException(L.T("RDCMan을 실행하지 못했습니다."));
            return process.Id;
        }
    }
}

public sealed partial class SurfaceDesk {
    static string ClientDataDirectory {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"WinHomeDesk","Client");}}
    void LaunchRdcManClient() {
        if(testMode)return;
        try {
            string target=preferences.RdcManPath;
            if(String.IsNullOrEmpty(target) || !File.Exists(target)) {
                target=RdcManClient.SelectExecutable(this,target);
                if(target==null)return;
                var next=CurrentOptions(preferences.StartServerAtLogin);next.RdcManPath=target;
                preferenceStore.Save(next);preferences=next;
            }
            int id=RdcManClient.Launch(target,ClientDataDirectory);
            AddLog(L.T("RDCMan 3.22 실행: 새 엔진 + 한글 입력 보정 (PID ")+id+")");
            AddLog(L.T("접속하는 PC에서는 서버를 시작하지 않아도 됩니다. RDCMan에서 기존 RDG를 열어 접속하세요."));
        }catch(Exception ex){MessageBox.Show(this,ex.Message,L.T("RDCMan 실행"),MessageBoxButtons.OK,MessageBoxIcon.Information);}
    }
    void OpenClientLog() {
        try {
            string path=Path.Combine(ClientDataDirectory,"WinHomeDesk-IME.log");
            if(!File.Exists(path)){MessageBox.Show(this,L.T("RDCMan을 한글 보정으로 실행한 뒤 로그를 확인할 수 있습니다."));return;}
            Process.Start(new ProcessStartInfo("notepad.exe","\""+path+"\"") {UseShellExecute=false});
        }catch(Exception ex){MessageBox.Show(this,ex.Message,L.T("보정 로그"));}
    }
}
