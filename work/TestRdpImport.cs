using System;using System.IO;using System.Text;using System.Collections.Generic;using System.Xml;
class TestRdpImport {
 static void Check(bool b,string message){if(!b)throw new Exception(message);}
 static string Write(string dir,string name,string text,Encoding encoding){string p=Path.Combine(dir,name);File.WriteAllText(p,text,encoding);return p;}
 static void Main(string[] args){
  string dir=Path.Combine(args[0],"work","rdp-import-tests");Directory.CreateDirectory(dir);Directory.CreateDirectory(Path.Combine(dir,"other"));
  string first=Write(dir,"서피스9.rdp","full address:s:192.168.0.110:3390\r\nusername:s:surface\r\ndomain:s:\r\nprompt for credentials on client:i:1\r\npassword 51:b:TEST-SECRET-IGNORE\r\n",Encoding.Unicode);
  SurfaceEntry e=SurfaceRdpImport.Read(first,"집 & 서피스");Check(e.Host=="192.168.0.110"&&e.Port==3390&&e.UserName=="surface"&&e.Name=="서피스9","Surface Desk UTF16");
  string second=Write(dir,"서피스8.rdp","FULL ADDRESS:s:surface8.example.invalid\nserver port:i:3391\nusername:s:DOMAIN\\tester\ndomain:s:DOMAIN",new UTF8Encoding(false));
  e=SurfaceRdpImport.Read(second,"집");Check(e.Port==3391&&e.UserName=="DOMAIN\\tester"&&e.Domain=="DOMAIN","separate port and account");
  string third=Write(dir,"기본.rdp","full address:s:third.example.invalid\n",Encoding.BigEndianUnicode);
  e=SurfaceRdpImport.Read(third,"집");Check(e.Port==3389&&e.UserName=="","default port/blank account/BE BOM");
  string ansi=Write(dir,"한글.rdp","full address:s:ansi.example.invalid\r\nusername:s:사용자",Encoding.GetEncoding(949));
  Check(SurfaceRdpImport.Read(ansi,"집").UserName=="사용자","ANSI Korean");
  string same=Write(Path.Combine(dir,"other"),"서피스8.rdp","full address:s:fourth.example.invalid:3392\nserver port:i:3389",Encoding.UTF8);
  string malformed=Write(dir,"잘못된.rdp","full address:s:pc.invalid:70000",Encoding.UTF8);
  var existing=new List<SurfaceEntry>{new SurfaceEntry("기존","기존 서버","192.168.0.110",3390)};
  RdpImportResult result=SurfaceRdpImport.Import(new[]{first,second,third,same,malformed},"집",existing);
  Check(result.Added.Count==3&&result.Skipped==1&&result.Errors.Count==1,"batch partial success");Check(existing.Count==1&&existing[0].Name=="기존 서버","existing untouched");
  Check(result.Added[2].Name=="서피스8 (2)"&&result.Added[2].Port==3392,"name collision and inline port precedence");
  existing.AddRange(result.Added);string output=Path.Combine(dir,"result.rdg");SurfaceRdg.Save(output,existing,"가져오기 검사");
  string xml=File.ReadAllText(output);Check(!xml.Contains("TEST-SECRET")&&!xml.Contains("prompt for credentials"),"no password or prompt copied");
  string title;var loaded=SurfaceRdg.Load(output,out title);Check(loaded.Count==4&&loaded[1].UserName=="DOMAIN\\tester"&&loaded[1].Domain=="DOMAIN","RDG roundtrip credentials");
  foreach(string content in new[]{"username:s:surface","full address:s:host:abc","full address:s:[::1]:3390","full address:s:http://example.invalid"}) {
   string bad=Write(dir,"bad.rdp",content,Encoding.UTF8);var badResult=SurfaceRdpImport.Import(new[]{bad},"집",new List<SurfaceEntry>());Check(badResult.Added.Count==0&&badResult.Errors.Count==1,"reject invalid RDP");
  }
  Console.WriteLine("PASS: UTF8/UTF16LE/UTF16BE/CP949, ports, accounts, groups, append, duplicates, name conflicts, partial errors, RDG roundtrip, password exclusion.");
 }
}
