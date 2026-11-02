using System;using System.IO;using System.Diagnostics;using System.Threading;using System.Reflection;using System.Windows.Forms;
class TestClient028 {
 static void Check(bool value,string text){if(!value)throw new Exception(text);Console.WriteLine("PASS "+text);}
 [STAThread] static void Main(string[] args){
  string root=args[0],target=args[1],data=Path.Combine(root,"client028-test");Directory.CreateDirectory(data);
  var embedded=RdcManClient.HookBytes();var original=File.ReadAllBytes(Path.Combine(root,"ime-fix","WinHomeDesk.ImeHook.dll"));
  Check(Convert.ToBase64String(embedded)==Convert.ToBase64String(original),"embedded hook matches user-confirmed working helper");
  string hook=RdcManClient.ExtractHook(data);Check(File.Exists(hook),"embedded hook extracted");
  var written=File.GetLastWriteTimeUtc(hook);Thread.Sleep(40);RdcManClient.ExtractHook(data);
  Check(File.GetLastWriteTimeUtc(hook)==written,"unchanged cached hook is not rewritten");
  File.WriteAllText(hook,"corrupted test cache");RdcManClient.ExtractHook(data);
  Check(Convert.ToBase64String(File.ReadAllBytes(hook))==Convert.ToBase64String(original),"damaged cache restored from embedded module");
  string old=Environment.GetEnvironmentVariable("DOTNET_STARTUP_HOOKS");
  try {
   Environment.SetEnvironmentVariable("DOTNET_STARTUP_HOOKS","existing-hook.dll");
   var start=RdcManClient.BuildStartInfo(target,hook,Path.Combine(data,"log.txt"));
   Check(start.EnvironmentVariables["DOTNET_STARTUP_HOOKS"]=="existing-hook.dll"+Path.PathSeparator+hook,"existing hooks preserved for child process");
   Check(Environment.GetEnvironmentVariable("DOTNET_STARTUP_HOOKS")=="existing-hook.dll","parent environment unchanged");
   Check(!start.UseShellExecute && start.Arguments=="-noconnect" && !start.Arguments.Contains("mstsc"),"new RDCore engine retained");
  }finally{Environment.SetEnvironmentVariable("DOTNET_STARTUP_HOOKS",old);}
  using(var form=new SurfaceDesk(false,true)) {
   var button=form.Controls.Find("LaunchRdcMan",false);
   Check(button.Length==1 && button[0].Enabled,"main client button enabled without server");
   typeof(SurfaceDesk).GetMethod("SetRunning",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(form,new object[]{true});
   Check(button[0].Enabled,"client button independent of running server");
   var preferences=(DesktopOptions)typeof(SurfaceDesk).GetField("preferences",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(form);
   preferences.RdcManPath=target;
   var saved=(DesktopOptions)typeof(SurfaceDesk).GetMethod("CurrentOptions",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(form,new object[]{false});
   Check(saved.RdcManPath==target,"connection saves retain client path");
  }
  string log=Path.Combine(data,"startup.log");File.WriteAllText(log,"");
  var info=RdcManClient.BuildStartInfo(target,hook,log);
  info.Arguments+=" -noopen";info.CreateNoWindow=true;
  info.EnvironmentVariables["DOTNET_BUNDLE_EXTRACT_BASE_DIR"]=Path.Combine(data,"bundle-cache");
  using(var process=Process.Start(info))try {
   bool active=false;
   for(int i=0;i<100;i++){if(process.HasExited)throw new Exception("RDCMan exited early");if(File.ReadAllText(log).Contains("active: remote viewport")){active=true;break;}Thread.Sleep(150);}
   Check(active,"actual RDCMan 3.22 starts with extracted embedded hook");
  }finally{if(!process.HasExited){process.Kill();process.WaitForExit();}}
 }
}
