using System;
using System.IO;
using System.Drawing;
using System.Reflection;
using System.Windows.Forms;
class TestClipboard034 {
 sealed class Startup:IStartupRegistration {public string Read(){return null;}public void Write(string command){throw new Exception("Unexpected startup mutation");}}
 static void Check(bool b,string name){if(!b)throw new Exception(name);Console.WriteLine("PASS "+name);}
 [STAThread] static void Main(string[] args) {
  string dir=Path.Combine(args[0],"clipboard034-tests-ui");Directory.CreateDirectory(dir);
  var store=new DesktopOptionsStore(Path.Combine(dir,"settings.xml"));var settings=new DesktopOptions();Check(settings.ClipboardEnabled,"clipboard enabled by default");settings.ClipboardEnabled=false;store.Save(settings);Check(!store.Load().ClipboardEnabled,"clipboard opt-out persists");
  string xml=File.ReadAllText(Path.Combine(dir,"settings.xml")).Replace("<ClipboardEnabled>false</ClipboardEnabled>","");File.WriteAllText(Path.Combine(dir,"settings.xml"),xml);Check(store.Load().ClipboardEnabled,"old preferences migrate to enabled");
  var document=SurfaceRdg.Create(new[]{new SurfaceEntry("Group","Monitor 1","192.0.2.1",3390),new SurfaceEntry("Group","Monitor 2","192.0.2.1",3391)},"Clipboard test");
  Check(document.SelectNodes("//server/localResources[@inherit='None']/redirectClipboard[text()='True']").Count==2,"RDG clipboard enabled per endpoint without inheritance");document.Save(Path.Combine(dir,"clipboard.rdg"));
  Application.EnableVisualStyles();var flags=BindingFlags.Instance|BindingFlags.NonPublic;
  using(var form=new SurfaceDesk(false,true)) {
   typeof(SurfaceDesk).GetField("preferences",flags).SetValue(form,settings);
   var current=(DesktopOptions)typeof(SurfaceDesk).GetMethod("CurrentOptions",flags).Invoke(form,new object[]{false});Check(!current.ClipboardEnabled,"other settings changes preserve clipboard opt-out");
   foreach(string lang in new[]{"ko","en"}) {
    L.Set(lang);using(var dialog=(Form)typeof(SurfaceDesk).GetMethod("CreateOptionsDialog",flags).Invoke(form,new object[]{new Startup()})) {
     var check=(CheckBox)dialog.Controls.Find("ClipboardEnabled",true)[0];Check(!check.Checked,"settings checkbox reflects opt-out "+lang);
     dialog.Show();Application.DoEvents();Check(check.Right<dialog.ClientSize.Width,"clipboard setting fits "+lang);
     using(var bmp=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(bmp,new Rectangle(Point.Empty,dialog.Size));bmp.Save(Path.Combine(dir,"settings-"+lang+".png"));}dialog.Close();
    }
   }
   form.ExitApplication();
  }L.Set("ko");
 }
}
