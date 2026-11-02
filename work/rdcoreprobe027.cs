using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Microsoft.RemoteDesktop.ClientCore;
public static class RDCoreProbe027 {
 [DllImport("kernel32",CharSet=CharSet.Unicode)] static extern bool SetDllDirectory(string p);
 static string report; static void Log(string s){File.AppendAllText(report,DateTime.UtcNow.ToString("O")+" "+s+Environment.NewLine);}
 public static void Run(string bundle,string secret,string output) {
  report=output; File.WriteAllText(report,""); SetDllDirectory(bundle);
  typeof(ActivityManager).Assembly.GetType("RdcMan.RDCoreActivation").GetMethod("Register",BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,null);
  using(var form=new Form()){form.Width=1100;form.Height=800;form.ShowInTaskbar=false;
   var manager=new ActivityManager();
   manager.Initialize(System.Runtime.InteropServices.RuntimeInformation.OSDescription,"unknown","com.microsoft.rdcman","3.22.0.0",0,LogLevel.Information);
   var connection=manager.CreateConnection("full address:s:127.0.0.1:13390\r\nusername:s:surface\r\nenableudptransport:i:0\r\nnetworkautodetect:i:0\r\nbandwidthautodetect:i:0\r\naudiomode:i:0\r\nredirectclipboard:i:1\r\nredirectprinters:i:1\r\nredirectsmartcards:i:1\r\ndynamic resolution:i:1\r\naudiocapturemode:i:1\r\ncamerastoredirect:s:*\r\ndevicestoredirect:s:*\r\nredirectcomports:i:1\r\ndrivestoredirect:s:*\r\nusbdevicestoredirect:s:*\r\nredirectlocation:i:1\r\n"+(Environment.GetEnvironmentVariable("WINHOME_PROBE_TLS")=="1"?"enablecredsspsupport:i:0\r\nprompt for credentials on client:i:1\r\n":""));
   var settings=connection.ConnectionSettings;
   settings.ConsumerHandlesCredentialsRequest=true;
   settings.UserName="surface";
   settings.SessionWindowHandle=(ulong)form.Handle.ToInt64();
   settings.PopupUIParentWindowHandle=(ulong)form.Handle.ToInt64();
   settings.DesktopWidth=1024;settings.DesktopHeight=768;settings.RedirectSpeakers=false;
   settings.EnableUDPShortpath=true;settings.ConnectionMode=(ConnectionMode)0;settings.EnableSmartSizing=true;settings.SingleMonInWindowedMode=true;settings.EnableDynamicResolution=false;settings.IntrinsicLoggingEnabled=true;
   connection.ConnectionSettings=settings;
   connection.ConnectionStatusChanged+=(s,a)=>Log("Status="+a.CurrentStatus);
   connection.Connected+=(s,a)=>Log("Connected");
   connection.LoginCompleted+=(s,a)=>Log("LoginCompleted");
   int requests=0;
   connection.CredentialsRequested+=(s,a)=>{Log("CredentialsRequested reason="+a.RequestReason);if(++requests>1){a.Request.Cancel();return;}a.Request.ProvideCredentials(Environment.GetEnvironmentVariable("WINHOME_PROBE_USER")??"surface",secret);};
   connection.CertificateErrorDetected+=(s,a)=>{Log("CertificateError="+a.CertificateValidationResult);a.Request.Accept();};
   connection.InsecureConnectionDetected+=(s,a)=>{Log("InsecureConnection");a.Request.Accept();};
   connection.Disconnected+=(s,a)=>Log("Disconnected="+a.DisconnectCode+" client="+a.ClientStackDisconnectCode+" server="+a.ServerStackDisconnectCode+" symbol="+a.ErrorCodeSymbolic+" message="+a.ErrorMessage);
   var timer=new System.Windows.Forms.Timer();timer.Interval=12000;timer.Tick+=(s,a)=>{timer.Stop();Log("Timeout test end");connection.Disconnect();form.Close();};
   form.Shown+=(s,a)=>{Log("Connect");connection.Connect();timer.Start();};
   Application.Run(form);timer.Dispose();GC.KeepAlive(manager);Log("Complete");
  }
 }
}


