using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Windows.Forms;
using System.Xml;
using Microsoft.Win32;

// Write either commits both registrations or restores its own snapshot.
public interface IAtomicStartupRegistration : IStartupRegistration {}
public interface IStartupBackend {
    string ReadTask();
    void WriteTask(string xml);
    string ReadRun(string name);
    void WriteRun(string name,string command);
}
public static class StartupTask {
    public const string Prefix="task-highest:";
    public const string Marker="WinHomeDesk interactive startup v1";
    public const string Ns="http://schemas.microsoft.com/windows/2004/02/mit/task";
    public static bool IsElevated {get {return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);}}
    public static string CurrentSid {get {return WindowsIdentity.GetCurrent().User.Value;}}
    public static bool IsTask(string command){return command!=null && command.StartsWith(Prefix,StringComparison.Ordinal);}
    public static string Command(string executable,bool elevated){return (elevated?Prefix:"")+UserStartupRegistration.Command(executable);}
    static XmlElement E(XmlNode parent,string name,string value) {
        var d=parent as XmlDocument??parent.OwnerDocument;var e=d.CreateElement(name,Ns);parent.AppendChild(e);if(value!=null)e.InnerText=value;return e;
    }
    public static string Xml(string executable,string sid) {
        UserStartupRegistration.Command(executable);new SecurityIdentifier(sid);
        var d=new XmlDocument();var root=E(d,"Task",null);root.SetAttribute("version","1.2");
        var info=E(root,"RegistrationInfo",null);E(info,"Description",Marker);
        var triggers=E(root,"Triggers",null);var logon=E(triggers,"LogonTrigger",null);
        E(logon,"Enabled","true");E(logon,"UserId",sid);E(logon,"Delay","PT5S");
        var principals=E(root,"Principals",null);var principal=E(principals,"Principal",null);principal.SetAttribute("id","CurrentUser");
        E(principal,"UserId",sid);E(principal,"LogonType","InteractiveToken");E(principal,"RunLevel","HighestAvailable");
        var settings=E(root,"Settings",null);
        E(settings,"MultipleInstancesPolicy","IgnoreNew");E(settings,"DisallowStartIfOnBatteries","false");E(settings,"StopIfGoingOnBatteries","false");
        E(settings,"StartWhenAvailable","true");E(settings,"Enabled","true");E(settings,"ExecutionTimeLimit","PT0S");
        var actions=E(root,"Actions",null);actions.SetAttribute("Context","CurrentUser");var exec=E(actions,"Exec",null);
        E(exec,"Command",executable);E(exec,"Arguments","--startup");E(exec,"WorkingDirectory",Path.GetDirectoryName(executable));
        return d.OuterXml;
    }
    public static string ReadCommand(string xml,string sid) {
        if(xml==null)return null;
        var d=new XmlDocument {XmlResolver=null};
        using(var r=XmlReader.Create(new StringReader(xml),new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null}))d.Load(r);
        var n=new XmlNamespaceManager(d.NameTable);n.AddNamespace("t",Ns);
        Func<string,string> value=delegate(string path){var node=d.SelectSingleNode("/t:Task/"+path,n);return node==null?"":node.InnerText;};
        if(value("t:RegistrationInfo/t:Description")!=Marker || value("t:Principals/t:Principal/t:UserId")!=sid ||
           value("t:Principals/t:Principal/t:LogonType")!="InteractiveToken" || value("t:Principals/t:Principal/t:RunLevel")!="HighestAvailable" ||
           value("t:Actions/t:Exec/t:Arguments")!="--startup" || d.SelectNodes("/t:Task/t:Actions/*",n).Count!=1)
            throw new InvalidOperationException(L.T("동일한 이름의 예약 작업이 WinHomeDesk 설정과 다릅니다. 작업 스케줄러에서 확인하세요."));
        // Disabled registrations still count, so disabling the option can remove them.
        return Command(value("t:Actions/t:Exec/t:Command"),true);
    }
    public static void Apply(IStartupBackend backend,string mode,string executable,string sid) {
        if(mode!="off" && mode!="normal" && mode!="admin")throw new ArgumentException(L.T("자동 시작 방식이 올바르지 않습니다."));
        string task=backend.ReadTask(),run=backend.ReadRun("WinHomeDesk"),legacy=backend.ReadRun("SurfaceDesk");
        if(task!=null)ReadCommand(task,sid);
        string nextTask=mode=="admin"?Xml(executable,sid):null;
        string nextRun=mode=="normal"?UserStartupRegistration.Command(executable):null;
        try {
            if(task!=nextTask)backend.WriteTask(nextTask);
            if(run!=nextRun)backend.WriteRun("WinHomeDesk",nextRun);
            if(legacy!=null)backend.WriteRun("SurfaceDesk",null);
        }catch(Exception failure) {
            Exception rollback=null;
            try{if(backend.ReadTask()!=task)backend.WriteTask(task);}catch(Exception ex){rollback=ex;}
            try{if(backend.ReadRun("WinHomeDesk")!=run)backend.WriteRun("WinHomeDesk",run);}catch(Exception ex){rollback=ex;}
            try{if(backend.ReadRun("SurfaceDesk")!=legacy)backend.WriteRun("SurfaceDesk",legacy);}catch(Exception ex){rollback=ex;}
            if(rollback!=null)throw new InvalidOperationException(L.T("자동 시작 변경 및 복원이 실패했습니다. 작업 스케줄러와 시작 프로그램 설정을 확인하세요. ")+rollback.Message,failure);
            throw;
        }
    }
}
public sealed class WindowsStartupBackend : IStartupBackend,IDisposable {
    readonly string sid;
    object service,folder;
    public string TaskName {get{return "WinHomeDesk-"+sid;}}
    public WindowsStartupBackend(string sid){this.sid=sid;new SecurityIdentifier(sid);}
    dynamic Folder {
        get {
            if(folder==null){service=Activator.CreateInstance(Type.GetTypeFromProgID("Schedule.Service",true));((dynamic)service).Connect();folder=((dynamic)service).GetFolder("\\");}
            return folder;
        }
    }
    public string ReadTask() {
        object task=null;
        try {task=Folder.GetTask(TaskName);return (string)((dynamic)task).Xml;}
        catch(COMException ex){if(ex.ErrorCode==unchecked((int)0x80070002))return null;throw;}
        finally{if(task!=null)Marshal.FinalReleaseComObject(task);}
    }
    public void WriteTask(string xml) {
        if(xml==null){if(ReadTask()!=null)Folder.DeleteTask(TaskName,0);return;}
        StartupTask.ReadCommand(xml,sid);
        // SYSTEM/admins can maintain the task; the user can read and run it.
        string sddl="D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;"+sid+")";
        object registered=Folder.RegisterTask(TaskName,xml,6,sid,null,3,sddl);
        if(registered!=null)Marshal.FinalReleaseComObject(registered);
    }
    const string RunKey=@"Software\Microsoft\Windows\CurrentVersion\Run";
    public string ReadRun(string name){using(var key=Registry.CurrentUser.OpenSubKey(RunKey))return key==null?null:key.GetValue(name) as string;}
    public void WriteRun(string name,string command) {
        if(name!="WinHomeDesk" && name!="SurfaceDesk")throw new ArgumentException("Unknown startup entry");
        using(var key=Registry.CurrentUser.CreateSubKey(RunKey)){if(command==null)key.DeleteValue(name,false);else key.SetValue(name,command,RegistryValueKind.String);}
    }
    public void Dispose(){if(folder!=null){Marshal.FinalReleaseComObject(folder);folder=null;}if(service!=null){Marshal.FinalReleaseComObject(service);service=null;}}
}
public sealed class ManagedStartupRegistration : IAtomicStartupRegistration {
    public string Read() {
        using(var b=new WindowsStartupBackend(StartupTask.CurrentSid)) {
            string xml=b.ReadTask();return xml!=null?StartupTask.ReadCommand(xml,StartupTask.CurrentSid):b.ReadRun("WinHomeDesk")??b.ReadRun("SurfaceDesk");
        }
    }
    public void Write(string command) {
        string exe=Application.ExecutablePath,sid=StartupTask.CurrentSid;
        bool elevated=StartupTask.IsTask(command);string mode=command==null?"off":elevated?"admin":"normal";
        if(command!=null && command!=StartupTask.Command(exe,elevated))throw new InvalidOperationException(L.T("현재 실행 중인 WinHomeDesk 경로만 자동 시작에 등록할 수 있습니다."));
        using(var b=new WindowsStartupBackend(sid)) {
            string xml=b.ReadTask();
            if(xml!=null)StartupTask.ReadCommand(xml,sid);
            if((xml!=null || elevated) && !StartupTask.IsElevated) {
                // Same executable, current SID and a fixed mode only; no scripts or imported task XML.
                var start=new ProcessStartInfo(exe,"--configure-startup "+mode+" "+sid) {UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Hidden};
                try {using(var p=Process.Start(start)){if(p==null)throw new IOException(L.T("관리자 설정 도구를 실행하지 못했습니다."));p.WaitForExit();if(p.ExitCode!=0)throw new InvalidOperationException(L.T("관리자 자동 시작을 저장하지 못했습니다. 관리자 설정 도구에 표시된 원인을 확인하세요."));}}
                catch(Win32Exception ex){if(ex.NativeErrorCode==1223)throw new InvalidOperationException(L.T("UAC 승인이 취소되어 기존 자동 시작 설정을 유지합니다."),ex);throw;}
                return;
            }
            StartupTask.Apply(b,mode,exe,sid);
        }
    }
    public static bool HandleHelper(string[] args) {
        if(args.Length==0 || args[0]!="--configure-startup")return false;
        try {
            if(args.Length!=3 || args[2]!=StartupTask.CurrentSid || !StartupTask.IsElevated)
                throw new InvalidOperationException(L.T("현재 로그인한 관리자 계정으로 UAC를 승인해야 합니다. 다른 관리자 계정의 암호로 등록할 수 없습니다."));
            using(var b=new WindowsStartupBackend(StartupTask.CurrentSid))StartupTask.Apply(b,args[1],Application.ExecutablePath,StartupTask.CurrentSid);
            Environment.ExitCode=0;
        }catch(Exception ex){MessageBox.Show(L.T("자동 시작 설정 실패\n")+ex.Message,"WinHomeDesk",MessageBoxButtons.OK,MessageBoxIcon.Error);Environment.ExitCode=1;}
        return true;
    }
}
