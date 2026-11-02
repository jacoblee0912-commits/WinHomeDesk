using System;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using System.Xml;
using System.Xml.Serialization;
using Microsoft.Win32;

public sealed class DesktopOptions {
    public int Version=1, Port=3390, Quality=0, FrameRate=0, GraphicsMode=0;
    public string Host="", CaptureMode="auto", EncoderMode="auto", EncryptedPassword="";
    public bool StartServerAtLogin=false, AudioEnabled=true;
    public bool RightAltHangul=true;
    public bool StartupElevated=false;
    public string Language="ko";
    public string RdcManPath="";
    static readonly byte[] entropy=Encoding.UTF8.GetBytes("SurfaceDesk/preferences/v1");
    public void ProtectPassword(string password) {
        byte[] bytes=Encoding.UTF8.GetBytes(password);
        try {EncryptedPassword=Convert.ToBase64String(ProtectedData.Protect(bytes,entropy,DataProtectionScope.CurrentUser));}
        finally {Array.Clear(bytes,0,bytes.Length);}
    }
    public string ReadPassword() {
        if(String.IsNullOrEmpty(EncryptedPassword))return "";
        byte[] bytes=ProtectedData.Unprotect(Convert.FromBase64String(EncryptedPassword),entropy,DataProtectionScope.CurrentUser);
        try{return Encoding.UTF8.GetString(bytes);}finally{Array.Clear(bytes,0,bytes.Length);}
    }
    public void Validate() {
        if(Language!="ko" && Language!="en")throw new InvalidDataException("Invalid UI language.");
        if(RdcManPath==null || RdcManPath.Length>32767 || (RdcManPath.Length>0 && (!Path.IsPathRooted(RdcManPath) || RdcManPath.IndexOf('"')>=0)))
            throw new InvalidDataException("저장된 RDCMan 경로가 올바르지 않습니다.");
        if((EncoderMode!="auto" && EncoderMode!="cpu") || GraphicsMode<0 || GraphicsMode>3 || Version!=1 || Port<1024 || Port>65535 || Quality<0 || Quality>2 || FrameRate<0 || FrameRate>2 ||
           (CaptureMode!="auto" && CaptureMode!="gdi") || Host==null || Host.Length>64 ||
           EncryptedPassword==null || EncryptedPassword.Length>16384)
            throw new InvalidDataException("저장된 설정 값이 올바르지 않습니다.");
        System.Net.IPAddress ip;
        if(Host.Length>0 && (!System.Net.IPAddress.TryParse(Host,out ip) || ip.AddressFamily!=System.Net.Sockets.AddressFamily.InterNetwork))
            throw new InvalidDataException("저장된 연결 주소가 올바르지 않습니다.");
        if(StartServerAtLogin && (Host.Length==0 || EncryptedPassword.Length==0))
            throw new InvalidDataException("자동 서버 시작에 필요한 주소 또는 암호가 없습니다.");
    }
}
public sealed class DesktopOptionsStore {
    readonly string path;
    public DesktopOptionsStore(string path){this.path=path;}
    public DesktopOptions Load() {
        if(!File.Exists(path))return new DesktopOptions();
        if(new FileInfo(path).Length>65536)throw new InvalidDataException("설정 파일이 너무 큽니다.");
        using(var reader=XmlReader.Create(path,new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null})) {
            var options=(DesktopOptions)new XmlSerializer(typeof(DesktopOptions)).Deserialize(reader);
            options.Validate();return options;
        }
    }
    public void Save(DesktopOptions options) {
        options.Validate();Directory.CreateDirectory(Path.GetDirectoryName(path));
        string temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            using(var writer=XmlWriter.Create(temporary,new XmlWriterSettings {Encoding=new UTF8Encoding(false),Indent=true}))
                new XmlSerializer(typeof(DesktopOptions)).Serialize(writer,options);
            if(File.Exists(path))File.Replace(temporary,path,null);else File.Move(temporary,path);
        }finally{if(File.Exists(temporary))File.Delete(temporary);}
    }
}
public interface IStartupRegistration {
    string Read();
    void Write(string command);
}
public sealed class UserStartupRegistration : IStartupRegistration {
    const string keyPath=@"Software\Microsoft\Windows\CurrentVersion\Run";
    const string valueName="WinHomeDesk";
    const string legacyValueName="SurfaceDesk";
    public string Read(){using(var key=Registry.CurrentUser.OpenSubKey(keyPath))return key==null?null:(key.GetValue(valueName) as string ?? key.GetValue(legacyValueName) as string);}
    public void Write(string command) {
        if(command==null){using(var key=Registry.CurrentUser.OpenSubKey(keyPath,true)){if(key!=null){key.DeleteValue(valueName,false);key.DeleteValue(legacyValueName,false);}}return;}
        using(var key=Registry.CurrentUser.CreateSubKey(keyPath)){key.SetValue(valueName,command,RegistryValueKind.String);key.DeleteValue(legacyValueName,false);}
    }
    public static string Command(string executable) {
        if(String.IsNullOrWhiteSpace(executable) || !Path.IsPathRooted(executable) || executable.IndexOf('"')>=0)
            throw new ArgumentException(L.T("프로그램 경로가 올바르지 않습니다."));
        string result="\""+executable+"\" --startup";
        if(result.Length>260)throw new ArgumentException(L.T("자동 시작에 사용할 경로가 너무 깁니다. 더 짧은 폴더로 옮겨주세요."));
        return result;
    }
}
public static class DesktopOptionsCommit {
    // Persist credentials first; if registration fails restore both previous values.
    public static void Save(DesktopOptionsStore store,IStartupRegistration registration,
                            DesktopOptions next,bool startup,string executable) {
        string command=startup?StartupTask.Command(executable,next.StartupElevated):null;
        DesktopOptions before=store.Load();string beforeCommand=registration.Read();
        store.Save(next);
        try{registration.Write(command);}
        catch {
            store.Save(before);
            if(!(registration is IAtomicStartupRegistration)){try{registration.Write(beforeCommand);}catch{}}
            throw;
        }
    }
}
