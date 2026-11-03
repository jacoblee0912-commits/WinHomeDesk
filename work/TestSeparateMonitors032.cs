using System;
using System.Collections;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
class TestSeparateMonitors032 {
 static BindingFlags flags=BindingFlags.NonPublic|BindingFlags.Instance;
 static T Field<T>(object o,string n){return (T)o.GetType().GetField(n,flags).GetValue(o);}
 static object Call(object o,string n,params object[] a){return o.GetType().GetMethod(n,flags).Invoke(o,a);}
 static void Check(bool b,string s){if(!b)throw new Exception(s);Console.WriteLine("PASS "+s);}
 static void Reject(Action a,string s){bool caught=false;try{a();}catch(InvalidDataException){caught=true;}Check(caught,s);}
 static void Pump(Func<bool> done,int timeout) {var end=DateTime.UtcNow.AddMilliseconds(timeout);while(!done() && DateTime.UtcNow<end){Application.DoEvents();Thread.Sleep(10);}Check(done(),"asynchronous session state");}
 static int[] Ids(SurfaceDesk f) {var sessions=Field<IList>(f,"monitorSessions");var ids=new int[sessions.Count];for(int i=0;i<ids.Length;i++)ids[i]=((Process)sessions[i].GetType().GetField("Process").GetValue(sessions[i])).Id;return ids;}
 static bool Alive(int id){try{using(var p=Process.GetProcessById(id))return !p.HasExited;}catch(ArgumentException){return false;}}
 [STAThread] static int Main(string[] args) {
  if(args.Length>0 && args[0].StartsWith("/bind-address:")) {
   int port=0;foreach(string arg in args)if(arg.StartsWith("/port:"))port=int.Parse(arg.Substring(6));
   var device=Environment.GetEnvironmentVariable("WINHOME_DESK_MONITOR_DEVICE");
   if(device!="DISPLAY1" && device!="DISPLAY3")return 3;
   if(Environment.GetEnvironmentVariable("SURFACE_DESK_AUDIO")!=(device=="DISPLAY1"?"1":"0"))return 4;
   if(Environment.GetEnvironmentVariable("WINHOME_DESK_CLIPBOARD")!=(device=="DISPLAY1"?"1":"0"))return 8;
   if(!Environment.GetEnvironmentVariable("SURFACE_DESK_DATA").EndsWith("port-"+port))return 5;
   try {
    var listener=new TcpListener(IPAddress.Loopback,port);listener.Start();
    if(Environment.GetEnvironmentVariable("WHD_TEST_FAIL_PORT")==port.ToString())return 6;
    Console.WriteLine("SurfaceDesk LISTENING port="+port);Console.Out.Flush();Thread.Sleep(30000);listener.Stop();return 0;
   }catch(SocketException){return 7;}
  }
  try {Run(args);return 0;}catch(Exception ex){Console.Error.WriteLine(ex.Message);Console.Error.WriteLine(ex.StackTrace);return 1;}
 }
 static void Run(string[] args) {
  string dir=Path.Combine(args[0],"monitor032-test");Directory.CreateDirectory(dir);
  var report=new MonitorReport {Count=2,Devices=new[]{"DISPLAY1","DISPLAY3"},SelectorCompatible=true};
  var plan=MonitorConnection.Plan(report,true,65534,true);
  Check(plan.Count==2 && plan[1].Port==65535 && plan[0].Audio && !plan[1].Audio && plan[1].Device=="DISPLAY3","distinct ports, device identity and one audio source");
  Reject(()=>MonitorConnection.Plan(report,true,65535,true),"port range overflow rejected before launching");
  Reject(()=>MonitorConnection.Plan(new MonitorReport {Count=2},true,3390,true),"old or incomplete engine rejected");
  var single=MonitorConnection.Plan(report,false,3390,true);Check(single.Count==1 && single[0].Device==null,"existing desktop connection unchanged");
  var entries=MonitorConnection.Entries(plan,"127.0.0.1");string path=Path.Combine(dir,"monitors.rdg");SurfaceRdg.Save(path,entries,"Monitors");
  string title;var loaded=SurfaceRdg.Load(path,out title);Check(loaded.Count==2 && loaded[1].Port==65535 && loaded[1].Name.EndsWith("DISPLAY3"),"RDG roundtrip retains distinct monitor endpoints");
  using(var manager=new RdgManagerForm("127.0.0.1",65534,entries)) {
   var grid=Field<DataGridView>(manager,"grid");Check(grid.Rows.Count==3,"group manager seeded with all monitors");Call(manager,"AddCurrent");Check(grid.Rows.Count==3,"adding current monitors twice is idempotent");
  }
  var store=new DesktopOptionsStore(Path.Combine(dir,"settings.xml"));var options=new DesktopOptions {MonitorScope="separate"};store.Save(options);Check(store.Load().MonitorScope=="separate","separate mode persists for automatic startup");
  var info=new ProcessStartInfo();info.EnvironmentVariables["WINHOME_DESK_MONITOR_DEVICE"]="DISPLAY99";MonitorSharing.Configure(info,false,0);Check(!info.EnvironmentVariables.ContainsKey("WINHOME_DESK_MONITOR_DEVICE"),"primary/all clear inherited per-monitor selection");
  string self=Assembly.GetExecutingAssembly().Location;
  Application.EnableVisualStyles();
  using(var f=new SurfaceDesk(false,true)) {
   f.Show();Application.DoEvents();Field<ComboBox>(f,"addresses").SelectedItem="127.0.0.1";
   Field<ComboBox>(f,"monitorScope").SelectedIndex=2;
   Field<CheckBox>(f,"consent").Checked=true;
   f.ApplyLanguage("en");f.ApplyLanguage("ko");Check(Field<ComboBox>(f,"monitorScope").SelectedIndex==2 && Field<CheckBox>(f,"consent").Checked,"language preserves separate mode and consent");
   Check(((DesktopOptions)Call(f,"CurrentOptions",false)).MonitorScope=="separate","startup saves separate mode");
   typeof(SurfaceDesk).GetField("monitorReport",flags).SetValue(f,report);
   foreach(string lang in new[]{"ko","en"}) {f.ApplyLanguage(lang);Application.DoEvents();using(var bitmap=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(bitmap,new Rectangle(Point.Empty,f.Size));bitmap.Save(Path.Combine(dir,"separate-"+lang+".png"));}}
   // All fake listeners use loopback; never invoke remote input or touch real credentials/settings.
   int basePort=23730;plan=MonitorConnection.Plan(report,true,basePort,true);
   Call(f,"LaunchMonitorSessions",self,true,0,0,plan);int[] ids=Ids(f);
   Pump(()=>Field<bool>(f,"listenerReported"),5000);Check(ids.Length==2 && Alive(ids[0]) && Alive(ids[1]),"both listeners ready with matching environment");
   f.Close();Check(!f.IsDisposed && Alive(ids[0]) && Alive(ids[1]),"closing window preserves both listeners");f.RestoreWindow();
   Call(f,"StopServer");Check(!Alive(ids[0]) && !Alive(ids[1]),"stop removes every child");
   using(var occupied=new PortHolder(basePort+1)) {
    Call(f,"LaunchMonitorSessions",self,true,0,0,plan);ids=Ids(f);Pump(()=>Field<IList>(f,"monitorSessions").Count==0,5000);Check(!Alive(ids[0]) && !Alive(ids[1]),"one bind failure stops all sibling listeners");
   }
   Call(f,"LaunchMonitorSessions",self,true,0,0,plan);ids=Ids(f);Pump(()=>Field<bool>(f,"listenerReported"),5000);
   using(var child=Process.GetProcessById(ids[1])){child.Kill();child.WaitForExit(3000);}Pump(()=>Field<IList>(f,"monitorSessions").Count==0,5000);Check(!Alive(ids[0]),"unexpected child exit stops sibling");
   Call(f,"LaunchMonitorSessions",self,true,0,0,plan);ids=Ids(f);Pump(()=>Field<bool>(f,"listenerReported"),5000);f.ExitApplication();Check(!Alive(ids[0]) && !Alive(ids[1]),"explicit app exit cleans up all monitors");
  }
 }
 sealed class PortHolder:IDisposable {TcpListener listener;public PortHolder(int port){listener=new TcpListener(IPAddress.Loopback,port);listener.Start();}public void Dispose(){listener.Stop();}}
}
