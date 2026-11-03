using System;using System.IO;using System.Xml;using System.Drawing;using System.Reflection;using System.Windows.Forms;using System.Text.RegularExpressions;using System.Diagnostics;
class NoStartup030:IStartupRegistration {public string Read(){return null;}public void Write(string s){throw new Exception("Do not mutate startup in language test");}}
class TestLocalization030 {
 static BindingFlags flags=BindingFlags.NonPublic|BindingFlags.Instance;
 static T Field<T>(object o,string n){return (T)o.GetType().GetField(n,flags).GetValue(o);}
 static void Check(bool ok,string label){if(!ok)throw new Exception(label);Console.WriteLine("PASS "+label);}
 static void Capture(Form f,string path){f.Show();Application.DoEvents();using(var b=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(b,new Rectangle(Point.Empty,f.Size));b.Save(path);}}
 static void CheckEnglish(Control c) {
  if(c is Label || c is ButtonBase || c is Form)Check(!Regex.IsMatch(c.Text,"[가-힣]"),"English "+c.GetType().Name+" "+c.Name);
  var combo=c as ComboBox;if(combo!=null&&combo.Name!="LanguageSelector")foreach(var item in combo.Items)Check(!Regex.IsMatch(Convert.ToString(item),"[가-힣]"),"English option");
  foreach(Control child in c.Controls)CheckEnglish(child);
 }
 [STAThread] static void Main(string[] args) {
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  string dir=Path.Combine(args[0],"localization030-test");Directory.CreateDirectory(dir);
  var store=new DesktopOptionsStore(Path.Combine(dir,"preferences.xml"));File.WriteAllText(Path.Combine(dir,"preferences.xml"),"<DesktopOptions><Version>1</Version></DesktopOptions>");
  Check(store.Load().Language=="ko","legacy settings remain Korean");
  var saved=store.Load();saved.Language="en";saved.RdcManPath=Path.Combine(args[0],"RDCMan.exe");saved.ProtectPassword("only-for-localization-test");store.Save(saved);
  Check(store.Load().Language=="en"&&store.Load().ReadPassword()=="only-for-localization-test","language persists without changing protected password");
  bool invalid=false;try{saved.Language="invalid";store.Save(saved);}catch(InvalidDataException){invalid=true;}Check(invalid,"unknown language rejected");
  string graphics=GraphicsMode.Arguments(0);L.Set("en");Check(GraphicsMode.Arguments(0)==graphics,"language does not change protocol options");
  Check(L.Count>=280,"translated UI strings embedded");
  using(var f=new SurfaceDesk(false,true)) {
   var pass=Field<TextBox>(f,"password");pass.Text="한글-keep-this-password";
   var address=Field<ComboBox>(f,"addresses");string before=address.Text;
   var quality=Field<ComboBox>(f,"quality");quality.SelectedIndex=1;
   var metrics=Field<PerformanceMetrics>(f,"performance");
   metrics.Accept("SurfaceDesk PERF_CAPTURE window_s=2 target=45 frames=38 fps=19 changed_fps=18 capture_ms=12 copy_ms=2 wait_ms=35 work_max_ms=85 late=1 errors=0 backend=dxgi pipeline=latest replaced=8");
   metrics.Accept("SurfaceDesk PERF_SEND state=active window_s=2 frames=36 fps=18 encode_ms=19 send_ms=14 send_max_ms=41 frame_max_ms=68 payload_bytes=1532000 payload_mbps=6.1 area_pct=78 failed=0 other=0");
   metrics.Accept("SurfaceDesk PERF_PIPELINE window_s=2 taken=36 changed=36 replaced=8 snapshot_ms=1 age_ms=9 age_max_ms=25 delivery_wait_ms=22 pending_limit=1");
   f.ApplyLanguage("en");DateTime captured=metrics.CaptureAt;
   Capture(f,Path.Combine(args[0],"preview-main030-en.png"));CheckEnglish(f);
   using(var d=(Form)typeof(SurfaceDesk).GetMethod("CreateOptionsDialog",flags).Invoke(f,new object[]{new NoStartup030()})){Capture(d,Path.Combine(args[0],"preview-options030-en.png"));CheckEnglish(d);d.Close();}
   f.ApplyLanguage("ko");Capture(f,Path.Combine(args[0],"preview-main030-ko.png"));
   Check(Field<Button>(f,"start").Text=="서버 시작","switch back to Korean");
   f.ApplyLanguage("en");Check(pass.Text=="한글-keep-this-password"&&address.Text==before&&quality.SelectedIndex==1,"live switching preserves password, address and selected mode");
   Check(metrics.CaptureAt==captured&&!Regex.IsMatch(metrics.CaptureText+metrics.SendText+metrics.PipelineText,"[가-힣]"),"metrics relocalize without resetting timestamps");
   var child=Process.Start(new ProcessStartInfo(Path.Combine(args[0],"TestLifecycle017.exe"),"--child") {UseShellExecute=false,CreateNoWindow=true});
   try {typeof(SurfaceDesk).GetField("engine",flags).SetValue(f,child);f.ApplyLanguage("ko");f.ApplyLanguage("en");Check(!child.HasExited,"language switch does not restart the engine process");}
   finally {typeof(SurfaceDesk).GetField("engine",flags).SetValue(f,null);if(!child.HasExited)child.Kill();child.Dispose();}
   // Exercise the selector's save path against an isolated preference store.
   typeof(SurfaceDesk).GetField("preferenceStore",flags).SetValue(f,store);typeof(SurfaceDesk).GetField("testMode",flags).SetValue(f,false);
   Field<ComboBox>(f,"languageSelector").SelectedIndex=0;
   Check(store.Load().Language=="ko"&&store.Load().ReadPassword()=="only-for-localization-test","selector saves language only without startup registration");
   f.ExitApplication();
  }
  foreach(string language in new[]{"ko","en"}) {
   L.Set(language);using(var group=new RdgManagerForm("192.0.2.10",3390)) {
    Field<TextBox>(group,"title").Text="내 이름 / My name";
    var grid=Field<DataGridView>(group,"grid");grid.Rows[0].Cells[1].Value="한글 기기 이름";
    L.ApplyStatic(group);Check(Field<TextBox>(group,"title").Text=="내 이름 / My name"&&Convert.ToString(grid.Rows[0].Cells[1].Value)=="한글 기기 이름","group user data preserved "+language);
    Capture(group,Path.Combine(args[0],"preview-groups030-"+language+".png"));group.Close();
   }
  }
  L.Set("ko");Console.WriteLine("Localization tests completed; no live server or startup task changed.");
 }
}
