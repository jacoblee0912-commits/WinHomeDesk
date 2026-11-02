using System;using System.Diagnostics;using System.Reflection;using System.Threading;using System.Windows.Forms;using System.Drawing;using System.IO;
class PreviewRegistration : IStartupRegistration {
    public string Read(){return null;}
    public void Write(string command){throw new Exception("Preview must not change startup registry");}
}
class TestLifecycle017 {
    static void Check(bool ok,string msg){if(!ok)throw new Exception(msg);}
    [STAThread] static void Main(string[] args) {
        if(args.Length>0 && args[0]=="--child"){Thread.Sleep(30000);return;}
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        Check(SurfaceDesk.CloseToTray(CloseReason.UserClosing,false),"X hides");
        Check(!SurfaceDesk.CloseToTray(CloseReason.WindowsShutDown,false),"shutdown allowed");
        Check(!SurfaceDesk.CloseToTray(CloseReason.UserClosing,true),"explicit exit allowed");
        var child=Process.Start(new ProcessStartInfo(Assembly.GetExecutingAssembly().Location,"--child") {UseShellExecute=false,CreateNoWindow=true});
        try {
            using(var f=new SurfaceDesk(false,true)) {
                f.Show();Application.DoEvents();
                typeof(SurfaceDesk).GetMethod("InitializeTray",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(f,null);
                var tray=(NotifyIcon)typeof(SurfaceDesk).GetField("tray",BindingFlags.NonPublic|BindingFlags.Instance).GetValue(f);
                Check(tray.Visible && tray.ContextMenuStrip.Items.Count==8,"native tray and menu created");
                using(var dialog=(Form)typeof(SurfaceDesk).GetMethod("CreateOptionsDialog",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(f,new object[]{new PreviewRegistration()})) {
                    dialog.Show();Application.DoEvents();
                    CheckBox login=null,server=null;
                    foreach(Control c in dialog.Controls){if(c.Name=="LoginStartup")login=(CheckBox)c;if(c.Name=="ServerStartup")server=(CheckBox)c;}
                    Check(!login.Checked && !server.Checked && !server.Enabled,"startup options off by default");
                    login.Checked=true;Check(server.Enabled,"server option requires app startup");server.Checked=true;
                    login.Checked=false;Check(!server.Checked&&!server.Enabled,"disable also clears server auto-start");
                    using(var bitmap=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(bitmap,new Rectangle(Point.Empty,dialog.Size));bitmap.Save(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),"preview-options029.png"));}
                    dialog.Close();
                }
                typeof(SurfaceDesk).GetField("engine",BindingFlags.NonPublic|BindingFlags.Instance).SetValue(f,child);
                f.Close();Application.DoEvents();
                Check(!f.IsDisposed && !f.Visible && !f.ShowInTaskbar,"close hides without disposal");
                Check(!child.HasExited,"server survives X");
                ((ToolStripMenuItem)tray.ContextMenuStrip.Items[1]).PerformClick();Application.DoEvents();Check(f.Visible && f.ShowInTaskbar,"restore from tray");
                int id=child.Id;((ToolStripMenuItem)tray.ContextMenuStrip.Items[7]).PerformClick();Application.DoEvents();Check(f.IsDisposed,"tray exit disposes");
                bool alive=false;try{using(var p=Process.GetProcessById(id))alive=!p.HasExited;}catch(ArgumentException){}
                Check(!alive,"explicit exit stops child");
            }
        }finally{try{if(!child.HasExited)child.Kill();}catch{}child.Dispose();}
        Console.WriteLine("Lifecycle: X keeps server alive, restore, explicit exit cleanup, shutdown policy PASS");
    }
}
