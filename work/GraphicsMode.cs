using System;
using System.Drawing;
using System.Windows.Forms;

public static class GraphicsMode {
    public static string Arguments(int mode) {
        if(mode<0 || mode>3)throw new ArgumentOutOfRangeException("mode");
        string common=" -rfx -nsc +bitmap-compat -gfx-progressive";
        if(mode==3)return common+" -gfx -gfx-avc420 -gfx-avc444";
        return common+" +gfx +gfx-planar "+(mode==2?"-gfx-avc420":"+gfx-avc420")+
            (mode==1?" +gfx-avc444":" -gfx-avc444");
    }
    public static string Caption(string line) {
        const string prefix="SurfaceDesk CODEC name=";
        if(!line.StartsWith(prefix,StringComparison.Ordinal))return null;
        string value=line.Substring(prefix.Length).Split(' ')[0];
        switch(value) {
            case "avc420":return "실제: H.264 AVC420";
            case "avc444":return "실제: H.264 AVC444 / CPU";
            case "avc444v2":return "실제: H.264 AVC444v2 / CPU";
            case "gfx-planar":return "실제: GFX Planar / CPU";
            case "gfx-raw":return "실제: GFX 무압축";
            case "bitmap":return "실제: 호환 비트맵 / CPU";
            default:return null;
        }
    }
}
public sealed partial class SurfaceDesk {
    readonly ComboBox graphicsMode=new ComboBox();
    readonly Label actualCodec=new Label();
    void InitializeGraphics() {
        Controls.Add(new Label {Text="전송 방식",AutoSize=true,Location=new Point(22,499)});
        graphicsMode.SetBounds(112,494,365,30);graphicsMode.DropDownStyle=ComboBoxStyle.DropDownList;
        graphicsMode.Items.AddRange(new object[]{"자동 — H.264 AVC420 우선 (기본)","글자 선명도 — H.264 AVC444 우선","GFX Planar — 무손실 색상 압축","호환 비트맵 — 기존 전송 방식"});
        graphicsMode.SelectedIndex=0;Controls.Add(graphicsMode);
        actualCodec.Text="실제 전송: 연결 대기";actualCodec.SetBounds(494,498,274,27);
        actualCodec.Font=new Font(Font.FontFamily,9);Controls.Add(actualCodec);
    }
}
