using System;using System.IO;using System.Reflection;using System.Diagnostics;using System.Windows.Forms;using System.Drawing;using System.Threading;
class TestMonitorSharing031 {
 static BindingFlags flags=BindingFlags.Instance|BindingFlags.NonPublic;
 static T Field<T>(object o,string n){return (T)o.GetType().GetField(n,flags).GetValue(o);}
 static object Call(object o,string n,params object[] a){return o.GetType().GetMethod(n,flags).Invoke(o,a);}
 static void Check(bool b,string s){if(!b)throw new Exception(s);Console.WriteLine("PASS "+s);}
 static void Reject(Action action,string label){bool rejected=false;try{action();}catch(InvalidDataException){rejected=true;}Check(rejected,label);}
 [STAThread] static int Main(string[] a) {
  if(a.Length==1&&a[0]=="/monitors") {
   if(Environment.GetEnvironmentVariable("WHD_TEST_TIMEOUT")=="1")Thread.Sleep(3000);
   string scope=Environment.GetEnvironmentVariable("SURFACE_DESK_ALL_MONITORS");string edge=Environment.GetEnvironmentVariable("SURFACE_DESK_MAX_EDGE");
   Console.Error.WriteLine(new string('e',70000)); // Exercise simultaneous draining of both pipes.
   Console.WriteLine("SurfaceDesk MONITOR_LIST count="+(scope=="1"?2:1)+" width="+(edge=="1440"?2880:3840)+" height=1200");
   Console.WriteLine("SurfaceDesk MONITOR_CANVAS protocol_monitors=1 physical_monitors="+(scope=="1"?2:1)+" origin=0,0");return -2001;
  }
  Application.EnableVisualStyles();string dir=Path.Combine(a[0],"monitor031-test");Directory.CreateDirectory(dir);
  foreach(string inherited in new[]{null,"0","1"})foreach(bool flag in new[]{false,true}) {
   Check(!MonitorSharing.Resolve("primary",inherited,flag),"explicit primary overrides legacy");
   Check(MonitorSharing.Resolve("all",inherited,flag),"explicit all overrides legacy");
   Check(MonitorSharing.Resolve("legacy",inherited,flag)==(inherited=="1"||(inherited!="0"&&flag)),"legacy precedence preserved");
  }
  var store=new DesktopOptionsStore(Path.Combine(dir,"settings.xml"));File.WriteAllText(Path.Combine(dir,"settings.xml"),"<DesktopOptions><Version>1</Version></DesktopOptions>");
  Check(store.Load().MonitorScope=="legacy","old XML migrates without expanding sharing");
  var saved=store.Load();saved.MonitorScope="all";saved.Host="127.0.0.1";saved.StartServerAtLogin=true;saved.ProtectPassword("local-monitor-test-secret");store.Save(saved);
  Check(store.Load().MonitorScope=="all"&&store.Load().ReadPassword()=="local-monitor-test-secret","scope and DPAPI password persist");
  saved.MonitorScope="invalid";Reject(()=>store.Save(saved),"invalid scope rejected");
  Reject(()=>MonitorReport.Parse("SurfaceDesk MONITOR_LIST count=17 width=4000 height=2000",0),"too many monitors rejected");
  Reject(()=>MonitorReport.Parse("SurfaceDesk MONITOR_LIST count=1 width=999999999999 height=100",0),"overflow rejected");
  Reject(()=>MonitorReport.Parse("SurfaceDesk MONITOR_LIST count=2 width=3840 height=1200",4),"nonprint failure exit rejected");
  Reject(()=>MonitorReport.Parse("legacy engine help",-2001),"old engine not reported as multi-monitor capable");
  string self=Assembly.GetExecutingAssembly().Location;
  var report=MonitorSharing.Probe(self,true,1,5000);Check(report.Count==2&&report.Width==2880,"child receives scope and quality; stdout/stderr drain; -2001 print accepted");
  Check(MonitorSharing.Probe(self,false,0,5000).Count==1,"primary reaches child as explicit zero");
  Environment.SetEnvironmentVariable("WHD_TEST_TIMEOUT","1");bool timed=false;try{MonitorSharing.Probe(self,true,0,50);}catch(TimeoutException){timed=true;}finally{Environment.SetEnvironmentVariable("WHD_TEST_TIMEOUT",null);}Check(timed,"hung enumeration terminated");
  L.Set("ko");using(var f=new SurfaceDesk(false,true)) {
   f.Show();Application.DoEvents();
   typeof(SurfaceDesk).GetField("preferenceStore",flags).SetValue(f,store);typeof(SurfaceDesk).GetField("testMode",flags).SetValue(f,false);
   var scope=Field<ComboBox>(f,"monitorScope");var consent=Field<CheckBox>(f,"consent");consent.Checked=true;scope.SelectedIndex=1;
   Check(!consent.Checked&&store.Load().MonitorScope=="all","scope change revokes consent and saves immediately");
   Check(store.Load().ReadPassword()=="local-monitor-test-secret","scope-only save preserves protected auto-start password");
   consent.Checked=true;f.ApplyLanguage("en");f.ApplyLanguage("ko");Check(scope.SelectedIndex==1&&consent.Checked,"language switch preserves scope and consent");
   Check(((DesktopOptions)Call(f,"CurrentOptions",false)).MonitorScope=="all","normal preference save includes scope");
   Check(Field<ComboBox>(f,"graphicsMode").Enabled,"H.264 remains selectable for all monitors");
   Call(f,"ResetRuntimeMonitors");
   Call(f,"AcceptRuntimeMonitorLine","SurfaceDesk MONITOR index=1 device=DISPLAY1 primary=1 source=0,0,2560,1440 stream=809,270,2248,1080");
   Call(f,"AcceptRuntimeMonitorLine","SurfaceDesk MONITOR index=2 device=DISPLAY2 primary=0 source=-1440,-480,0,2080 stream=0,0,809,1440");
   Call(f,"AcceptRuntimeMonitorLine","SurfaceDesk CAPTURE backend=gdi source=6560x2560 stream=3688x1440 resize=GDI-halftone");
   var actual=Field<MonitorReport>(f,"monitorReport");Check(actual.Count==2&&actual.Width==3688&&actual.Height==1440,"runtime device IDs and stream dimensions replace preflight estimate");
   Call(f,"SetRunning",true);Check(!scope.Enabled,"scope disabled while running");Call(f,"SetRunning",false);Check(scope.Enabled,"scope enabled after stop");
   foreach(string language in new[]{"ko","en"}) {f.ApplyLanguage(language);Application.DoEvents();using(var b=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(b,new Rectangle(Point.Empty,f.Size));b.Save(Path.Combine(dir,"monitor-"+language+".png"));}}
   f.ExitApplication();
  }
  if(a.Length>1) {
   var primary=MonitorSharing.Probe(a[1],false,0,8000);var all=MonitorSharing.Probe(a[1],true,0,8000);
   Check(primary.Count==1&&all.Count>=primary.Count,"supplied engine primary/all enumeration");
   File.WriteAllText(Path.Combine(dir,"actual-monitors.txt"),primary.Details+"\n"+all.Details);
  }
  Console.WriteLine("No live user session, startup task or real preferences modified.");return 0;
 }
}
