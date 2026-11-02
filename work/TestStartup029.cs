using System;using System.IO;using System.Reflection;using System.Runtime.InteropServices;using System.Windows.Forms;using System.Drawing;
class MemoryStartup : IStartupBackend {
 public string Task,Run,Legacy; public int Writes,FailAt;
 void Tick(){Writes++;if(Writes==FailAt)throw new IOException("injected failure");}
 public string ReadTask(){return Task;}public void WriteTask(string x){Tick();Task=x;}
 public string ReadRun(string name){return name=="WinHomeDesk"?Run:Legacy;}
 public void WriteRun(string name,string value){Tick();if(name=="WinHomeDesk")Run=value;else Legacy=value;}
}
class AtomicFailure : IAtomicStartupRegistration {
 public int Calls;
 public string Read(){return null;}
 public void Write(string x){Calls++;throw new IOException("UAC cancelled / no mutation");}
}
class PreviewAdmin : IStartupRegistration {
 public string Read(){return StartupTask.Command(Application.ExecutablePath,true);}
 public void Write(string value){throw new Exception("UI preview must not register tasks");}
}
class TestStartup029 {
 static void Check(bool ok,string message){if(!ok)throw new Exception(message);Console.WriteLine("PASS "+message);}
 static void Reject(Action action){bool caught=false;try{action();}catch{caught=true;}if(!caught)throw new Exception("Expected rejection");}
 [STAThread] static void Main(string[] args) {try{Run(args);}catch(Exception ex){Console.WriteLine("FAIL type="+ex.GetType().FullName+" message="+ex.Message+" hresult="+ex.HResult.ToString("X"));Environment.ExitCode=1;}}
 static void Run(string[] args) {
  string sid=StartupTask.CurrentSid,exe=Path.Combine(Path.GetFullPath(args[0]),"한글 & space","WinHomeDesk.exe");
  string normal=StartupTask.Command(exe,false),xml=StartupTask.Xml(exe,sid);
  Check(StartupTask.ReadCommand(xml,sid)==StartupTask.Command(exe,true),"XML roundtrip with Korean, ampersand and spaces");
  Reject(delegate{StartupTask.ReadCommand(xml,"S-1-5-21-1-2-3-999");});
  Reject(delegate{StartupTask.ReadCommand(xml.Replace("--startup","--unexpected"),sid);});
  Reject(delegate{StartupTask.Xml("relative.exe",sid);});Console.WriteLine("PASS foreign SID / unexpected action / relative path rejected");
  var b=new MemoryStartup {Run="old-run",Legacy="legacy-run"};
  StartupTask.Apply(b,"admin",exe,sid);Check(b.Task==xml&&b.Run==null&&b.Legacy==null,"normal to admin removes both Run entries");
  StartupTask.Apply(b,"normal",exe,sid);Check(b.Task==null&&b.Run==normal&&b.Legacy==null,"admin to normal removes task");
  StartupTask.Apply(b,"admin",exe,sid);StartupTask.Apply(b,"off",exe,sid);Check(b.Task==null&&b.Run==null&&b.Legacy==null,"off removes task and Run entries");
  foreach(string mode in new[]{"admin","normal","off"})for(int fail=1;fail<=3;fail++) {
   string beforeTask=mode=="admin"?null:xml;
   b=new MemoryStartup {Task=beforeTask,Run="old-run",Legacy="legacy-run",FailAt=fail};
   Reject(delegate{StartupTask.Apply(b,mode,exe,sid);});
   Check(b.Task==beforeTask&&b.Run=="old-run"&&b.Legacy=="legacy-run","rollback "+mode+" mutation "+fail);
  }
  string dir=Path.Combine(args[0],"startup029-test");Directory.CreateDirectory(dir);var store=new DesktopOptionsStore(Path.Combine(dir,"settings.xml"));store.Save(new DesktopOptions());
  var atomic=new AtomicFailure();Reject(delegate{DesktopOptionsCommit.Save(store,atomic,new DesktopOptions {StartupElevated=true},true,exe);});
  Check(atomic.Calls==1&&!store.Load().StartupElevated,"cancel restores preferences without repeating elevation");
  // Windows Task Scheduler parses the real schema, but no task is registered or run.
  Console.WriteLine("COM creating service");object service=null,definition=null;
  try {service=Activator.CreateInstance(Type.GetTypeFromProgID("Schedule.Service",true));((dynamic)service).Connect();definition=((dynamic)service).NewTask(0);((dynamic)definition).XmlText=xml;
   Check(((dynamic)definition).Principal.RunLevel==1&&((dynamic)definition).Principal.LogonType==3,"Windows COM validates highest interactive token task XML");
   Check(((dynamic)definition).Settings.DisallowStartIfOnBatteries==false&&((dynamic)definition).Settings.StopIfGoingOnBatteries==false&&((dynamic)definition).Settings.ExecutionTimeLimit=="PT0S","battery use allowed and no timed shutdown");
  }catch(UnauthorizedAccessException){Console.WriteLine("SKIP real Task Scheduler COM validation: sandbox access denied (0x80070005)");}
  finally {if(definition!=null)Marshal.FinalReleaseComObject(definition);if(service!=null)Marshal.FinalReleaseComObject(service);}
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  using(var f=new SurfaceDesk(false,true))using(var d=(Form)typeof(SurfaceDesk).GetMethod("CreateOptionsDialog",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(f,new object[]{new PreviewAdmin()})) {
   d.Show();Application.DoEvents();var login=(CheckBox)d.Controls["LoginStartup"];var admin=(CheckBox)d.Controls["ElevatedStartup"];var server=(CheckBox)d.Controls["ServerStartup"];
   Check(login.Checked&&admin.Checked&&admin.Enabled,"existing elevated task reflected in options");
   using(var bmp=new Bitmap(d.Width,d.Height)){d.DrawToBitmap(bmp,new Rectangle(Point.Empty,d.Size));bmp.Save(Path.Combine(args[0],"preview-options029.png"));}
   login.Checked=false;Check(!admin.Checked&&!admin.Enabled&&!server.Checked&&!server.Enabled,"disable login clears dependent options");
   login.Checked=true;Check(admin.Enabled&&!admin.Checked&&server.Enabled,"elevation remains opt-in when re-enabling login");
   d.Close();f.ExitApplication();
  }
  Console.WriteLine("No Task Scheduler registration, startup registry mutation, UAC prompt or logout performed.");
 }
}
