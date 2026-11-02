using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using System.Xml;

public static class L {
    static readonly Dictionary<string,string> english=new Dictionary<string,string>(StringComparer.Ordinal);
    static readonly Dictionary<string,string> korean=new Dictionary<string,string>(StringComparer.Ordinal);
    public static string Language {get;private set;}
    public static int Count {get {return english.Count;}}
    static L() {
        Language="ko";
        using(var stream=typeof(L).Assembly.GetManifestResourceStream("WinHomeDesk.Translations.xml")) {
            if(stream==null)throw new InvalidOperationException("Missing embedded translations.");
            var doc=new XmlDocument {XmlResolver=null};
            using(var r=XmlReader.Create(stream,new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null}))doc.Load(r);
            foreach(XmlNode node in doc.SelectNodes("/translations/text")) {
                string ko=node["ko"].InnerText,en=node["en"].InnerText;english.Add(ko,en);
                if(!korean.ContainsKey(en))korean.Add(en,ko);
            }
        }
    }
    public static void Set(string value){if(value!="ko" && value!="en")throw new ArgumentException("Language must be ko or en.");Language=value;}
    public static string T(string source){string result;return Language=="en"&&english.TryGetValue(source,out result)?result:source;}
    public static string Exact(string text) {
        if(text==null)return null;string source;
        if(english.ContainsKey(text))return T(text);
        if(korean.TryGetValue(text,out source))return T(source);
        return text;
    }
    public static string PreferencesPath {get {return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SurfaceDesk","preferences.xml");}}
    public static void LoadPreference(){try{Set(new DesktopOptionsStore(PreferencesPath).Load().Language);}catch{Set("ko");}}
    public static void ApplyStatic(Control control) {
        if(control is Label || control is ButtonBase || control is Form)control.Text=Exact(control.Text);
        var combo=control as ComboBox;
        if(combo!=null && combo.DropDownStyle==ComboBoxStyle.DropDownList && combo.Name!="LanguageSelector") {
            int selected=combo.SelectedIndex;
            for(int i=0;i<combo.Items.Count;i++){string old=combo.Items[i] as string;if(old!=null){string translated=Exact(old);if(translated!=old)combo.Items[i]=translated;}}
            combo.SelectedIndex=selected;
        }
        var grid=control as DataGridView;
        if(grid!=null)foreach(DataGridViewColumn column in grid.Columns)column.HeaderText=Exact(column.HeaderText);
        foreach(Control child in control.Controls)ApplyStatic(child);
    }
    public static void ApplyMenu(ToolStripItemCollection items) {
        foreach(ToolStripItem item in items){item.Text=Exact(item.Text);var menu=item as ToolStripMenuItem;if(menu!=null)ApplyMenu(menu.DropDownItems);}
    }
}

public sealed partial class SurfaceDesk {
    readonly ComboBox languageSelector=new ComboBox();
    bool changingLanguage;
    string lastAudioLine;
    void InitializeLanguageSelector() {
        var label=new Label {Text="Language",AutoSize=true,Location=new System.Drawing.Point(548,55),Font=new System.Drawing.Font(Font.FontFamily,9)};
        Controls.Add(label);
        languageSelector.Name="LanguageSelector";languageSelector.DropDownStyle=ComboBoxStyle.DropDownList;
        languageSelector.SetBounds(625,49,143,30);languageSelector.Items.AddRange(new object[]{"한국어","English"});
        languageSelector.SelectedIndex=L.Language=="en"?1:0;Controls.Add(languageSelector);
        languageSelector.SelectedIndexChanged+=delegate {
            if(changingLanguage)return;
            string previous=L.Language,chosen=languageSelector.SelectedIndex==1?"en":"ko";
            try {
                if(!testMode) {
                    // Save only language; do not re-register startup or capture unsaved server fields.
                    var saved=preferenceStore.Load();saved.Language=chosen;preferenceStore.Save(saved);preferences.Language=chosen;
                }
                ApplyLanguage(chosen);
            }catch(Exception ex) {
                changingLanguage=true;languageSelector.SelectedIndex=previous=="en"?1:0;changingLanguage=false;
                MessageBox.Show(this,L.T("언어 설정을 저장하지 못했습니다.\n")+ex.Message,"WinHomeDesk");
            }
        };
    }
    public void ApplyLanguage(string code) {
        L.Set(code);changingLanguage=true;languageSelector.SelectedIndex=code=="en"?1:0;changingLanguage=false;
        L.ApplyStatic(this);
        Text=L.T("WinHomeDesk — 독립 RDP 서버 0.3.0 · 실험 버전")+(StartupTask.IsElevated?L.T(" · 관리자 권한"):L.T(" · 일반 권한"));
        performance.Relocalize();RefreshPerformance();UpdateStatus();
        if(lastAudioLine!=null)audioStatus.Text=AudioCaption(lastAudioLine);
        if(tray!=null)L.ApplyMenu(tray.ContextMenuStrip.Items);RefreshTray();
    }
}
