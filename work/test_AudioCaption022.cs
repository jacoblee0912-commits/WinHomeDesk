using System;
class TestAudioCaption022 {
    static void Check(bool value){if(!value)throw new Exception("audio caption failed");}
    static void Main() {
        Check(SurfaceDesk.AudioCaption("SurfaceDesk AUDIO_LATENCY confirmed=0 server_age_ms=12.5 client_hold_ms=0").Contains("대기"));
        Check(SurfaceDesk.AudioCaption("SurfaceDesk AUDIO_LATENCY confirmed=20 server_age_ms=12.5 client_hold_ms=350").Contains("350ms"));
        Check(SurfaceDesk.AudioCaption("SurfaceDesk AUDIO_LATENCY confirmed=20 server_age_ms=NaN client_hold_ms=350")==null);
        Check(SurfaceDesk.AudioCaption("SurfaceDesk AUDIO_LATENCY confirmed=20 client_hold_ms=350")==null);
        Console.WriteLine("Audio UI PASS: pending confirmation, reported delay and malformed metrics");
    }
}
