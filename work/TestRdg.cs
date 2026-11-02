using System;using System.IO;using System.Collections.Generic;using System.Xml;
class TestRdg {
 static void Check(bool value,string what){if(!value)throw new Exception(what);}
 static void Reject(Action action,string what){try{action();}catch(ArgumentException){return;}throw new Exception("Expected rejection: "+what);}
 static void Main(string[] args){
  string root=args[0];
  var entries=new List<SurfaceEntry>{new SurfaceEntry("사무실 & 테스트","Surface Pro 9 <테스트>","surface9.example.invalid",3390),new SurfaceEntry("사무실 & 테스트","Surface Pro 8","surface8.example.invalid",3391),new SurfaceEntry("외부","Surface 다른 기기","surface-other.example.invalid",3390)};
  string path=Path.Combine(root,"work","RDG-compatibility-test.rdg");SurfaceRdg.Save(path,entries,"Surface Desk 호환성 검사");
  string title;var roundtrip=SurfaceRdg.Load(path,out title);Check(roundtrip.Count==3&&title=="Surface Desk 호환성 검사","roundtrip");
  XmlDocument d=new XmlDocument();d.Load(path);Check(d.SelectNodes("/RDCMan/file/group").Count==2,"groups");
  Check(d.SelectNodes("//server/connectionSettings/port[text()='3391']").Count==1,"custom port");
  Check(d.SelectNodes("//server/logonCredentials[@inherit='None']/userName[text()='surface']").Count==3,"accounts");
  Check(d.SelectNodes("//password[string-length(.)>0]").Count==0,"no passwords");
  Check(d.SelectNodes("//server/remoteDesktop[@inherit='None']/colorDepth[text()='32']").Count==3,"32-bit graphics profiles");
  Check(d.SelectNodes("//server/securitySettings[@inherit='None']/authentication[text()='Warn']").Count==3,"compatible security defaults");
  for(int i=0;i<entries.Count;i++){Check(roundtrip[i].Group==entries[i].Group&&roundtrip[i].Name==entries[i].Name&&roundtrip[i].Port==entries[i].Port,"escaped fields");}
  Reject(()=>SurfaceRdg.Create(new List<SurfaceEntry>(),"empty"),"empty");
  foreach(string host in new[]{"http://192.168.0.1","192.168.0.1:3390","999.1.1.1","","a b"})Reject(()=>SurfaceRdg.Create(new List<SurfaceEntry>{new SurfaceEntry("g","n",host,3390)},"t"),host);
  Reject(()=>SurfaceRdg.Create(new List<SurfaceEntry>{new SurfaceEntry("g","n","pc.invalid",0)},"t"),"bad port");
  Reject(()=>SurfaceRdg.Create(new List<SurfaceEntry>{entries[0],entries[0]},"t"),"duplicates");
  string inherit=Path.Combine(root,"work","rdg-inherit-test.rdg");
  File.WriteAllText(inherit,"<RDCMan><file><properties><name>T</name></properties><connectionSettings inherit='None'><port>3392</port></connectionSettings><group><properties><name>G</name></properties><server><properties><name>pc.invalid</name></properties></server></group></file></RDCMan>");
  Check(SurfaceRdg.Load(inherit,out title)[0].Port==3392,"inherited port");
  File.WriteAllText(inherit,"<!DOCTYPE RDCMan [<!ENTITY x SYSTEM 'file:///not-used'>]><RDCMan><file>&x;</file></RDCMan>");
  bool blocked=false;try{SurfaceRdg.Load(inherit,out title);}catch(XmlException){blocked=true;}Check(blocked,"DTD prohibited");
  SurfaceRdg.Save(Path.Combine(root,"outputs","SurfaceDesk-내서피스.rdg"),new List<SurfaceEntry>{new SurfaceEntry("서피스","Surface Pro 9","192.168.0.110",3390)},"내 서피스");
  Console.WriteLine("RDG checks passed: groups, XML escaping, ports, credentials, roundtrip, invalid inputs, inherited ports, DTD blocking.");
 }
}
