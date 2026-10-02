using System.Configuration;
using System.Data;
using System.Windows;

namespace ZionLauncher;

/// <summary>
/// Interaction logic for App.xaml
/// </summary>
public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        AppDomain.CurrentDomain.UnhandledException += (s, ev) =>
        {
            try { System.IO.File.WriteAllText(System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher_crash.txt"), ev.ExceptionObject.ToString()); } catch { }
        };
        DispatcherUnhandledException += (s, ev) =>
        {
            try { System.IO.File.WriteAllText(System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher_crash.txt"), ev.Exception.ToString()); } catch { }
        };
        base.OnStartup(e);
    }
}

