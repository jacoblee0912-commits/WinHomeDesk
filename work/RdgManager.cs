using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Net;
using System.Text;
using System.Windows.Forms;
using System.Xml;

public sealed class SurfaceEntry {
    public string Group, Name, Host;
    public string UserName="surface", Domain="";
    public int Port;
    public SurfaceEntry(string group,string name,string host,int port) { Group=group;Name=name;Host=host;Port=port; }
}

public static class SurfaceRdg {
    public static void Validate(IList<SurfaceEntry> entries) {
        if(entries.Count==0)throw new ArgumentException(L.T("서피스를 한 대 이상 추가하세요."));
        HashSet<string> endpoints=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        HashSet<string> names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        for(int i=0;i<entries.Count;i++) {
            SurfaceEntry e=entries[i];string prefix=(i+1)+L.T("행: ");
            e.Group=(e.Group??"").Trim();e.Name=(e.Name??"").Trim();e.Host=(e.Host??"").Trim();
            if(e.Group.Length==0||e.Name.Length==0||e.Host.Length==0)throw new ArgumentException(prefix+L.T("그룹·기기 이름·IP/호스트 이름을 모두 입력하세요."));
            if(e.Group.Length>128||e.Name.Length>128||e.Host.Length>253)throw new ArgumentException(prefix+L.T("이름이나 주소가 너무 깁니다."));
            XmlConvert.VerifyXmlChars(e.Group);XmlConvert.VerifyXmlChars(e.Name);
            e.UserName=(e.UserName??"").Trim();e.Domain=(e.Domain??"").Trim();
            XmlConvert.VerifyXmlChars(e.UserName);XmlConvert.VerifyXmlChars(e.Domain);
            if(e.UserName.Length>256||e.Domain.Length>256)throw new ArgumentException(prefix+L.T("계정 이름이 너무 깁니다."));
            IPAddress ip;
            if(IPAddress.TryParse(e.Host,out ip)) {
                if(ip.AddressFamily!=System.Net.Sockets.AddressFamily.InterNetwork)throw new ArgumentException(prefix+L.T("IPv4 주소를 사용하세요."));
                if(ip.Equals(IPAddress.Any)||ip.Equals(IPAddress.Broadcast))throw new ArgumentException(prefix+L.T("접속할 수 있는 기기의 주소를 입력하세요."));
            } else {
                if(e.Host.Contains(":")||e.Host.Contains("/")||e.Host.Contains("\\")||Uri.CheckHostName(e.Host)!=UriHostNameType.Dns)
                    throw new ArgumentException(prefix+L.T("주소에 http:// 또는 :포트를 붙이지 마세요. 포트는 별도 칸에 입력합니다."));
                bool numeric=true;foreach(char c in e.Host)if(!(char.IsDigit(c)||c=='.'))numeric=false;
                if(numeric)throw new ArgumentException(prefix+L.T("IPv4 주소가 올바르지 않습니다."));
            }
            if(e.Port<1||e.Port>65535)throw new ArgumentException(prefix+L.T("포트는 1~65535 범위로 입력하세요."));
            if(!endpoints.Add(e.Host+":"+e.Port))throw new ArgumentException(prefix+L.T("같은 주소와 포트가 이미 목록에 있습니다."));
            if(!names.Add(e.Group+"\n"+e.Name))throw new ArgumentException(prefix+L.T("같은 그룹 안의 기기 이름은 다르게 지정하세요."));
        }
    }
    static XmlElement Element(XmlNode parent,string name,string value) {
        XmlDocument doc=parent as XmlDocument??parent.OwnerDocument;
        XmlElement e=doc.CreateElement(name);if(value!=null)e.InnerText=value;parent.AppendChild(e);return e;
    }
    public static XmlDocument Create(IList<SurfaceEntry> entries,string title) {
        Validate(entries);
        XmlDocument doc=new XmlDocument();doc.AppendChild(doc.CreateXmlDeclaration("1.0","utf-8",null));
        XmlElement root=Element(doc,"RDCMan",null);root.SetAttribute("programVersion","2.93");root.SetAttribute("schemaVersion","3");
        XmlElement file=Element(root,"file",null);Element(file,"credentialsProfiles",null);
        XmlElement props=Element(file,"properties",null);Element(props,"expanded","True");Element(props,"name",String.IsNullOrWhiteSpace(title)?"WinHomeDesk":title.Trim());
        Element(props,"comment",L.T("WinHomeDesk: 각 서피스에서 서버를 시작하고, RDCMan 서버 속성 > Logon Credentials에 현재 앱 전용 암호를 입력하세요."));
        Dictionary<string,XmlElement> groups=new Dictionary<string,XmlElement>(StringComparer.OrdinalIgnoreCase);
        foreach(SurfaceEntry e in entries) {
            XmlElement group;
            if(!groups.TryGetValue(e.Group,out group)) {
                group=Element(file,"group",null);groups.Add(e.Group,group);
                props=Element(group,"properties",null);Element(props,"expanded","True");Element(props,"name",e.Group);
            }
            XmlElement server=Element(group,"server",null);props=Element(server,"properties",null);
            Element(props,"displayName",e.Name);Element(props,"name",e.Host);
            Element(props,"comment",L.T("접속 암호는 RDCMan에서 입력하세요. WinHomeDesk 접속 시 해당 앱의 전용 암호를 사용합니다. / 포트 ")+e.Port);
            XmlElement logon=Element(server,"logonCredentials",null);logon.SetAttribute("inherit","None");
            XmlElement profile=Element(logon,"profileName","Custom");profile.SetAttribute("scope","Local");
            Element(logon,"userName",e.UserName);Element(logon,"password","");Element(logon,"domain",e.Domain);
            XmlElement connection=Element(server,"connectionSettings",null);connection.SetAttribute("inherit","None");
            Element(connection,"connectToConsole","False");Element(connection,"port",e.Port.ToString(CultureInfo.InvariantCulture));
            XmlElement security=Element(server,"securitySettings",null);security.SetAttribute("inherit","None");
            // 2.93 rejects enableCredSspSupport (introduced in newer RDCMan).
            // Non-inherited defaults enable CredSSP in 3.22; both versions know Warn.
            Element(security,"authentication","Warn");
            XmlElement desktop=Element(server,"remoteDesktop",null);desktop.SetAttribute("inherit","None");
            Element(desktop,"colorDepth","32");
            XmlElement resources=Element(server,"localResources",null);resources.SetAttribute("inherit","None");
            Element(resources,"audioRedirection","Client");
            Element(resources,"audioRedirectionQuality","High");
            Element(resources,"audioCaptureRedirection","DoNotRecord");
            Element(resources,"redirectClipboard","False");
        }
        Element(root,"connected",null);Element(root,"favorites",null);Element(root,"recentlyUsed",null);
        return doc;
    }
    public static void Save(string path,IList<SurfaceEntry> entries,string title) {
        XmlDocument doc=Create(entries,title);
        XmlWriterSettings options=new XmlWriterSettings {Indent=true,Encoding=new UTF8Encoding(false),NewLineChars="\r\n"};
        using(XmlWriter w=XmlWriter.Create(path,options))doc.Save(w);
    }
    public static List<SurfaceEntry> Load(string path,out string title) {
        XmlReaderSettings options=new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=4000000};
        XmlDocument doc=new XmlDocument();doc.XmlResolver=null;
        using(XmlReader reader=XmlReader.Create(path,options))doc.Load(reader);
        XmlNode file=doc.SelectSingleNode("/RDCMan/file");if(file==null)throw new ArgumentException(L.T("RDCMan .rdg 파일이 아닙니다."));
        title=Value(file,"properties/name","WinHomeDesk");
        List<SurfaceEntry> list=new List<SurfaceEntry>();
        foreach(XmlNode server in file.SelectNodes(".//server")) {
            List<string> groups=new List<string>();
            for(XmlNode p=server.ParentNode;p!=null&&p!=file;p=p.ParentNode)if(p.Name=="group")groups.Insert(0,Value(p,"properties/name",L.T("서피스")));
            int port=3389;
            for(XmlNode p=server;p!=null&&p!=doc.DocumentElement;p=p.ParentNode) {
                XmlElement settings=p.SelectSingleNode("connectionSettings") as XmlElement;
                if(settings!=null&&settings.GetAttribute("inherit")=="None") {
                    string text=Value(settings,"port","3389");if(!Int32.TryParse(text,out port))throw new ArgumentException(L.T("파일의 포트 값이 올바르지 않습니다."));break;
                }
            }
            string host=Value(server,"properties/name","");
            SurfaceEntry entry=new SurfaceEntry(groups.Count==0?L.T("서피스"):String.Join(" / ",groups.ToArray()),Value(server,"properties/displayName",host),host,port);
            for(XmlNode p=server;p!=null&&p!=doc.DocumentElement;p=p.ParentNode) {
                XmlElement credentials=p.SelectSingleNode("logonCredentials") as XmlElement;
                if(credentials!=null&&credentials.GetAttribute("inherit")=="None") {entry.UserName=Value(credentials,"userName","");entry.Domain=Value(credentials,"domain","");break;}
            }
            list.Add(entry);
        }
        Validate(list);return list;
    }
    static string Value(XmlNode node,string path,string fallback) {XmlNode item=node.SelectSingleNode(path);return item==null?fallback:item.InnerText;}
}

