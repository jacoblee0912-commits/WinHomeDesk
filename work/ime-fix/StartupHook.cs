using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

// A process-local .NET startup hook. No global hooks, key capture or binary patches.
public static class StartupHook {
    static System.Threading.Timer waitForForm;
    static int scheduled;
    public static void Initialize() {
        // Do not pass this process-specific hook to programs launched by RDCMan.
        Environment.SetEnvironmentVariable("DOTNET_STARTUP_HOOKS",Environment.GetEnvironmentVariable("WINHOME_DESK_PREVIOUS_HOOKS"));
        Environment.SetEnvironmentVariable("WINHOME_DESK_PREVIOUS_HOOKS",null);
        var entry=Assembly.GetEntryAssembly();
        if(entry==null || entry.GetName().Name!="RDCMan")return;
        var version=entry.GetName().Version;
        if(version.Major!=3 || version.Minor!=22)return;
        ImeController.WriteLog("startup RDCMan="+version+" engine=RDCore retained");
        waitForForm=new System.Threading.Timer(_=>{
            try {
                // RDCMan creates its UI on a separate STA after Main starts.
                var program=entry.GetType("RdcMan.Program");
                var property=program?.GetProperty("TheForm",BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Static);
                var form=property?.GetValue(null) as Form;
                if(form==null || form.IsDisposed || !form.IsHandleCreated)return;
                if(Interlocked.CompareExchange(ref scheduled,1,0)!=0)return;
                try {form.BeginInvoke((Action)(()=>{ImeController.Install();waitForForm?.Dispose();}));}
                catch {Interlocked.Exchange(ref scheduled,0);}
            } catch(Exception e) { ImeController.WriteLog("startup-wait "+e.GetType().Name); }
        },null,100,250);
    }
}

public sealed class ImeController : IMessageFilter {
    const string ViewType="RDCMan.RDCoreConnectionUserControl";
    static ImeController instance;
    readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer();
    readonly HashSet<Control> views=new HashSet<Control>();
    bool applying;
    public int ProtectedViews {get{return views.Count;}}
    [DllImport("imm32")] static extern bool ImmAssociateContextEx(IntPtr hwnd,IntPtr context,uint flags);
    [DllImport("imm32")] static extern IntPtr ImmGetContext(IntPtr hwnd);
    [DllImport("imm32")] static extern bool ImmReleaseContext(IntPtr hwnd,IntPtr context);
    [DllImport("user32")] static extern IntPtr GetFocus();
    [DllImport("user32")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("kernel32")] static extern uint GetCurrentThreadId();
    [DllImport("user32")] static extern bool EnumChildWindows(IntPtr hwnd,EnumWindowsProc proc,IntPtr state);
    delegate bool EnumWindowsProc(IntPtr hwnd,IntPtr state);

    public static void WriteLog(string message) {
        try {
            var path=Environment.GetEnvironmentVariable("WINHOME_DESK_IME_LOG");
            if(!String.IsNullOrEmpty(path))File.AppendAllText(path,DateTime.Now.ToString("s")+" "+message+Environment.NewLine);
        }catch { /* Logging must never interrupt input. */ }
    }
    public static ImeController Install() {
        if(instance!=null)return instance;
        instance=new ImeController();
        Application.AddMessageFilter(instance);
        instance.timer.Interval=200;
        instance.timer.Tick+=(s,e)=>instance.Refresh();
        instance.timer.Start();
        Application.ApplicationExit+=(s,e)=>{instance.timer.Dispose();Application.RemoveMessageFilter(instance);};
        instance.Refresh();WriteLog("active: remote viewport local IME disabled; keys forwarded to remote IME");
        return instance;
    }
    static bool IsRemoteView(Control control) {
        return control!=null && control.GetType().FullName==ViewType &&
               control.GetType().Assembly.GetName().Name=="RDCMan";
    }
    static Control FindRemoteView(IntPtr hwnd) {
        var c=Control.FromChildHandle(hwnd);
        for(;c!=null;c=c.Parent)if(IsRemoteView(c))return c;
        return null;
    }
    public void Refresh() {
        if(applying)return;
        try {
            // On the RDCMan UI thread: include docked, undocked and full-screen forms.
            foreach(Form form in Application.OpenForms)Visit(form);
            foreach(var view in new List<Control>(views))if(view.IsDisposed)views.Remove(view);
        }catch(Exception e){WriteLog("refresh "+e.GetType().Name);}
    }
    void Visit(Control c) {
        if(c.IsDisposed)return;
        if(IsRemoteView(c)){
            if(views.Add(c)){
                c.HandleCreated+=(s,e)=>Apply(c);
                c.GotFocus+=(s,e)=>Apply(c);
                c.Disposed+=(s,e)=>views.Remove(c);
                WriteLog("viewport-attached count="+views.Count);
            }
            Apply(c);return;
        }
        foreach(Control child in c.Controls)Visit(child);
    }
    void DisableNative(IntPtr hwnd) {
        uint pid;
        if(GetWindowThreadProcessId(hwnd,out pid)!=GetCurrentThreadId())return;
        // Do not close a shared HIMC, which could switch unrelated local editors.
        // Detach it only from this remote-display window.
        var imc=ImmGetContext(hwnd);
        if(imc==IntPtr.Zero)return;
        ImmReleaseContext(hwnd,imc);
        if(!ImmAssociateContextEx(hwnd,IntPtr.Zero,0))WriteLog("IME detach failed");
    }
    public void Apply(Control view) {
        if(applying || !IsRemoteView(view) || view.IsDisposed || !view.IsHandleCreated)return;
        applying=true;
        try {
            if(view.ImeMode!=ImeMode.Disable)view.ImeMode=ImeMode.Disable;
            DisableNative(view.Handle);
            // Children can be created after connection/reconnection.
            EnumChildWindows(view.Handle,(hwnd,state)=>{DisableNative(hwnd);return true;},IntPtr.Zero);
        }catch(Exception e){WriteLog("viewport-apply "+e.GetType().Name);}
        finally{applying=false;}
    }
    public bool PreFilterMessage(ref Message message) {
        // Before TranslateMessage can turn a physical key into local IME composition.
        // Never inspect key values, suppress messages, or touch credential/search dialogs.
        try {
            if(message.Msg==0x100 || message.Msg==0x104 || message.Msg==0x51 || message.Msg==7 || message.Msg==0x281) {
                var view=FindRemoteView(message.HWnd);
                if(view!=null)Apply(view);
            }
        }catch(Exception e){WriteLog("input-filter "+e.GetType().Name);}
        return false;
    }
}
