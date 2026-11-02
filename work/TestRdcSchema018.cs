using System;using System.Collections.Generic;using System.Reflection;using System.Xml;
class TestRdcSchema018 {
 static void Main(string[] args) {
  var a=Assembly.LoadFrom(args[0]);var t=a.GetType("RdcMan.RemoteDesktopSettings");var settings=Activator.CreateInstance(t,true);
  var doc=new XmlDocument();doc.Load(args[1]);var errors=new List<string>();
  var read=t.BaseType.GetMethod("ReadXml",BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic);
  read.Invoke(settings,new object[]{doc.SelectSingleNode("//server/remoteDesktop"),null,errors});
  var depth=t.GetProperty("ColorDepth").GetValue(settings,null);
  if((int)depth.GetType().GetProperty("Value").GetValue(depth,null)!=32 || errors.Count!=0)throw new Exception("RDCMan settings rejected");
  Console.WriteLine("Actual RDCMan settings parser accepted generated remoteDesktop/colorDepth=32 PASS");
  t=a.GetType("RdcMan.LocalResourcesSettings");settings=Activator.CreateInstance(t,true);
  read=t.BaseType.GetMethod("ReadXml",BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic);
  read.Invoke(settings,new object[]{doc.SelectSingleNode("//server/localResources"),null,errors});
  foreach(var pair in new[]{new[]{"AudioRedirectionMode","Client"},new[]{"AudioRedirectionQuality","High"},new[]{"AudioCaptureRedirectionMode","DoNotRecord"}}) {
   var setting=t.GetProperty(pair[0]).GetValue(settings,null);
   if(setting.GetType().GetProperty("Value").GetValue(setting,null).ToString()!=pair[1])throw new Exception("Audio setting rejected: "+pair[0]);
  }
  if(errors.Count!=0)throw new Exception(String.Join(";",errors));
  Console.WriteLine("Actual RDCMan settings parser: local playback / high quality / microphone disabled PASS");
 }
}