public sealed class RdpImportResult {
    public readonly List<SurfaceEntry> Added=new List<SurfaceEntry>();
    public readonly List<string> Errors=new List<string>();
    public int Skipped;
}
public static class SurfaceRdpImport {
    public static SurfaceEntry Read(string path,string group) {
        if(new FileInfo(path).Length>1024*1024)throw new ArgumentException(L.T("RDP 파일이 너무 큽니다 (최대 1MB)."));
        // StreamReader detects UTF-8/UTF-16 BOMs; legacy ANSI falls back to the system code page.
        string text;
        try {using(StreamReader reader=new StreamReader(path,new UTF8Encoding(false,true),true))text=reader.ReadToEnd();}
        catch(DecoderFallbackException){text=File.ReadAllText(path,Encoding.Default);}
        if(text.IndexOf('\0')>=0)throw new ArgumentException(L.T("파일 인코딩을 읽을 수 없습니다. UTF-8 또는 BOM이 있는 UTF-16으로 저장하세요."));
        Dictionary<string,string> fields=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(string line in text.Split(new char[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries)) {
            int first=line.IndexOf(':');if(first<0)continue;int second=line.IndexOf(':',first+1);if(second<0)continue;
            string key=line.Substring(0,first).Trim();
            if(key.Equals("full address",StringComparison.OrdinalIgnoreCase)||key.Equals("server port",StringComparison.OrdinalIgnoreCase)||key.Equals("username",StringComparison.OrdinalIgnoreCase)||key.Equals("domain",StringComparison.OrdinalIgnoreCase))
                fields[key]=line.Substring(second+1).Trim();
        }
        string address;if(!fields.TryGetValue("full address",out address)||String.IsNullOrWhiteSpace(address))throw new ArgumentException(L.T("full address 접속 주소가 없습니다."));
        int port=3389;string separatePort;
        if(fields.TryGetValue("server port",out separatePort)&&!Int32.TryParse(separatePort,NumberStyles.None,CultureInfo.InvariantCulture,out port))throw new ArgumentException(L.T("server port 값이 올바르지 않습니다."));
        int colon=address.LastIndexOf(':');
        if(colon>=0) {
            if(address.IndexOf(':')!=colon||address.StartsWith("["))throw new ArgumentException(L.T("현재 IPv4 또는 호스트 이름만 가져올 수 있습니다."));
            if(!Int32.TryParse(address.Substring(colon+1),NumberStyles.None,CultureInfo.InvariantCulture,out port))throw new ArgumentException(L.T("주소 뒤의 포트가 올바르지 않습니다."));
            address=address.Substring(0,colon).Trim();
        }
        string name=Path.GetFileNameWithoutExtension(path);if(name.Length>110)name=name.Substring(0,110);
        SurfaceEntry entry=new SurfaceEntry(group,name,address,port);
        // Missing user stays blank: do not silently convert ordinary Windows RDP accounts to surface.
        string user,domain;fields.TryGetValue("username",out user);fields.TryGetValue("domain",out domain);
        entry.UserName=user??"";entry.Domain=domain??"";
        SurfaceRdg.Validate(new List<SurfaceEntry>{entry});return entry;
    }
    public static RdpImportResult Import(string[] paths,string group,IList<SurfaceEntry> existing) {
        if(String.IsNullOrWhiteSpace(group)||group.Trim().Length>128)throw new ArgumentException(L.T("추가할 그룹 이름을 1~128자로 입력하세요."));
        group=group.Trim();XmlConvert.VerifyXmlChars(group);
        RdpImportResult result=new RdpImportResult();
        HashSet<string> endpoints=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        HashSet<string> names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(SurfaceEntry e in existing){endpoints.Add(e.Host+":"+e.Port);names.Add(e.Group+"\n"+e.Name);}
        foreach(string path in paths) {
            try {
                SurfaceEntry e=Read(path,group);
                if(!endpoints.Add(e.Host+":"+e.Port)){result.Skipped++;continue;}
                string baseName=e.Name;int suffix=2;
                while(!names.Add(group+"\n"+e.Name))e.Name=baseName+" ("+(suffix++)+")";
                result.Added.Add(e);
            }catch(Exception ex) {
                if(!(ex is ArgumentException||ex is IOException||ex is UnauthorizedAccessException||ex is XmlException))throw;
                result.Errors.Add(Path.GetFileName(path)+": "+ex.Message);
            }
        }
        return result;
    }
}

public sealed class RdgManagerForm:Form {
    readonly DataGridView grid=new DataGridView();
    readonly TextBox title=new TextBox();
    readonly ComboBox importGroup=new ComboBox();
    readonly string currentHost;readonly int currentPort;
    public RdgManagerForm(string host,int port) {
        currentHost=host;currentPort=port;Text=L.T("WinHomeDesk — RDCMan 그룹 파일 만들기");
        Font=new Font("맑은 고딕",10);ClientSize=new Size(920,590);MinimumSize=Size;StartPosition=FormStartPosition.CenterParent;
        Controls.Add(new Label{Text=L.T("여러 서피스를 그룹별로 등록하고 .rdg 파일로 저장하세요."),AutoSize=true,Location=new Point(18,16)});
        Controls.Add(new Label{Text=L.T("목록 이름"),AutoSize=true,Location=new Point(18,54)});
        title.Text=L.T("내 서피스");title.SetBounds(110,49,400,29);Controls.Add(title);
        Controls.Add(new Label{Text=L.T("RDP 추가 그룹"),AutoSize=true,Location=new Point(535,54)});
        importGroup.SetBounds(650,49,250,29);importGroup.Text=L.T("서피스");Controls.Add(importGroup);
        grid.SetBounds(18,94,884,300);grid.Anchor=AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;
        grid.AllowUserToAddRows=true;grid.AllowUserToDeleteRows=true;grid.AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill;
        grid.SelectionMode=DataGridViewSelectionMode.FullRowSelect;grid.MultiSelect=true;grid.RowHeadersWidth=30;
        grid.Columns.Add("group",L.T("그룹"));grid.Columns.Add("name",L.T("기기 이름"));grid.Columns.Add("host",L.T("IP / 호스트 이름"));grid.Columns.Add("port",L.T("포트"));
        grid.Columns.Add("user",L.T("사용자 이름"));grid.Columns.Add("domain","Domain");
        grid.Columns[0].FillWeight=16;grid.Columns[1].FillWeight=22;grid.Columns[2].FillWeight=26;grid.Columns[3].FillWeight=10;grid.Columns[4].FillWeight=16;grid.Columns[5].FillWeight=12;
        grid.DefaultValuesNeeded+=delegate(object sender,DataGridViewRowEventArgs e){e.Row.Cells[0].Value=importGroup.Text;e.Row.Cells[3].Value=3390;e.Row.Cells[4].Value="surface";};
        grid.CellEndEdit+=delegate{RefreshGroups();};
        Controls.Add(grid);
        Button add=ButtonAt(L.T("이 PC 추가"),18,410,105);add.Click+=delegate{AddCurrent();};
        Button remove=ButtonAt(L.T("선택 행 삭제"),133,410,120);remove.Click+=delegate{foreach(DataGridViewRow row in grid.SelectedRows)if(!row.IsNewRow)grid.Rows.Remove(row);RefreshGroups();};
        Button load=ButtonAt(L.T("RDG 불러오기"),263,410,140);load.Click+=LoadList;
        Button import=ButtonAt(L.T("RDP 파일 추가"),413,410,150);import.Click+=ImportRdp;
        Button save=ButtonAt(L.T("RDG 파일 저장"),573,410,150);save.Click+=SaveList;
        Button close=ButtonAt(L.T("닫기"),733,410,110);close.Click+=delegate{Close();};
        Label help=new Label {Text=L.T("RDP 추가 그룹을 선택하거나 입력한 뒤 'RDP 파일 추가'에서 여러 파일을 선택하세요.\n기존 목록에 주소·포트·계정을 추가합니다. 같은 주소와 포트는 중복 추가하지 않습니다.\n암호·게이트웨이·자동 암호 입력창 설정은 가져오지 않습니다.\n저장한 RDG는 RDCMan의 File → Open으로 열고, 연결 시 Shift+Enter로 암호를 입력하세요.\nWinHomeDesk 계정은 surface / Domain 빈칸 / 기본 포트 3390입니다."),AutoSize=false,Location=new Point(18,458),Size=new Size(884,118),Anchor=AnchorStyles.Left|AnchorStyles.Right|AnchorStyles.Bottom};Controls.Add(help);
        AddCurrent();
    }
    Button ButtonAt(string text,int x,int y,int width) {
        Button b=new Button{Text=text,Location=new Point(x,y),Size=new Size(width,34),Anchor=AnchorStyles.Left|AnchorStyles.Bottom};Controls.Add(b);return b;
    }
    void AddCurrent() {
        foreach(DataGridViewRow row in grid.Rows)if(!row.IsNewRow&&Convert.ToString(row.Cells[2].Value)==currentHost&&Convert.ToString(row.Cells[3].Value)==currentPort.ToString()) {grid.CurrentCell=row.Cells[2];return;}
        grid.Rows.Add(L.T("서피스"),Environment.MachineName,currentHost,currentPort,"surface","");RefreshGroups();
    }
    void RefreshGroups() {
        string selected=importGroup.Text;importGroup.Items.Clear();
        foreach(DataGridViewRow row in grid.Rows) {string name=Convert.ToString(row.Cells[0].Value).Trim();if(name.Length>0&&!importGroup.Items.Contains(name))importGroup.Items.Add(name);}
        importGroup.Text=selected;
    }
    List<SurfaceEntry> ReadRows(bool allowEmpty=false) {
        grid.EndEdit();List<SurfaceEntry> result=new List<SurfaceEntry>();
        foreach(DataGridViewRow row in grid.Rows) {
            if(row.IsNewRow)continue;
            int port;if(!Int32.TryParse(Convert.ToString(row.Cells[3].Value),out port))throw new ArgumentException((row.Index+1)+L.T("행: 포트를 숫자로 입력하세요."));
            SurfaceEntry entry=new SurfaceEntry(Convert.ToString(row.Cells[0].Value),Convert.ToString(row.Cells[1].Value),Convert.ToString(row.Cells[2].Value),port);
            entry.UserName=Convert.ToString(row.Cells[4].Value);entry.Domain=Convert.ToString(row.Cells[5].Value);result.Add(entry);
        }
        if(!allowEmpty||result.Count>0)SurfaceRdg.Validate(result);return result;
    }
    void ImportRdp(object sender,EventArgs e) {
        try {
            List<SurfaceEntry> existing=ReadRows(true);
            using(OpenFileDialog d=new OpenFileDialog{Filter=L.T("원격 데스크톱 연결|*.rdp"),Multiselect=true,Title=L.T("그룹에 추가할 RDP 파일 선택")}) {
                if(d.ShowDialog(this)!=DialogResult.OK)return;
                RdpImportResult result=SurfaceRdpImport.Import(d.FileNames,importGroup.Text,existing);
                foreach(SurfaceEntry row in result.Added)grid.Rows.Add(row.Group,row.Name,row.Host,row.Port,row.UserName,row.Domain);
                RefreshGroups();
                string message=L.T("추가 ")+result.Added.Count+L.T("대 / 중복 제외 ")+result.Skipped+L.T("개 / 읽기 실패 ")+result.Errors.Count+L.T("개");
                if(result.Errors.Count>0)message+="\n\n"+String.Join("\n",result.Errors.GetRange(0,Math.Min(result.Errors.Count,10)).ToArray());
                MessageBox.Show(this,message,L.T("RDP 파일 추가 결과"));
            }
        }catch(Exception ex){MessageBox.Show(this,ex.Message,L.T("RDP 파일 확인"));}
    }
    void SaveList(object sender,EventArgs e) {
        try {
            List<SurfaceEntry> rows=ReadRows();
            using(SaveFileDialog d=new SaveFileDialog{Filter=L.T("RDCMan 그룹 파일|*.rdg"),FileName="WinHomeDesk-Surfaces.rdg"}) {
                if(d.ShowDialog(this)!=DialogResult.OK)return;
                SurfaceRdg.Save(d.FileName,rows,title.Text);
                MessageBox.Show(this,L.T("저장했습니다.\n")+d.FileName+L.T("\n\n관리 PC의 RDCMan에서 File → Open으로 여세요.\n각 서버의 Logon Credentials에 전용 암호를 입력하면 연결할 수 있습니다.\n이 파일에는 암호가 저장되지 않습니다."),L.T("RDG 저장 완료"));
            }
        }catch(Exception ex){MessageBox.Show(this,ex.Message,L.T("목록 확인"));}
    }
    void LoadList(object sender,EventArgs e) {
        using(OpenFileDialog d=new OpenFileDialog{Filter=L.T("RDCMan 그룹 파일|*.rdg")}) {
            if(d.ShowDialog(this)!=DialogResult.OK)return;
            try {
                string loadedTitle;List<SurfaceEntry> rows=SurfaceRdg.Load(d.FileName,out loadedTitle);
                if(MessageBox.Show(this,L.T("현재 표를 선택한 파일의 기기 목록으로 바꿉니다.\n그룹·기기명·주소·포트·계정만 가져오며, 암호와 기타 RDCMan 설정은 가져오지 않습니다.\n원본 파일을 보존하려면 다른 이름으로 저장하세요."),L.T("목록 불러오기"),MessageBoxButtons.OKCancel)!=DialogResult.OK)return;
                grid.Rows.Clear();foreach(SurfaceEntry row in rows)grid.Rows.Add(row.Group,row.Name,row.Host,row.Port,row.UserName,row.Domain);title.Text=loadedTitle;RefreshGroups();
            }catch(Exception ex){MessageBox.Show(this,ex.Message,L.T("목록 확인"));}
        }
    }
}
