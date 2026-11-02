using System;using System.IO;using System.Drawing;using System.Windows.Forms;using System.Runtime.InteropServices;
class RdpHost:AxHost {public RdpHost():base("{A0C63C30-F08D-4AB4-907C-34905D770C7D}"){} public object Client{get{return GetOcx();}}}
class MicrosoftProbe:Form {
    RdpHost host=new RdpHost();Timer timer=new Timer();string report;bool connected,login;
    void Log(string s){File.AppendAllText(report,s+Environment.NewLine);}
    public MicrosoftProbe(string password,string file) {
        report=file;SetBounds(-16000,-16000,800,600);ShowInTaskbar=false;Opacity=0;
        host.Dock=DockStyle.Fill;Controls.Add(host);
        Shown+=delegate {
            try {
                dynamic client=host.Client;
                Guid events=new Guid("336d5562-efa8-482e-8cb3-c5c0fc7a7db6");
                ComEventsHelper.Combine(host.Client,events,2,new Action(delegate{connected=true;Log("OnConnected");}));
                ComEventsHelper.Combine(host.Client,events,3,new Action(delegate{login=true;Log("OnLoginComplete");}));
                ComEventsHelper.Combine(host.Client,events,4,new Action<int>(delegate(int reason){Log("OnDisconnected="+reason);try{Log("Description="+client.GetErrorDescription(reason,client.ExtendedDisconnectReason));}catch{}}));
                client.Server="127.0.0.1";client.UserName="surface";
                client.ColorDepth=Environment.GetEnvironmentVariable("SURFACE_MS_COLOR")=="16"?16:32;
                client.AdvancedSettings2.RDPPort=13390;client.AdvancedSettings2.ClearTextPassword=password;
                client.AdvancedSettings7.EnableCredSspSupport=true;
                client.AdvancedSettings2.AuthenticationLevel=0; // loopback test only
                client.AdvancedSettings7.AudioRedirectionMode=0;
                if(Environment.GetEnvironmentVariable("SURFACE_MS_HIGH")!=null)client.AdvancedSettings7.AudioQualityMode=2;
                client.Connect();timer.Interval=Environment.GetEnvironmentVariable("SURFACE_MS_AUDIO_TEST")!=null?18000:9000;timer.Start();
            }catch(Exception ex){Log("ERROR="+ex);Environment.ExitCode=2;Close();}
        };
        timer.Tick+=delegate {
            timer.Stop();Log("connected="+connected+" login="+login);
            try{dynamic client=host.Client;client.Disconnect();}catch{}
            Environment.ExitCode=connected?0:1;Close();
        };
    }
    [STAThread]static void Main(string[] a){Application.EnableVisualStyles();Application.Run(new MicrosoftProbe(a[0],a[1]));}
}
