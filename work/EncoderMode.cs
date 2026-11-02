using System;
using System.Drawing;
using System.Windows.Forms;

public sealed partial class SurfaceDesk {
    readonly ComboBox encoderMode=new ComboBox();
    readonly Label actualEncoder=new Label();
    void InitializeEncoder() {
        Controls.Add(new Label {Text=L.T("영상 압축"),AutoSize=true,Location=new Point(22,539)});
        encoderMode.SetBounds(112,534,365,30);encoderMode.DropDownStyle=ComboBoxStyle.DropDownList;
        encoderMode.Items.AddRange(new object[]{L.T("GPU 우선 — 사용 불가 시 CPU (AVC420)"),L.T("CPU — 호환 모드")});
        encoderMode.SelectedIndex=0;Controls.Add(encoderMode);
        actualEncoder.Text=L.T("인코더: 연결 대기");actualEncoder.SetBounds(494,538,274,30);
        actualEncoder.Font=new Font(Font.FontFamily,9);Controls.Add(actualEncoder);
    }
    public static string EncoderCaption(string line) {
        if(line.StartsWith("SurfaceDesk ENCODER backend=gpu ",StringComparison.Ordinal))return L.T("사용 중: GPU H.264");
        if(line.StartsWith("SurfaceDesk ENCODER backend=cpu reason=",StringComparison.Ordinal))return L.T("사용 중: CPU (GPU 대체)");
        if(line.StartsWith("SurfaceDesk ENCODER profile=",StringComparison.Ordinal))return L.T("사용 중: CPU H.264");
        if(line.StartsWith("SurfaceDesk CODEC name=gfx-",StringComparison.Ordinal)||line.StartsWith("SurfaceDesk CODEC name=bitmap ",StringComparison.Ordinal))return L.T("사용 중: CPU (비 H.264)");
        return null;
    }
}
