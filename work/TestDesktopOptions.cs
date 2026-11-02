using System;using System.IO;using System.Security.Cryptography;
class FakeStartup : IStartupRegistration {
    public string Value;public bool Fail;
    public string Read(){return Value;}
    public void Write(string value){if(Fail){Fail=false;throw new IOException("test write failure");}Value=value;}
}
class TestDesktopOptions {
    static void Check(bool ok,string msg){if(!ok)throw new Exception(msg);}
    static void Reject(Action a,string msg){try{a();}catch{return;}throw new Exception(msg);}
    static void Main(string[] args) {
        string dir=Path.Combine(args[0],"work","options-tests-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(dir);
        string path=Path.Combine(dir,"preferences.xml");var store=new DesktopOptionsStore(path);
        Check(!store.Load().StartServerAtLogin,"default must be off");
        Check(store.Load().RightAltHangul,"right Alt compatibility default");
        File.WriteAllText(path,"<DesktopOptions><Version>1</Version></DesktopOptions>");
        Check(store.Load().RightAltHangul,"legacy settings default");
        Check(store.Load().RdcManPath=="","legacy settings client path default");
        Check(!store.Load().StartupElevated,"legacy settings do not opt into elevation");
        store.Save(new DesktopOptions {StartupElevated=true});Check(store.Load().StartupElevated,"elevated startup preference roundtrip");
        var clientOptions=new DesktopOptions {RdcManPath=Path.Combine(dir,"RDCMan.exe")};store.Save(clientOptions);
        Check(store.Load().RdcManPath==clientOptions.RdcManPath,"RDCMan path roundtrip");
        var keyboardOptions=new DesktopOptions {RightAltHangul=false};store.Save(keyboardOptions);
        Check(!store.Load().RightAltHangul,"right Alt disabled roundtrip");
        var p=new DesktopOptions {Host="192.168.0.110",Quality=1,FrameRate=2,CaptureMode="auto",StartServerAtLogin=true};
        string secret="test-only-"+Guid.NewGuid().ToString("N");p.ProtectPassword(secret);store.Save(p);
        Check(!File.ReadAllText(path).Contains(secret),"plain password must not persist");
        var loaded=store.Load();Check(loaded.ReadPassword()==secret && loaded.Host==p.Host && loaded.FrameRate==2,"round trip");
        var fake=new FakeStartup();string exe=Path.Combine(dir,"folder with spaces","SurfaceDesk.exe");
        DesktopOptionsCommit.Save(store,fake,p,true,exe);Check(fake.Value=="\""+exe+"\" --startup","quoted command");
        fake.Fail=true;Reject(delegate{DesktopOptionsCommit.Save(store,fake,new DesktopOptions {Host="127.0.0.1"},false,exe);},"registration error");
        Check(store.Load().StartServerAtLogin && fake.Value!=null,"rollback");
        DesktopOptionsCommit.Save(store,fake,new DesktopOptions {Host="127.0.0.1"},false,exe);
        Check(fake.Value==null && store.Load().EncryptedPassword=="","disable removes credential and startup entry");
        Reject(delegate{UserStartupRegistration.Command("relative.exe");},"relative startup path rejected");
        p.Port=1;Reject(delegate{store.Save(p);},"invalid port rejected");
        File.WriteAllText(path,"<!DOCTYPE x [<!ENTITY y SYSTEM 'file:///dummy'>]><DesktopOptions>&y;</DesktopOptions>");
        Reject(delegate{store.Load();},"DTD rejected");
        Console.WriteLine("Settings: encrypted roundtrip, opt-in default, command quoting, rollback, disable, validation PASS (fake startup registry only)");
    }
}
