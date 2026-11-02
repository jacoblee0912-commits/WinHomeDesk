using System;
using System.Drawing;
using System.Windows.Forms;

public sealed partial class SurfaceDesk {
    readonly CheckBox audioEnabled=new CheckBox();
    readonly Label audioStatus=new Label();
    void InitializeAudio() {
        audioEnabled.Text="시스템 소리 전송";audioEnabled.Checked=true;
        audioEnabled.SetBounds(22,574,230,30);Controls.Add(audioEnabled);
        audioStatus.Text="소리 전송: 서버 시작 대기";audioStatus.Font=new Font(Font.FontFamily,9);
        audioStatus.SetBounds(260,575,508,38);Controls.Add(audioStatus);
    }
    public static string AudioCaption(string line) {
        if(line.StartsWith("SurfaceDesk AUDIO_LATENCY ",StringComparison.Ordinal)) {
            double age=AudioValue(line,"server_age_ms"),hold=AudioValue(line,"client_hold_ms"),confirmed=AudioValue(line,"confirmed");
            if(age<0 || hold<0 || confirmed<0)return null;
            if(confirmed==0)return "소리 전송 중 · 클라이언트 재생 확인 대기";
            return "소리: 서버 약 "+age.ToString("0")+"ms / 클라이언트 보고 "+hold.ToString("0")+"ms";
        }
        if(line.StartsWith("SurfaceDesk AUDIO state=capturing ",StringComparison.Ordinal))return "소리 캡처 중 · 44.1kHz 스테레오 / 연결 대기";
        if(line.StartsWith("SurfaceDesk AUDIO state=error ",StringComparison.Ordinal))return "소리 캡처 실패 · 출력 장치 확인 / 자동 재시도";
        if(line=="SurfaceDesk AUDIO state=device-changed")return "출력 장치 변경 · 소리 캡처 다시 시작 중";
        if(line=="SurfaceDesk AUDIO state=disabled")return "소리 전송: 꺼짐";
        if(line=="SurfaceDesk AUDIO state=stopped")return "소리 전송: 중지";
        if(line.StartsWith("SurfaceDesk AUDIO_CHANNEL state=ready ",StringComparison.Ordinal))return "원격 오디오 채널 준비됨 · 44.1kHz 스테레오";
        if(line.StartsWith("SurfaceDesk AUDIO_CHANNEL state=client-disabled",StringComparison.Ordinal))return "클라이언트에서 소리 꺼짐 · 이 컴퓨터에서 재생으로 설정";
        if(line.StartsWith("SurfaceDesk AUDIO_CHANNEL state=unsupported",StringComparison.Ordinal))return "클라이언트가 지원하는 오디오 형식 없음";
        if(line.StartsWith("SurfaceDesk AUDIO_CHANNEL state=send-error",StringComparison.Ordinal))return "오디오 전송 오류 · 연결을 끊고 다시 접속하세요";
        if(line=="SurfaceDesk AUDIO_CHANNEL state=init-error")return "오디오 채널 시작 실패 · 다시 접속하세요";
        if(line.StartsWith("SurfaceDesk AUDIO_SENT frames=",StringComparison.Ordinal))return "소리 전송 중 · 원격 PC의 음량도 확인하세요";
        return null;
    }
    static double AudioValue(string line,string key) {
        foreach(string token in line.Split(' '))if(token.StartsWith(key+"=",StringComparison.Ordinal)) {
            double value;
            if(Double.TryParse(token.Substring(key.Length+1),System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out value)
                && !Double.IsNaN(value) && !Double.IsInfinity(value) && value>=0)return value;
        }
        return -1;
    }
}
