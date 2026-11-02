using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
class TestGraphics018 {
    static void Check(bool ok,string msg){if(!ok)throw new Exception(msg);}
    [STAThread] static void Main(string[] args) {
        string path=Path.Combine(args[0],"work","graphics-preferences-test.xml");
        File.WriteAllText(path,"<DesktopOptions><Version>1</Version><Port>3390</Port><CaptureMode>auto</CaptureMode></DesktopOptions>");
        var store=new DesktopOptionsStore(path);var p=store.Load();
        Check(p.GraphicsMode==0,"Old settings must migrate to auto");
        Check(p.AudioEnabled,"Old settings migrate to system audio enabled");
        p.AudioEnabled=false;store.Save(p);Check(!store.Load().AudioEnabled,"Persist audio disabled");
        p.AudioEnabled=true;store.Save(p);Check(store.Load().AudioEnabled,"Persist audio enabled");
        p.GraphicsMode=1;store.Save(p);Check(store.Load().GraphicsMode==1,"Persist AVC444");
        p.GraphicsMode=4;bool rejected=false;try{store.Save(p);}catch(InvalidDataException){rejected=true;}Check(rejected,"Invalid mode accepted");
        using(var form=new SurfaceDesk(false,true)) {
            var field=typeof(SurfaceDesk).GetField("graphicsMode",BindingFlags.Instance|BindingFlags.NonPublic);
            var modes=(ComboBox)field.GetValue(form);modes.SelectedIndex=1;
            var audio=(CheckBox)typeof(SurfaceDesk).GetField("audioEnabled",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(form);
            audio.Checked=false;
            var encoder=(ComboBox)typeof(SurfaceDesk).GetField("encoderMode",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(form);
            Check(encoder.SelectedIndex==0,"GPU auto should default on");encoder.SelectedIndex=1;
            var get=typeof(SurfaceDesk).GetMethod("CurrentOptions",BindingFlags.Instance|BindingFlags.NonPublic);
            Check(((DesktopOptions)get.Invoke(form,new object[]{false})).GraphicsMode==1,"UI selection was not saved");
            Check(((DesktopOptions)get.Invoke(form,new object[]{false})).EncoderMode=="cpu","Encoder mode not saved");
            Check(!((DesktopOptions)get.Invoke(form,new object[]{false})).AudioEnabled,"Audio UI flag not saved");
            var running=typeof(SurfaceDesk).GetMethod("SetRunning",BindingFlags.Instance|BindingFlags.NonPublic);
            running.Invoke(form,new object[]{true});Check(!modes.Enabled,"Live codec change must be disabled");
            Check(!encoder.Enabled,"Live encoder change must be disabled");
            Check(!audio.Enabled,"Audio change while running must be disabled");
            running.Invoke(form,new object[]{false});Check(modes.Enabled,"Stopped codec selection should be enabled");
            Check(audio.Enabled,"Audio option not restored");
            form.ExitApplication();
        }
        Check(GraphicsMode.Caption("SurfaceDesk CODEC name=avc444v2 encoder=cpu").Contains("AVC444v2"),"Actual codec caption");
        Check(GraphicsMode.Caption("SurfaceDesk CODEC name=unknown encoder=cpu")==null,"Unknown codec mislabelled");
        Check(SurfaceDesk.EncoderCaption("SurfaceDesk ENCODER backend=gpu api=media-foundation").Contains("GPU"),"GPU status");
        Check(SurfaceDesk.EncoderCaption("SurfaceDesk ENCODER backend=cpu reason=gpu-init-failed").Contains("CPU"),"fallback status");
        File.Delete(path);Console.WriteLine("Graphics: old settings migration, persisted selection, running controls, actual codec parsing PASS");
    }
}
