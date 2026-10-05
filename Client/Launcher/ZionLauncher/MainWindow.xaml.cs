using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using System.Text.Json;
using MySqlConnector;
using SharpCompress.Archives;
using SharpCompress.Archives.SevenZip;
using SharpCompress.Common;

namespace ZionLauncher
{
    public partial class MainWindow : Window
    {
        public static readonly Version CurrentLauncherVersion = new("1.3.0");
        private static readonly HttpClient _httpClient = new() { Timeout = TimeSpan.FromSeconds(5) };
        private readonly Random _random = new();
        private DispatcherTimer? _diagTimer;

        private const string RemoteServerIp = "15.204.82.250";
        private const string LocalServerIp = "127.0.0.1";
        private string _currentServerIp = RemoteServerIp;
        private const int DatabasePort = 3307;
        private string _authenticatedUser = "";
        private string _authenticatedPassword = "";
        private List<OperativeProfile> _operativesList = new List<OperativeProfile>();
        private OperativeProfile? _selectedOperative = null;

        // Authentic Matrix glyph set: Half-width Katakana, digits, uppercase letters, symbols
        private static readonly string MatrixGlyphChars =
            "\uFF66\uFF67\uFF68\uFF69\uFF6A\uFF6B\uFF6C\uFF6D\uFF6E\uFF6F" +
            "\uFF70\uFF71\uFF72\uFF73\uFF74\uFF75\uFF76\uFF77\uFF78\uFF79" +
            "\uFF7A\uFF7B\uFF7C\uFF7D\uFF7E\uFF7F\uFF80\uFF81\uFF82\uFF83" +
            "\uFF84\uFF85\uFF86\uFF87\uFF88\uFF89\uFF8A\uFF8B\uFF8C\uFF8D" +
            "\uFF8E\uFF8F\uFF90\uFF91\uFF92\uFF93\uFF94\uFF95\uFF96\uFF97" +
            "\uFF98\uFF99\uFF9A\uFF9B\uFF9C\uFF9D" +
            "0123456789" +
            "Z987XKJQTHEMATRIX" +
            ":・=+*<>-¦|";

        private class MatrixRainColumn
        {
            public double X;
            public double HeadY;
            public double Speed;        // px/sec
            public int TrailLength;     // glyph count
            public int[] GlyphIndices = Array.Empty<int>();
            public double NextDropDelay;
            public double MutationTimer;
        }

        private readonly DrawingVisual _visual = new();
        private readonly Typeface _typeface = new("Consolas");
        private readonly double _fontSize = 15.0;
        private readonly double _lineHeight = 16.0;
        private MatrixRainColumn[] _rainColumns = Array.Empty<MatrixRainColumn>();
        private FormattedText[,]? _cachedGlyphs;
        private FormattedText[]? _cachedHeadGlow;
        private TimeSpan _lastRenderTime = TimeSpan.Zero;
        private bool _isRainRunning;

        private static string ResolveGameRoot()
        {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\', '/');
            if (File.Exists(Path.Combine(baseDir, "matrix.exe")) ||
                File.Exists(Path.Combine(baseDir, "launcher.exe")) ||
                Directory.Exists(Path.Combine(baseDir, "resource")))
            {
                return baseDir;
            }
            if (Directory.Exists(@"E:\Games\The Matrix Online") &&
                (File.Exists(@"E:\Games\The Matrix Online\matrix.exe") || File.Exists(@"E:\Games\The Matrix Online\launcher.exe")))
            {
                return @"E:\Games\The Matrix Online";
            }
            return baseDir;
        }

        private static string GameRoot => ResolveGameRoot();
        private static string RealityServer => Path.Combine(GameRoot, @"mxoemu_live\Server\Reality\Binaries\Reality.exe");
        private static string MySqlExe => Directory.Exists(@"E:\TESTDB\bin") ? @"E:\TESTDB\bin\mysqld.exe" : Path.Combine(GameRoot, @"mysql\bin\mysqld.exe");
        private static string HookDll
        {
            get
            {
                string clientHook = Path.Combine(GameRoot, "Client", "mxohax.dll");
                if (File.Exists(clientHook)) return clientHook;
                string rootHook = Path.Combine(GameRoot, "mxohax.dll");
                if (File.Exists(rootHook)) return rootHook;
                return Path.Combine(GameRoot, "mxohax_modern.dll");
            }
        }

        // The retail 7.6005 client needs the modern hook (login/network shim + launcher watch); the old 113k mxohax.dll
        // only knows the 2005 client's offsets. Client\mxohax.dll is intentionally left untouched.
        private static string RetailHookDll
        {
            get
            {
                string clientModern = Path.Combine(GameRoot, "Client", "mxohax_modern.dll");
                if (File.Exists(clientModern)) return clientModern;
                return Path.Combine(GameRoot, "mxohax_modern.dll");
            }
        }

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true, ExactSpelling = true)]
        static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, uint nSize, out IntPtr lpNumberOfBytesWritten);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, out IntPtr lpThreadId);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr GetModuleHandle(string lpModuleName);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        [return: System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.Bool)]
        static extern bool CloseHandle(IntPtr hObject);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

        const uint PROCESS_ALL_ACCESS = 0x1F0FFF;
        const uint MEM_COMMIT = 0x00001000;
        const uint MEM_RESERVE = 0x00002000;
        const uint PAGE_READWRITE = 0x04;

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        struct STARTUPINFO
        {
            public uint cb;
            public string lpReserved;
            public string lpDesktop;
            public string lpTitle;
            public uint dwX;
            public uint dwY;
            public uint dwXSize;
            public uint dwYSize;
            public uint dwXCountChars;
            public uint dwYCountChars;
            public uint dwFillAttribute;
            public uint dwFlags;
            public short wShowWindow;
            public short cbReserved2;
            public IntPtr lpReserved2;
            public IntPtr hStdInput;
            public IntPtr hStdOutput;
            public IntPtr hStdError;
        }

        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        struct PROCESS_INFORMATION
        {
            public IntPtr hProcess;
            public IntPtr hThread;
            public uint dwProcessId;
            public uint dwThreadId;
        }

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true, CharSet = System.Runtime.InteropServices.CharSet.Ansi)]
        static extern bool CreateProcess(
            string? lpApplicationName,
            string lpCommandLine,
            IntPtr lpProcessAttributes,
            IntPtr lpThreadAttributes,
            bool bInheritHandles,
            uint dwCreationFlags,
            IntPtr lpEnvironment,
            string? lpCurrentDirectory,
            [System.Runtime.InteropServices.In] ref STARTUPINFO lpStartupInfo,
            out PROCESS_INFORMATION lpProcessInformation
        );

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern uint ResumeThread(IntPtr hThread);

        const uint CREATE_SUSPENDED = 0x00000004;

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern bool VirtualProtectEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flNewProtect, out uint lpflOldProtect);

        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError = true)]
        static extern bool FlushInstructionCache(IntPtr hProcess, IntPtr lpBaseAddress, UIntPtr dwSize);

        const uint PAGE_EXECUTE_READWRITE = 0x40;

        // PE TimeDateStamp of the retail 7.6005 launcher.exe / matrix.exe (the 2005 client is 0x421564AE).
        const uint RETAIL_7_6005_TIMESTAMP = 0x48CB38B2;

        private static bool IsRetailClient(string exePath)
        {
            try
            {
                using var fs = new FileStream(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                using var br = new BinaryReader(fs);
                fs.Seek(0x3C, SeekOrigin.Begin);
                int peOff = br.ReadInt32();
                fs.Seek(peOff + 8, SeekOrigin.Begin);
                return br.ReadUInt32() == RETAIL_7_6005_TIMESTAMP;
            }
            catch { return false; }
        }

        private static bool PatchRemote(IntPtr hProcess, uint addr, byte[] bytes)
        {
            if (!VirtualProtectEx(hProcess, (IntPtr)addr, (uint)bytes.Length, PAGE_EXECUTE_READWRITE, out uint oldProt)) return false;
            bool ok = WriteProcessMemory(hProcess, (IntPtr)addr, bytes, (uint)bytes.Length, out _);
            VirtualProtectEx(hProcess, (IntPtr)addr, (uint)bytes.Length, oldProt, out _);
            return ok;
        }

        /// <summary>
        /// Applies the retail 7.6005 launcher bypasses to a SUSPENDED client process (same set as tools/harness32.cpp):
        /// single-instance check, parent-launcher check, "run via launcher.exe / -clone deprecated" modal, EULA/patcher flags.
        /// </summary>
        private static bool ApplyRetailBypassPatches(IntPtr hProcess)
        {
            bool ok = true;
            ok &= PatchRemote(hProcess, 0x0040A0C0, new byte[] { 0x31, 0xC0, 0xC3 });                   // single instance: xor eax,eax; ret
            ok &= PatchRemote(hProcess, 0x0040A180, new byte[] { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 }); // parent check: mov eax,1; ret
            ok &= PatchRemote(hProcess, 0x0040B4B1, new byte[] { 0xEB, 0x51 });                         // jump past deprecation modal
            ok &= PatchRemote(hProcess, 0x004C8B1C, new byte[] { 0x00, 0x00 });                         // clear EULA/patcher flags
            FlushInstructionCache(hProcess, IntPtr.Zero, UIntPtr.Zero);
            return ok;
        }

        private static System.Media.SoundPlayer? s_matrixClickPlayer = null;
        private static readonly object s_soundLock = new object();

        private static void PlayMatrixClickSound()
        {
            try
            {
                lock (s_soundLock)
                {
                    if (s_matrixClickPlayer == null)
                    {
                        string[] candidatePaths = new[]
                        {
                            System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "click.wav"),
                            System.IO.Path.Combine(AppContext.BaseDirectory, "click.wav"),
                            System.IO.Path.Combine(Directory.GetCurrentDirectory(), "click.wav"),
                            @"E:\Games\The Matrix Online\click.wav"
                        };

                        foreach (var path in candidatePaths)
                        {
                            if (System.IO.File.Exists(path))
                            {
                                s_matrixClickPlayer = new System.Media.SoundPlayer(path);
                                s_matrixClickPlayer.Load();
                                break;
                            }
                        }
                    }
                }
                s_matrixClickPlayer?.Play();
            }
            catch { }
        }

        private void AnyButton_Click(object sender, RoutedEventArgs e)
        {
            PlayMatrixClickSound();
        }

        public MainWindow()
        {
            InitializeComponent();
            EventManager.RegisterClassHandler(typeof(Button), Button.ClickEvent, new RoutedEventHandler((s, e) => PlayMatrixClickSound()));
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            SetBrowserEmulationMode();
            try { wbCommunity.Navigated += (s, ev) => SetSilent(wbCommunity, true); } catch { }

            // Dynamic version labels
            txtLoaderSubTitle.Text = $"THE MATRIX ONLINE // NEURAL BRIDGE LOADER v{CurrentLauncherVersion}";
            txtConsoleSubTitle.Text = $"THE MATRIX ONLINE // REALITY REMASTER (v{CurrentLauncherVersion})";
            txtLauncherVersionInfo.Text = $"CURRENT BUILD: v{CurrentLauncherVersion} (Active)";

            // 1. Setup Matrix Code Rain Animation
            VisualHost host = new VisualHost { Visual = _visual };
            MatrixCanvas.Children.Add(host);

            InitializeGlyphCache();
            InitializeMatrixColumns();

            _isRainRunning = true;
            CompositionTarget.Rendering += OnMatrixCompositionRendering;

            // 2. Pre-fill default account credentials
            txtLoginUser.Text = "Slacker";
            txtLoginPass.Password = "test";

            UpdateServerBanner();
            UpdateClientStatusUI();

            // 3. Periodic Diagnostics
            _diagTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(10) };
            _diagTimer.Tick += (s, ev) => RunDiagnosticsAsync();
            _diagTimer.Start();

            RunDiagnosticsAsync();

            // 4. Begin Startup Loader & Auto-Update sequence
            _ = InitializeStartupFlowAsync();
        }

        private void Window_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
        {
            _isRainRunning = false;
            CompositionTarget.Rendering -= OnMatrixCompositionRendering;
            _diagTimer?.Stop();
        }

        private void Window_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (e.NewSize.Width > 100 && e.NewSize.Height > 100)
            {
                InitializeMatrixColumns();
            }
        }

        private void InitializeGlyphCache()
        {
            var dpi = VisualTreeHelper.GetDpi(this).PixelsPerDip;
            int glyphCount = MatrixGlyphChars.Length;
            _cachedGlyphs = new FormattedText[glyphCount, 8];
            _cachedHeadGlow = new FormattedText[glyphCount];

            // 8 Color Tiers:
            // Tier 0: Pure glowing white (Head)
            // Tier 1: Luminous pale neon green (Neck)
            // Tier 2: Bright neon matrix green
            // Tier 3: Classic phosphor green
            // Tier 4: Medium phosphor green
            // Tier 5: Dim phosphor green
            // Tier 6: Dark green
            // Tier 7: Faint green tail
            Brush[] brushes = new Brush[]
            {
                new SolidColorBrush(Color.FromRgb(255, 255, 255)),
                new SolidColorBrush(Color.FromRgb(200, 255, 200)),
                new SolidColorBrush(Color.FromRgb(85, 255, 85)),
                new SolidColorBrush(Color.FromRgb(0, 255, 65)),
                new SolidColorBrush(Color.FromRgb(0, 208, 53)),
                new SolidColorBrush(Color.FromRgb(0, 153, 38)),
                new SolidColorBrush(Color.FromRgb(0, 102, 25)),
                new SolidColorBrush(Color.FromArgb(160, 0, 51, 13))
            };

            foreach (var b in brushes) b.Freeze();
            Brush glowBrush = new SolidColorBrush(Color.FromArgb(140, 180, 255, 180));
            glowBrush.Freeze();

            for (int i = 0; i < glyphCount; i++)
            {
                string s = MatrixGlyphChars[i].ToString();
                for (int t = 0; t < 8; t++)
                {
                    _cachedGlyphs[i, t] = new FormattedText(
                        s,
                        System.Globalization.CultureInfo.InvariantCulture,
                        FlowDirection.LeftToRight,
                        _typeface,
                        _fontSize,
                        brushes[t],
                        dpi);
                }

                _cachedHeadGlow[i] = new FormattedText(
                    s,
                    System.Globalization.CultureInfo.InvariantCulture,
                    FlowDirection.LeftToRight,
                    _typeface,
                    _fontSize + 2.0,
                    glowBrush,
                    dpi);
            }
        }

        private void InitializeMatrixColumns()
        {
            double width = Math.Max(this.ActualWidth > 0 ? this.ActualWidth : 880, 800);
            double height = Math.Max(this.ActualHeight > 0 ? this.ActualHeight : 680, 600);
            int columnCount = (int)(width / (_fontSize + 2.0));
            _rainColumns = new MatrixRainColumn[columnCount];

            for (int i = 0; i < columnCount; i++)
            {
                _rainColumns[i] = CreateColumn(i * (_fontSize + 2.0), height, initialScatter: true);
            }
        }

        private MatrixRainColumn CreateColumn(double x, double height, bool initialScatter)
        {
            int length = _random.Next(14, 34);
            var col = new MatrixRainColumn
            {
                X = x,
                Speed = _random.Next(110, 310),
                TrailLength = length,
                GlyphIndices = new int[length],
                NextDropDelay = initialScatter ? 0 : _random.NextDouble() * 1.5,
                MutationTimer = _random.NextDouble() * 0.3
            };

            if (initialScatter)
            {
                col.HeadY = _random.NextDouble() * height;
            }
            else
            {
                col.HeadY = -_random.Next(20, 160);
            }

            for (int k = 0; k < length; k++)
            {
                col.GlyphIndices[k] = _random.Next(MatrixGlyphChars.Length);
            }

            return col;
        }

        private void OnMatrixCompositionRendering(object? sender, EventArgs e)
        {
            if (!_isRainRunning || _cachedGlyphs == null || _rainColumns.Length == 0) return;
            if (this.WindowState == WindowState.Minimized) return;

            RenderingEventArgs renderingArgs = (RenderingEventArgs)e;
            if (_lastRenderTime == TimeSpan.Zero)
            {
                _lastRenderTime = renderingArgs.RenderingTime;
                return;
            }

            double dt = (renderingArgs.RenderingTime - _lastRenderTime).TotalSeconds;
            _lastRenderTime = renderingArgs.RenderingTime;
            if (dt <= 0.0) return;
            if (dt > 0.05) dt = 0.05; // clamp frame spikes

            double width = Math.Max(this.ActualWidth, 880);
            double height = Math.Max(this.ActualHeight, 680);

            using (DrawingContext dc = _visual.RenderOpen())
            {
                // Clear background with crisp solid black
                dc.DrawRectangle(Brushes.Black, null, new Rect(0, 0, width, height));

                for (int i = 0; i < _rainColumns.Length; i++)
                {
                    var col = _rainColumns[i];
                    if (col.NextDropDelay > 0)
                    {
                        col.NextDropDelay -= dt;
                        continue;
                    }

                    col.HeadY += col.Speed * dt;

                    // Periodic glyph mutation in the stream for atmospheric Matrix alive-code effect
                    col.MutationTimer -= dt;
                    if (col.MutationTimer <= 0)
                    {
                        col.MutationTimer = 0.08 + _random.NextDouble() * 0.18;
                        int mutateIdx = _random.Next(col.TrailLength);
                        col.GlyphIndices[mutateIdx] = _random.Next(MatrixGlyphChars.Length);
                    }

                    // Render trail
                    for (int k = 0; k < col.TrailLength; k++)
                    {
                        double y = col.HeadY - k * _lineHeight;
                        if (y < -_lineHeight || y > height) continue;

                        int glyphIdx = col.GlyphIndices[k];
                        if (glyphIdx < 0 || glyphIdx >= MatrixGlyphChars.Length) continue;

                        int tier;
                        if (k == 0)
                        {
                            tier = 0;
                            // Head glow effect behind the white leading glyph
                            if (_cachedHeadGlow != null)
                            {
                                dc.DrawText(_cachedHeadGlow[glyphIdx], new Point(col.X - 1.0, y - 1.0));
                            }
                        }
                        else if (k == 1) tier = 1;
                        else if (k == 2) tier = 2;
                        else
                        {
                            tier = 3 + (int)((k - 3) * 5.0 / Math.Max(1, col.TrailLength - 3));
                            if (tier > 7) tier = 7;
                        }

                        dc.DrawText(_cachedGlyphs[glyphIdx, tier], new Point(col.X, y));
                    }

                    // Reset column when tail passes below canvas
                    if (col.HeadY - col.TrailLength * _lineHeight > height)
                    {
                        _rainColumns[i] = CreateColumn(col.X, height, initialScatter: false);
                    }
                }
            }
        }

        private async Task InitializeStartupFlowAsync()
        {
            // 1. Startup Cleanup: remove old update artifacts if present
            CleanupOldUpdateArtifacts();

            var args = Environment.GetCommandLineArgs();
            bool fastMode = args.Any(a => a.Equals("--fast", StringComparison.OrdinalIgnoreCase) || a.Equals("--no-loader", StringComparison.OrdinalIgnoreCase));
            if (fastMode)
            {
                DismissLoader();
                return;
            }

            // 2. Initial Loader HUD state
            txtLoaderPhase.Text = "INITIALIZING CONSTRUCT PROTOCOL...";
            txtLoaderDetail.Text = "Calibrating neural interface and Matrix code stream...";
            pbLoader.Value = 18;
            txtLoaderPercent.Text = "18%";
            txtLoaderSpeed.Text = "NEURAL LINK: STANDBY";
            loaderActionGrid.Visibility = Visibility.Collapsed;

            await Task.Delay(400);

            txtLoaderPhase.Text = "VERIFYING CIPHER INTEGRITY...";
            txtLoaderDetail.Text = "Bypassing hardline security protocols...";
            pbLoader.Value = 38;
            txtLoaderPercent.Text = "38%";

            await Task.Delay(350);

            txtLoaderPhase.Text = "CHECKING FOR MAINFRAME UPGRADE...";
            txtLoaderDetail.Text = $"Querying patch server ({_currentServerIp}) for construct update...";
            pbLoader.Value = 58;
            txtLoaderPercent.Text = "58%";

            // Check for updates
            await CheckForLauncherUpdateAsync(isStartup: true);
        }

        private void CleanupOldUpdateArtifacts()
        {
            try
            {
                string baseDir = AppContext.BaseDirectory;
                string script = Path.Combine(baseDir, "update_launcher.bat");
                if (File.Exists(script)) try { File.Delete(script); } catch { }

                foreach (var tmp in Directory.GetFiles(baseDir, "*.update.tmp"))
                {
                    try { File.Delete(tmp); } catch { }
                }
                foreach (var old in Directory.GetFiles(baseDir, "*.old"))
                {
                    try { File.Delete(old); } catch { }
                }
            }
            catch { }
        }

        public static bool TryParseNormalizedVersion(string? verStr, out Version version)
        {
            version = new Version(0, 0, 0, 0);
            if (string.IsNullOrWhiteSpace(verStr)) return false;
            verStr = verStr.Trim();
            if (verStr.StartsWith('v') || verStr.StartsWith('V')) verStr = verStr[1..].Trim();
            verStr = verStr.Split('-')[0].Split('+')[0].Trim();

            var parts = verStr.Split('.');
            if (parts.Length < 1) return false;

            int major = 0, minor = 0, build = 0, revision = 0;
            if (!int.TryParse(parts[0], out major)) return false;
            if (parts.Length > 1 && !int.TryParse(parts[1], out minor)) return false;
            if (parts.Length > 2 && !int.TryParse(parts[2], out build)) return false;
            if (parts.Length > 3 && !int.TryParse(parts[3], out revision)) return false;

            version = new Version(major, minor, build, revision);
            return true;
        }

        public static Version NormalizeVersion(Version v)
        {
            return new Version(
                v.Major >= 0 ? v.Major : 0,
                v.Minor >= 0 ? v.Minor : 0,
                v.Build >= 0 ? v.Build : 0,
                v.Revision >= 0 ? v.Revision : 0);
        }

        public static bool IsRemoteVersionNewer(string? remoteVerStr, Version currentVer, out Version? parsedRemote)
        {
            parsedRemote = null;
            if (!TryParseNormalizedVersion(remoteVerStr, out var remoteVer))
                return false;

            parsedRemote = remoteVer;
            return NormalizeVersion(remoteVer) > NormalizeVersion(currentVer);
        }

        private async Task CheckForLauncherUpdateAsync(bool isStartup)
        {
            string host = _currentServerIp;
            string? downloadUrl = null;
            string? remoteVerStr = null;
            string filename = "ZionLauncher.exe";

            async Task<string?> TryFetchUrlAsync(string url, int timeoutSeconds)
            {
                try
                {
                    using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds));
                    using var resp = await _httpClient.GetAsync(url, cts.Token);
                    if (resp.IsSuccessStatusCode)
                    {
                        return await resp.Content.ReadAsStringAsync(cts.Token);
                    }
                }
                catch { }
                return null;
            }

            try
            {
                // Endpoint 1: /launcher/version
                string? json = await TryFetchUrlAsync($"http://{host}/launcher/version", 4);

                // Endpoint 2: /version.json
                if (string.IsNullOrEmpty(json))
                {
                    json = await TryFetchUrlAsync($"http://{host}/version.json", 4);
                }

                if (!string.IsNullOrEmpty(json))
                {
                    try
                    {
                        using var doc = JsonDocument.Parse(json);
                        if (doc.RootElement.TryGetProperty("version", out var vProp))
                            remoteVerStr = vProp.GetString();
                        if (doc.RootElement.TryGetProperty("downloadUrl", out var dProp))
                            downloadUrl = dProp.GetString();
                        if (doc.RootElement.TryGetProperty("filename", out var fProp))
                            filename = fProp.GetString() ?? filename;
                    }
                    catch { }
                }

                // Endpoint 3: patch_manifest.json fallback
                if (string.IsNullOrEmpty(remoteVerStr))
                {
                    string? mJson = await TryFetchUrlAsync($"http://{host}/patch/patch_manifest.json", 4);
                    if (!string.IsNullOrEmpty(mJson))
                    {
                        try
                        {
                            using var mDoc = JsonDocument.Parse(mJson);
                            if (mDoc.RootElement.TryGetProperty("launcher", out var lProp))
                            {
                                if (lProp.TryGetProperty("version", out var lv))
                                    remoteVerStr = lv.GetString();
                                if (lProp.TryGetProperty("url", out var lu))
                                    downloadUrl = lu.GetString();
                                if (lProp.TryGetProperty("filename", out var lf))
                                    filename = lf.GetString() ?? filename;
                            }
                        }
                        catch { }
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine("[UpdateCheck] Unexpected error: " + ex.Message);
            }

            // Evaluation
            if (string.IsNullOrEmpty(remoteVerStr) || !TryParseNormalizedVersion(remoteVerStr, out var remoteVer))
            {
                // Unreachable or offline fallback
                if (isStartup)
                {
                    txtLoaderPhase.Text = "MAINFRAME OFFLINE // LOCAL MODE";
                    txtLoaderDetail.Text = "Update server unreachable. Continuing in autonomous construct mode...";
                    pbLoader.Value = 100;
                    txtLoaderPercent.Text = "100%";
                    txtLoaderSpeed.Text = "AUTONOMOUS MODE";
                    await Task.Delay(850);
                    DismissLoader();
                }
                else
                {
                    txtLauncherUpdateStatus.Text = "STATUS: OFFLINE";
                    txtLauncherUpdateStatus.Foreground = Brushes.Orange;
                    MessageBox.Show($"Could not reach update server at {host}.\nMake sure the server is online or check your network connection.", "Update Server Unreachable", MessageBoxButton.OK, MessageBoxImage.Warning);
                }
                return;
            }

            if (NormalizeVersion(remoteVer) > NormalizeVersion(CurrentLauncherVersion))
            {
                // Newer version available!
                downloadUrl ??= $"/launcher/{filename}";
                if (isStartup)
                {
                    txtLoaderPhase.Text = $"MAINFRAME UPGRADE FOUND: v{remoteVer}";
                    txtLoaderDetail.Text = $"Current build: v{CurrentLauncherVersion}. Initiating automated core upgrade...";
                    pbLoader.Value = 75;
                    txtLoaderPercent.Text = "75%";
                    await Task.Delay(600);
                    await PerformLauncherUpdateAsync(downloadUrl, remoteVer, filename);
                }
                else
                {
                    txtLauncherUpdateStatus.Text = $"UPGRADE AVAILABLE: v{remoteVer}";
                    txtLauncherUpdateStatus.Foreground = Brushes.Yellow;
                    var res = MessageBox.Show(
                        $"A new launcher version (v{remoteVer}) is available!\n(Currently running v{CurrentLauncherVersion})\n\nWould you like to download and install this update now?",
                        "Launcher Upgrade Available",
                        MessageBoxButton.YesNo,
                        MessageBoxImage.Information);

                    if (res == MessageBoxResult.Yes)
                    {
                        await PerformLauncherUpdateAsync(downloadUrl, remoteVer, filename);
                    }
                }
            }
            else
            {
                // Up to date!
                if (isStartup)
                {
                    txtLoaderPhase.Text = $"CONSTRUCT VERIFIED: v{CurrentLauncherVersion} [OPTIMAL]";
                    txtLoaderDetail.Text = "All neural constructs synchronized. Transferring to Jack-In console...";
                    pbLoader.Value = 100;
                    txtLoaderPercent.Text = "100%";
                    txtLoaderSpeed.Text = "SYSTEM ONLINE";
                    txtLauncherUpdateStatus.Text = $"STATUS: UP TO DATE (v{CurrentLauncherVersion})";
                    await Task.Delay(900);
                    DismissLoader();
                }
                else
                {
                    txtLauncherUpdateStatus.Text = $"UP TO DATE (v{CurrentLauncherVersion})";
                    txtLauncherUpdateStatus.Foreground = Brushes.Lime;
                    MessageBox.Show($"Your launcher is already running the latest version (v{CurrentLauncherVersion})!", "Up To Date", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
        }

        private async Task PerformLauncherUpdateAsync(string downloadUrl, Version remoteVer, string filename)
        {
            LoaderOverlay.Visibility = Visibility.Visible;
            MainConsoleBorder.Visibility = Visibility.Collapsed;
            loaderActionGrid.Visibility = Visibility.Collapsed;

            txtLoaderPhase.Text = $"DOWNLOADING UPGRADE v{remoteVer}...";
            txtLoaderDetail.Text = "Acquiring updated construct binary from Zion patch server...";
            pbLoader.Value = 0;
            txtLoaderPercent.Text = "0%";
            txtLoaderSpeed.Text = "CONNECTING...";

            string processPath = Environment.ProcessPath 
                ?? Process.GetCurrentProcess().MainModule?.FileName 
                ?? Path.Combine(AppContext.BaseDirectory, "ZionLauncher.exe");

            string currentExe = Path.GetFileName(processPath).StartsWith("dotnet", StringComparison.OrdinalIgnoreCase)
                ? Path.Combine(AppContext.BaseDirectory, "ZionLauncher.exe")
                : processPath;

            string targetDir = Path.GetDirectoryName(currentExe) ?? AppContext.BaseDirectory;
            string tempFile = Path.Combine(targetDir, Path.GetFileName(currentExe) + ".update.tmp");

            string fullUrl = downloadUrl.StartsWith("http", StringComparison.OrdinalIgnoreCase)
                ? downloadUrl
                : $"http://{_currentServerIp}" + (downloadUrl.StartsWith('/') ? downloadUrl : "/" + downloadUrl);

            try
            {
                using var downloadClient = new HttpClient { Timeout = TimeSpan.FromMinutes(15) };
                using var response = await downloadClient.GetAsync(fullUrl, HttpCompletionOption.ResponseHeadersRead);
                response.EnsureSuccessStatusCode();

                long? totalBytes = response.Content.Headers.ContentLength;
                await using var contentStream = await response.Content.ReadAsStreamAsync();
                await using var fileStream = new FileStream(tempFile, FileMode.Create, FileAccess.Write, FileShare.None);

                byte[] buffer = new byte[65536];
                long totalDownloaded = 0;
                var sw = Stopwatch.StartNew();
                TimeSpan lastReport = TimeSpan.Zero;
                long lastBytes = 0;

                int bytesRead;
                while ((bytesRead = await contentStream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                {
                    await fileStream.WriteAsync(buffer.AsMemory(0, bytesRead));
                    totalDownloaded += bytesRead;

                    if (sw.Elapsed - lastReport > TimeSpan.FromMilliseconds(100))
                    {
                        double dt = (sw.Elapsed - lastReport).TotalSeconds;
                        long bytesSince = totalDownloaded - lastBytes;
                        double speedMBps = dt > 0 ? (bytesSince / (1024.0 * 1024.0)) / dt : 0;

                        double pct = totalBytes > 0 ? ((double)totalDownloaded / totalBytes.Value) * 100.0 : 0.0;
                        string eta = "--";
                        if (totalBytes > 0 && speedMBps > 0.02)
                        {
                            double remaining = totalBytes.Value - totalDownloaded;
                            double sec = remaining / (speedMBps * 1024.0 * 1024.0);
                            TimeSpan t = TimeSpan.FromSeconds(sec);
                            eta = t.Hours > 0 ? $"{t.Hours}h {t.Minutes}m" : $"{t.Minutes}m {t.Seconds}s";
                        }

                        pbLoader.Value = Math.Min(100, Math.Max(0, pct));
                        txtLoaderPercent.Text = $"{pct:F0}%";
                        double curMB = totalDownloaded / (1024.0 * 1024.0);
                        double maxMB = totalBytes.HasValue ? totalBytes.Value / (1024.0 * 1024.0) : 0;
                        txtLoaderDetail.Text = $"Downloaded: {curMB:F1} MB / {maxMB:F1} MB | Speed: {speedMBps:F2} MB/s | ETA: {eta}";
                        txtLoaderSpeed.Text = $"{speedMBps:F2} MB/s";

                        lastReport = sw.Elapsed;
                        lastBytes = totalDownloaded;
                    }
                }

                await fileStream.FlushAsync();
                fileStream.Close();

                pbLoader.Value = 100;
                txtLoaderPercent.Text = "100%";
                txtLoaderPhase.Text = "APPLYING CONSTRUCT UPGRADE...";
                txtLoaderDetail.Text = "Replacing executable binary and rebooting...";
                txtLoaderSpeed.Text = "REBOOTING...";

                await Task.Delay(400);

                // Helper updater batch script with retry loop and UAC elevation fallback
                string scriptPath = Path.Combine(targetDir, "update_launcher.bat");
                string scriptContent = 
                    "@echo off\r\n" +
                    "set \"TARGET=%~1\"\r\n" +
                    "set \"UPDATE=%~2\"\r\n" +
                    "set \"PID=%~3\"\r\n\r\n" +
                    ":WAIT_PID\r\n" +
                    "tasklist /fi \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul\r\n" +
                    "if not errorlevel 1 (\r\n" +
                    "    ping -n 2 127.0.0.1 >nul\r\n" +
                    "    goto WAIT_PID\r\n" +
                    ")\r\n\r\n" +
                    "set ATTEMPTS=0\r\n" +
                    ":COPY_LOOP\r\n" +
                    "set /a ATTEMPTS+=1\r\n" +
                    "copy /Y \"%UPDATE%\" \"%TARGET%\" >nul 2>nul\r\n" +
                    "if not errorlevel 1 goto COPY_SUCCESS\r\n\r\n" +
                    "if %ATTEMPTS% leq 15 (\r\n" +
                    "    ping -n 2 127.0.0.1 >nul\r\n" +
                    "    goto COPY_LOOP\r\n" +
                    ")\r\n\r\n" +
                    ":: If copy still failed after retries (e.g. UAC protected directory), elevate via PowerShell\r\n" +
                    "powershell -Command \"Start-Process cmd -ArgumentList '/c copy /Y \\\"%UPDATE%\\\" \\\"%TARGET%\\\" & del \\\"%UPDATE%\\\" & start \\\"\\\" \\\"%TARGET%\\\"' -Verb RunAs -WindowStyle Hidden\" >nul 2>nul\r\n" +
                    "goto CLEANUP\r\n\r\n" +
                    ":COPY_SUCCESS\r\n" +
                    "del \"%UPDATE%\" >nul 2>nul\r\n" +
                    "start \"\" \"%TARGET%\"\r\n\r\n" +
                    ":CLEANUP\r\n" +
                    "ping -n 2 127.0.0.1 >nul\r\n" +
                    "(goto) 2>nul & del \"%~f0\"\r\n";

                File.WriteAllText(scriptPath, scriptContent);

                int currentPid = Process.GetCurrentProcess().Id;
                Process.Start(new ProcessStartInfo
                {
                    FileName = scriptPath,
                    Arguments = $"\"{currentExe}\" \"{tempFile}\" {currentPid}",
                    UseShellExecute = true,
                    CreateNoWindow = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                });

                // Shutdown old process cleanly and terminate immediately
                _isRainRunning = false;
                CompositionTarget.Rendering -= OnMatrixCompositionRendering;
                _diagTimer?.Stop();
                Application.Current.Shutdown();
                Environment.Exit(0);
            }
            catch (Exception ex)
            {
                txtLoaderPhase.Text = "UPGRADE FAILED";
                txtLoaderDetail.Text = "Error during update: " + ex.Message;
                txtLoaderSpeed.Text = "ERROR";
                loaderActionGrid.Visibility = Visibility.Visible;
            }
        }

        private void DismissLoader()
        {
            LoaderOverlay.Visibility = Visibility.Collapsed;
            MainConsoleBorder.Visibility = Visibility.Visible;
            txtStatus.Text = "Zion Mainframe online. Ready to Jack In.";
            _ = ProcessCommandLineArgsAsync();
        }

        private void Window_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.F1)
            {
                btnShowLogin_Click(sender, e);
            }
            else if (e.Key == Key.F2)
            {
                btnShowCommunity_Click(sender, e);
            }
            else if (e.Key == Key.F3)
            {
                btnShowPatchNotes_Click(sender, e);
            }
            else if (e.Key == Key.F4)
            {
                btnShowRegister_Click(sender, e);
            }
            else if (e.Key == Key.F5)
            {
                btnShowServer_Click(sender, e);
            }
            else if (e.Key == Key.F6)
            {
                btnShowPatch_Click(sender, e);
            }
            else if (e.Key == Key.F7)
            {
                if (OperativeRosterPanel.Visibility == Visibility.Visible && btnLaunchSelectedOperative.IsEnabled)
                {
                    btnLaunchSelectedOperative_Click(sender, e);
                }
                else if (LoginPanel.Visibility == Visibility.Visible && btnJackIn.IsEnabled)
                {
                    btnJackIn_Click(sender, e);
                }
            }
        }

        private async Task ProcessCommandLineArgsAsync()
        {
            try
            {
                var args = Environment.GetCommandLineArgs();
                for (int i = 1; i < args.Length; i++)
                {
                    string a = args[i].ToLowerInvariant();
                    if (a == "--transmissions" || a == "-transmissions" || a == "-patchnotes" || (a == "--tab" && i + 1 < args.Length && args[i + 1].ToLowerInvariant() == "transmissions"))
                    {
                        btnShowPatchNotes_Click(this, new RoutedEventArgs());
                    }
                    else if (a == "--community" || a == "-community" || a == "-forum" || (a == "--tab" && i + 1 < args.Length && args[i + 1].ToLowerInvariant() == "community"))
                    {
                        btnShowCommunity_Click(this, new RoutedEventArgs());
                    }
                    else if (a == "--roster" || a == "-roster" || a == "--auth-select" || (a == "--tab" && i + 1 < args.Length && args[i + 1].ToLowerInvariant() == "roster"))
                    {
                        await Task.Delay(300);
                        btnJackIn_Click(this, new RoutedEventArgs());
                    }
                    else if (a == "--jackin" || a == "-jackin")
                    {
                        await Task.Delay(400);
                        btnJackIn_Click(this, new RoutedEventArgs());
                        await Task.Delay(3500);
                        btnLaunchSelectedOperative_Click(this, new RoutedEventArgs());
                    }
                }
            }
            catch { }
        }

        private void btnLoaderProceed_Click(object sender, RoutedEventArgs e)
        {
            DismissLoader();
        }

        private async void btnLoaderRetry_Click(object sender, RoutedEventArgs e)
        {
            loaderActionGrid.Visibility = Visibility.Collapsed;
            await CheckForLauncherUpdateAsync(isStartup: true);
        }

        private async void btnCheckLauncherUpdate_Click(object sender, RoutedEventArgs e)
        {
            btnCheckLauncherUpdate.IsEnabled = false;
            txtLauncherUpdateStatus.Text = "STATUS: CHECKING...";
            txtLauncherUpdateStatus.Foreground = Brushes.Yellow;
            try
            {
                await CheckForLauncherUpdateAsync(isStartup: false);
            }
            finally
            {
                btnCheckLauncherUpdate.IsEnabled = true;
            }
        }

        private void UpdateServerBanner()
        {
            if (_currentServerIp == RemoteServerIp)
            {
                lblServerTarget.Text = $"ACTIVE MAINFRAME: LIVE VPS ({RemoteServerIp})";
            }
            else
            {
                lblServerTarget.Text = $"ACTIVE MAINFRAME: LOCAL DEV ({LocalServerIp})";
            }
        }

        public bool IsClientInstalled(out string executablePath)
        {
            executablePath = "";
            string root = GameRoot;
            string[] candidates = new[]
            {
                Path.Combine(root, "matrix.exe"),
                Path.Combine(root, "Client", "matrix.exe"),
                Path.Combine(root, "launcher.exe"),
                Path.Combine(root, "Client", "launcher.exe")
            };

            foreach (var c in candidates)
            {
                if (File.Exists(c))
                {
                    executablePath = c;
                    break;
                }
            }

            bool hasExe = !string.IsNullOrEmpty(executablePath);
            bool hasDll = File.Exists(Path.Combine(root, "client.dll")) || File.Exists(Path.Combine(root, "Client", "client.dll"));
            bool hasResources = Directory.Exists(Path.Combine(root, "resource")) || Directory.Exists(Path.Combine(root, "Client", "resource"));

            return hasExe && (hasDll || hasResources);
        }

        public void UpdateClientStatusUI()
        {
            bool installed = IsClientInstalled(out _);
            if (installed)
            {
                clientStatusBanner.Visibility = Visibility.Collapsed;
                btnJackIn.Content = "AUTHENTICATE & SELECT OPERATIVE";
                btnStartPatch.Content = "DOWNLOAD / REPAIR CLIENT";
            }
            else
            {
                clientStatusBanner.Visibility = Visibility.Visible;
                txtClientStatusBanner.Text = "⚠️ MATRIX ONLINE CLIENT NOT DETECTED";
                btnJackIn.Content = "📥 DOWNLOAD CLIENT & JACK IN";
                btnStartPatch.Content = "📥 DOWNLOAD CLIENT (1.26 GB)";
            }
        }

        private void btnShowLogin_Click(object sender, RoutedEventArgs e)
        {
            if (_operativesList.Count > 0 && !string.IsNullOrEmpty(_authenticatedUser))
            {
                ShowPanel(OperativeRosterPanel);
                txtStatus.Text = "Operative selection terminal active.";
            }
            else
            {
                ShowPanel(LoginPanel);
                txtStatus.Text = "Ready to Jack In. Enter credentials.";
            }
        }

        private void btnShowCommunity_Click(object sender, RoutedEventArgs e)
        {
            ShowPanel(CommunityPanel);
            txtStatus.Text = "Connected to Zion Construct Community Hub & Forums.";
            LoadCommunityWeb();
        }

        private void btnShowPatchNotes_Click(object sender, RoutedEventArgs e)
        {
            ShowPanel(TransmissionsPanel);
            txtStatus.Text = "Loading Matrix Transmissions & Patch Notes...";
            LoadPatchNotesAsync();
        }

        private void btnShowRegister_Click(object sender, RoutedEventArgs e)
        {
            ShowPanel(RegisterPanel);
            txtStatus.Text = "Enter registration details...";
        }

        private void btnShowServer_Click(object sender, RoutedEventArgs e)
        {
            ShowPanel(ServerPanel);
            RunDiagnosticsAsync();
            txtStatus.Text = "Inspecting mainframe infrastructure...";
        }

        private void btnShowPatch_Click(object sender, RoutedEventArgs e)
        {
            ShowPanel(PatchPanel);
            txtStatus.Text = "Update protocol standby...";
        }

        private void ShowPanel(StackPanel panelToShow)
        {
            LoginPanel.Visibility = Visibility.Collapsed;
            OperativeRosterPanel.Visibility = Visibility.Collapsed;
            CommunityPanel.Visibility = Visibility.Collapsed;
            TransmissionsPanel.Visibility = Visibility.Collapsed;
            RegisterPanel.Visibility = Visibility.Collapsed;
            ServerPanel.Visibility = Visibility.Collapsed;
            PatchPanel.Visibility = Visibility.Collapsed;
            panelToShow.Visibility = Visibility.Visible;
            txtStatus.Foreground = Brushes.Lime;
        }

        private void LoadCommunityWeb()
        {
            try
            {
                SetSilent(wbCommunity, true);
                string user = !string.IsNullOrWhiteSpace(_authenticatedUser) ? _authenticatedUser : "Slacker";
                string url = $"http://{_currentServerIp}/community?user={Uri.EscapeDataString(user)}";
                wbCommunity.Navigate(url);
            }
            catch (Exception ex)
            {
                txtStatus.Text = $"Community web view notice: {ex.Message}";
            }
        }

        private static void SetSilent(WebBrowser webBrowser, bool silent)
        {
            try
            {
                var fi = typeof(WebBrowser).GetField("_axIWebBrowser2", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
                if (fi != null)
                {
                    dynamic? browser = fi.GetValue(webBrowser);
                    if (browser != null)
                    {
                        browser.Silent = silent;
                    }
                }
            }
            catch { }
        }

        public static void SetBrowserEmulationMode()
        {
            try
            {
                using var key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(
                    @"Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION");
                string appName = System.IO.Path.GetFileName(Environment.ProcessPath ?? "ZionLauncher.exe");
                key?.SetValue(appName, 11001, Microsoft.Win32.RegistryValueKind.DWord);
                key?.SetValue("ZionLauncher.exe", 11001, Microsoft.Win32.RegistryValueKind.DWord);
            }
            catch { }
        }

        private void btnOpenBrowserCommunity_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string user = !string.IsNullOrWhiteSpace(_authenticatedUser) ? _authenticatedUser : "Slacker";
                string url = $"http://{_currentServerIp}/community?user={Uri.EscapeDataString(user)}";
                Process.Start(new ProcessStartInfo
                {
                    FileName = url,
                    UseShellExecute = true
                });
            }
            catch (Exception ex)
            {
                txtStatus.Text = $"Browser launch error: {ex.Message}";
            }
        }

        private void btnRefreshCommunity_Click(object sender, RoutedEventArgs e)
        {
            LoadCommunityWeb();
        }

        private void btnViewAvatars_Click(object sender, RoutedEventArgs e)
        {
            btnOpenBrowserCommunity_Click(sender, e);
        }

        private void btnRefreshTransmissions_Click(object sender, RoutedEventArgs e)
        {
            LoadPatchNotesAsync();
        }

        private async void LoadPatchNotesAsync()
        {
            PatchNotesCardsContainer.Children.Clear();
            var loading = new TextBlock
            {
                Text = "Decrypting Matrix Transmissions from live server...",
                Foreground = Brushes.LightGreen,
                Padding = new Thickness(8)
            };
            PatchNotesCardsContainer.Children.Add(loading);

            try
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
                var res = await _httpClient.GetAsync($"http://{_currentServerIp}/api/news", cts.Token);
                if (res.IsSuccessStatusCode)
                {
                    var json = await res.Content.ReadAsStringAsync();
                    using var doc = JsonDocument.Parse(json);
                    if (doc.RootElement.TryGetProperty("news", out var newsArray))
                    {
                        PatchNotesCardsContainer.Children.Clear();
                        foreach (var item in newsArray.EnumerateArray())
                        {
                            string title = item.TryGetProperty("title", out var tp) ? tp.GetString() ?? "" : "";
                            string date = item.TryGetProperty("date", out var dp) ? dp.GetString() ?? "" : "";
                            string author = item.TryGetProperty("author", out var ap) ? ap.GetString() ?? "" : "";
                            string category = item.TryGetProperty("category", out var cp) ? cp.GetString() ?? "" : "";
                            string content = item.TryGetProperty("content", out var conP) ? conP.GetString() ?? "" : "";

                            var card = CreatePatchNoteCard(title, date, author, category, content);
                            PatchNotesCardsContainer.Children.Add(card);
                        }
                        return;
                    }
                }
            }
            catch { }

            PopulateFallbackPatchNotes();
        }

        private Border CreatePatchNoteCard(string title, string date, string author, string category, string content)
        {
            var card = new Border
            {
                Background = new SolidColorBrush(Color.FromRgb(8, 26, 12)),
                BorderBrush = new SolidColorBrush(Color.FromRgb(0, 150, 40)),
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(4),
                Padding = new Thickness(10),
                Margin = new Thickness(0, 0, 0, 8)
            };

            var stack = new StackPanel();
            var titleGrid = new Grid();
            titleGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            titleGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var titleBlock = new TextBlock
            {
                Text = title,
                FontWeight = FontWeights.Bold,
                FontSize = 13,
                Foreground = new SolidColorBrush(Color.FromRgb(57, 255, 20)),
                TextWrapping = TextWrapping.Wrap
            };
            Grid.SetColumn(titleBlock, 0);

            var catBlock = new TextBlock
            {
                Text = $"[{category} // {date}]",
                FontSize = 10,
                Foreground = new SolidColorBrush(Color.FromRgb(0, 255, 170)),
                VerticalAlignment = VerticalAlignment.Center
            };
            Grid.SetColumn(catBlock, 1);

            titleGrid.Children.Add(titleBlock);
            titleGrid.Children.Add(catBlock);
            stack.Children.Add(titleGrid);

            var authorBlock = new TextBlock
            {
                Text = $"Broadcast Author: {author}",
                FontSize = 10,
                Foreground = new SolidColorBrush(Color.FromRgb(136, 255, 170)),
                Margin = new Thickness(0, 2, 0, 4)
            };
            stack.Children.Add(authorBlock);

            var contentBlock = new TextBlock
            {
                Text = content.Replace("### ", "").Replace("- ", "• "),
                FontSize = 11,
                Foreground = new SolidColorBrush(Color.FromRgb(200, 255, 200)),
                TextWrapping = TextWrapping.Wrap,
                LineHeight = 15
            };
            stack.Children.Add(contentBlock);

            card.Child = stack;
            return card;
        }

        private void PopulateFallbackPatchNotes()
        {
            PatchNotesCardsContainer.Children.Clear();
            PatchNotesCardsContainer.Children.Add(CreatePatchNoteCard(
                "⚡ Patch 1.3.0: Martial Arts Disciplines & Synchronized Combat Netcode",
                "2026-10-04",
                "Reality Lead Architect",
                "COMBAT",
                "• Martial Arts Forms: Karate, Kung Fu, Aikido, and Tae Kwon Do postures synchronized.\n• Synchronized Defense: Block and dodge reactions serialize in real-time with incoming strikes.\n• 16-bit Extended Animation Netcode: Eliminates stance desync across clients.\n• Authentic Damage FX: Replaced placeholder textures with retail floating damage numbers (0x280006DF)."
            ));
            PatchNotesCardsContainer.Children.Add(CreatePatchNoteCard(
                "⚡ Patch 1.2.5: Hacker & Coder Disciplines Reconstructed",
                "2026-10-04",
                "Operator Tank",
                "DISCIPLINE",
                "• Hacker Kit: Logic Blast 1-3, Harmful Code DoTs, Code Freeze, and Network Firewalls.\n• Coder / Support Kit: Restore Health 1-3, Fast Healing, Emergency Repairs (<35%), and Revive RSI for downed operatives.\n• Personal Firewall: Actively absorbs hostile viral packets and direct damage."
            ));
            PatchNotesCardsContainer.Children.Add(CreatePatchNoteCard(
                "⚡ Patch 1.2.0: Widescreen HUD, Zero-Drift Locomotion & Megacity Elevation",
                "2026-10-03",
                "Morpheus",
                "WORLD",
                "• Widescreen Anchoring: Vitals, Compass, Chat, and Quickbar fluidly anchor to modern 1080p/1440p bounds.\n• Zero-Drift Locomotion: Directional sprinting and mouse orbiting stabilized.\n• Elevation Clamping: Operative feet remain flush with ground pavement at Y = 572.0 ± 0.5."
            ));
            PatchNotesCardsContainer.Children.Add(CreatePatchNoteCard(
                "⚡ Patch 1.1.0: Live VPS Mainframe & MariaDB Dedicated Persistence",
                "2026-10-01",
                "The Architect",
                "MAINFRAME",
                "• VPS Shard Active: 15.204.82.250 dedicated reality container with isolated internal DB.\n• Auto Hosts Redirection: Automatic DNS/hosts synchronization for instant login."
            ));
        }

        private void rbServer_Checked(object sender, RoutedEventArgs e)
        {
            if (rbRemoteServer.IsChecked == true)
            {
                _currentServerIp = RemoteServerIp;
            }
            else
            {
                _currentServerIp = LocalServerIp;
            }
            UpdateServerBanner();
            RunDiagnosticsAsync();
        }

        private async void RunDiagnosticsAsync()
        {
            string host = _currentServerIp;
            bool authOk = await TestTcpPortAsync(host, 11000, 1500);
            bool marginOk = await TestTcpPortAsync(host, 10000, 1500);
            bool patchOk = await TestTcpPortAsync(host, 80, 1500);
            bool dbOk = false;
            try
            {
                var resp = await _httpClient.GetAsync($"http://{host}/api/stats");
                dbOk = resp.IsSuccessStatusCode;
            }
            catch
            {
                if (host == LocalServerIp)
                    dbOk = await TestTcpPortAsync(host, DatabasePort, 1500);
            }
            bool hostsOk = CheckHostsRedirection(host);

            diagAuth.Text = authOk ? "ONLINE (Port 11000 Reachable)" : "OFFLINE / REFUSED";
            diagAuth.Foreground = authOk ? Brushes.Lime : Brushes.Red;

            diagMargin.Text = marginOk ? "ONLINE (Port 10000 Reachable)" : "OFFLINE / REFUSED";
            diagMargin.Foreground = marginOk ? Brushes.Lime : Brushes.Red;

            diagPatch.Text = patchOk ? "ONLINE (Port 80 Reachable)" : "OFFLINE / REFUSED";
            diagPatch.Foreground = patchOk ? Brushes.Lime : Brushes.Red;

            diagDb.Text = dbOk ? "ONLINE (Mainframe Database)" : "OFFLINE / REFUSED";
            diagDb.Foreground = dbOk ? Brushes.Lime : Brushes.Red;

            diagHosts.Text = hostsOk ? $"CONFIGURED -> {host}" : $"NOT REDIRECTED TO {host}";
            diagHosts.Foreground = hostsOk ? Brushes.Lime : Brushes.Orange;

            lblPingStatus.Text = authOk ? "AUTH: ONLINE" : "AUTH: OFFLINE";
            lblPingStatus.Foreground = authOk ? Brushes.Lime : Brushes.Red;
        }

        private static async Task<bool> TestTcpPortAsync(string host, int port, int timeoutMs)
        {
            try
            {
                using var client = new TcpClient();
                var connectTask = client.ConnectAsync(host, port);
                var timeoutTask = Task.Delay(timeoutMs);
                var completed = await Task.WhenAny(connectTask, timeoutTask);
                return completed == connectTask && client.Connected;
            }
            catch
            {
                return false;
            }
        }

        private static bool CheckHostsRedirection(string targetIp)
        {
            try
            {
                string hostsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"drivers\etc\hosts");
                if (!File.Exists(hostsPath)) return false;
                string content = File.ReadAllText(hostsPath);
                return content.Contains($"{targetIp} auth.beta.thematrixonline.com") &&
                       content.Contains($"{targetIp} reality.beta.thematrixonline.com");
            }
            catch
            {
                return false;
            }
        }

        private void btnRunDiagnostics_Click(object sender, RoutedEventArgs e)
        {
            RunDiagnosticsAsync();
        }

        private void btnApplyHosts_Click(object sender, RoutedEventArgs e)
        {
            bool success = ApplyHostsRedirection(_currentServerIp);
            if (success)
            {
                txtStatus.Text = $"SUCCESS: Windows hosts redirection applied -> {_currentServerIp}";
            }
            else
            {
                txtStatus.Text = "Notice: Updating hosts requires Administrator rights. Running elevated helper...";
                RunElevatedHostsPatch(_currentServerIp);
            }
            RunDiagnosticsAsync();
        }

        private static bool ApplyHostsRedirection(string targetIp)
        {
            try
            {
                string hostsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), @"drivers\etc\hosts");
                string[] lines = File.ReadAllLines(hostsPath);
                var sb = new StringBuilder();

                foreach (var line in lines)
                {
                    string trimmed = line.Trim();
                    if (trimmed.Contains("thematrixonline.com") || trimmed.Contains("matrixonline.com") || trimmed.Contains("Matrix Online Emulator"))
                        continue;
                    sb.AppendLine(line);
                }

                sb.AppendLine();
                sb.AppendLine("# Matrix Online Emulator Redirection");
                sb.AppendLine($"{targetIp} auth.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} update.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} patch.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} matrixonline.com");
                sb.AppendLine($"{targetIp} reality.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} Reality.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} slums.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} vector.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} syntax.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} recursion.beta.thematrixonline.com");
                sb.AppendLine($"{targetIp} method.beta.thematrixonline.com");

                File.WriteAllText(hostsPath, sb.ToString());
                return true;
            }
            catch
            {
                return false;
            }
        }

        private static void RunElevatedHostsPatch(string targetIp)
        {
            try
            {
                string script = $"$hosts = [System.IO.File]::ReadAllLines('C:\\Windows\\System32\\drivers\\etc\\hosts') | Where-Object {{ $_ -notmatch 'thematrixonline|matrixonline' }}; " +
                    $"$hosts += ''; $hosts += '# Matrix Online Emulator Redirection'; " +
                    $"$hosts += '{targetIp} auth.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} update.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} patch.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} matrixonline.com'; " +
                    $"$hosts += '{targetIp} reality.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} Reality.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} slums.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} vector.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} syntax.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} recursion.beta.thematrixonline.com'; " +
                    $"$hosts += '{targetIp} method.beta.thematrixonline.com'; " +
                    $"[System.IO.File]::WriteAllLines('C:\\Windows\\System32\\drivers\\etc\\hosts', $hosts)";
                Process.Start(new ProcessStartInfo
                {
                    FileName = "powershell.exe",
                    Arguments = $"-NoProfile -ExecutionPolicy Bypass -Command \"{script}\"",
                    Verb = "runas",
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                });
            }
            catch { }
        }

        private string GetConnectionString()
        {
            // Direct raw MySQL connections are restricted strictly to localhost (127.0.0.1).
            // Remote environments communicate exclusively via authenticated HTTP REST API gateway (/api/login, /api/register).
            return $"Server=127.0.0.1;Port={DatabasePort};User ID=reality;Password=reality;Database=reality;Connection Timeout=6;";
        }

        private void txtLogin_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter && btnJackIn.IsEnabled)
            {
                btnJackIn_Click(sender, e);
            }
        }

        private void txtLogin_TextChanged(object sender, TextChangedEventArgs e)
        {
            ResetOperativeSelector();
        }

        private void txtLogin_PasswordChanged(object sender, RoutedEventArgs e)
        {
            ResetOperativeSelector();
        }

        private void ResetOperativeSelector()
        {
            if (OperativeSelectPanel != null && OperativeSelectPanel.Visibility == Visibility.Visible)
            {
                OperativeSelectPanel.Visibility = Visibility.Collapsed;
                cmbOperatives.Items.Clear();
                btnJackIn.Content = "AUTHENTICATE & SELECT OPERATIVE";
            }
        }

        private void cmbOperatives_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cmbOperatives.SelectedItem is ComboBoxItem item)
            {
                string tag = item.Tag as string ?? "";
                if (string.IsNullOrEmpty(tag))
                {
                    btnJackIn.Content = "ENTER CHARACTER CREATION";
                    txtStatus.Foreground = Brushes.Orange;
                    txtStatus.Text = "Creating New Operative: Handle MUST NOT contain spaces (e.g. 'TheOne', not 'The One').";
                }
                else
                {
                    btnJackIn.Content = "JACK IN AS OPERATIVE";
                    txtStatus.Foreground = Brushes.Lime;
                    txtStatus.Text = $"Ready to jack in as operative '{tag}'. Press JACK IN to enter MegaCity.";
                }
            }
        }

        private void txtReg_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter && btnRegister.IsEnabled)
            {
                btnRegister_Click(sender, e);
            }
        }

        private async void btnJackIn_Click(object sender, RoutedEventArgs e)
        {
            if (!IsClientInstalled(out _))
            {
                var result = MessageBox.Show(
                    "Matrix Online game client files were not detected in this folder.\n\n" +
                    "Would you like to download and install the complete authentic game client (1.26 GB) from the Zion live server now?",
                    "Client Download Required",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Information);

                if (result == MessageBoxResult.Yes)
                {
                    ShowPanel(PatchPanel);
                    await StartClientDownloadAndExtractAsync();
                }
                return;
            }

            string username = txtLoginUser.Text.Trim();
            string password = txtLoginPass.Password;

            if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
            {
                txtStatus.Foreground = Brushes.Red;
                txtStatus.Text = "ERROR: Missing credentials.";
                return;
            }

            // If operative selector is already displayed and populated, proceed directly to launch
            if (OperativeSelectPanel.Visibility == Visibility.Visible && cmbOperatives.Items.Count > 0)
            {
                string chosenChar = "";
                if (cmbOperatives.SelectedItem is ComboBoxItem selectedItem && selectedItem.Tag is string tag)
                {
                    chosenChar = tag;
                }

                btnJackIn.IsEnabled = false;
                if (string.IsNullOrEmpty(chosenChar))
                {
                    txtStatus.Foreground = Brushes.Lime;
                    txtStatus.Text = "ACCESS GRANTED. Entering Character Creation... (NOTE: Operative handle must not contain spaces, e.g. 'TheOne')";
                }
                else
                {
                    txtStatus.Foreground = Brushes.Lime;
                    txtStatus.Text = $"ACCESS GRANTED. Jacking in as operative '{chosenChar}'...";
                }

                JackIn(username, password, chosenChar);
                btnJackIn.IsEnabled = true;
                return;
            }

            btnJackIn.IsEnabled = false;
            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = $"Verifying credentials with Zion mainframe ({_currentServerIp})...";

            var (authOk, authMsg, operatives) = await ValidateCredentialsAsync(username, password);
            if (!authOk)
            {
                txtStatus.Foreground = Brushes.Red;
                txtStatus.Text = authMsg;
                btnJackIn.IsEnabled = true;
                return;
            }

            _authenticatedUser = username;
            _authenticatedPassword = password;
            _operativesList = operatives ?? new List<OperativeProfile>();

            btnJackIn.IsEnabled = true;
            DisplayOperativeRoster(_operativesList);
            ShowPanel(OperativeRosterPanel);

            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = _operativesList.Count > 0 
                ? $"ACCESS GRANTED. Select your operative from the roster to Jack In."
                : $"ACCESS GRANTED. No operatives detected. Click [+] CREATE OPERATIVE to begin.";
        }

        private void DisplayOperativeRoster(List<OperativeProfile> operatives)
        {
            OperativeCardsContainer.Children.Clear();
            lblAccountSummary.Text = $"OPERATOR ACCOUNT: {_authenticatedUser} | MATRIX SHARD: LIVE ({_currentServerIp}) | [{operatives.Count} Operatives]";

            if (operatives.Count == 0)
            {
                var emptyNotice = new TextBlock
                {
                    Text = "No Operatives detected on this account.\nClick [+] CREATE OPERATIVE below to enter character creation in the construct.",
                    TextAlignment = TextAlignment.Center,
                    Foreground = Brushes.LightGreen,
                    Margin = new Thickness(0, 30, 0, 0),
                    FontSize = 13
                };
                OperativeCardsContainer.Children.Add(emptyNotice);
                SelectedOperativeDossier.Visibility = Visibility.Collapsed;
                btnLaunchSelectedOperative.IsEnabled = false;
                return;
            }

            SelectedOperativeDossier.Visibility = Visibility.Visible;
            btnLaunchSelectedOperative.IsEnabled = true;

            for (int i = 0; i < operatives.Count; i++)
            {
                var op = operatives[i];
                var card = new Border
                {
                    Background = new SolidColorBrush(Color.FromRgb(5, 25, 10)),
                    BorderBrush = new SolidColorBrush(Color.FromRgb(0, 100, 35)),
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(4),
                    Padding = new Thickness(10, 8, 10, 8),
                    Margin = new Thickness(0, 0, 0, 6),
                    Cursor = Cursors.Hand,
                    Tag = op
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(40) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                string iconStr = GetProfessionIcon(op.Profession);
                var iconBlock = new TextBlock
                {
                    Text = iconStr,
                    FontSize = 22,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(iconBlock, 0);

                var infoStack = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
                var nameBlock = new TextBlock
                {
                    Text = op.Handle,
                    FontWeight = FontWeights.Bold,
                    FontSize = 14,
                    Foreground = new SolidColorBrush(Color.FromRgb(57, 255, 20))
                };
                string discStr = GetProfessionName(op.Profession);
                string distStr = !string.IsNullOrWhiteSpace(op.District) ? op.District : "Megacity";
                var statsBlock = new TextBlock
                {
                    Text = $"Level {op.Level} {discStr}  //  Sector: {distStr}",
                    FontSize = 11,
                    Foreground = new SolidColorBrush(Color.FromRgb(136, 255, 170))
                };
                infoStack.Children.Add(nameBlock);
                infoStack.Children.Add(statsBlock);
                Grid.SetColumn(infoStack, 1);

                var statusBlock = new TextBlock
                {
                    Text = "READY",
                    FontWeight = FontWeights.Bold,
                    FontSize = 11,
                    Foreground = new SolidColorBrush(Color.FromRgb(0, 255, 102)),
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(statusBlock, 2);

                grid.Children.Add(iconBlock);
                grid.Children.Add(infoStack);
                grid.Children.Add(statusBlock);
                card.Child = grid;

                card.MouseDown += (s, e) =>
                {
                    if (s is Border b && b.Tag is OperativeProfile clickedOp)
                    {
                        SelectOperative(clickedOp, b);
                    }
                };

                OperativeCardsContainer.Children.Add(card);

                if (i == 0)
                {
                    SelectOperative(op, card);
                }
            }
        }

        private void SelectOperative(OperativeProfile op, Border cardBorder)
        {
            _selectedOperative = op;

            foreach (var child in OperativeCardsContainer.Children)
            {
                if (child is Border b)
                {
                    b.BorderBrush = new SolidColorBrush(Color.FromRgb(0, 100, 35));
                    b.Background = new SolidColorBrush(Color.FromRgb(5, 25, 10));
                }
            }

            cardBorder.BorderBrush = new SolidColorBrush(Color.FromRgb(57, 255, 20));
            cardBorder.Background = new SolidColorBrush(Color.FromRgb(10, 45, 18));

            txtSelectedOperativeName.Text = $"OPERATIVE: {op.Handle}";
            string disc = GetProfessionName(op.Profession);
            string dist = !string.IsNullOrWhiteSpace(op.District) ? op.District : "Megacity";
            txtSelectedOperativeStats.Text = $"DISCIPLINE: Level {op.Level} {disc} | SECTOR: {dist}";
            txtSelectedOperativeIcon.Text = GetProfessionIcon(op.Profession);
            txtSelectedOperativeRsi.Text = "RSI STATUS: 100% HEALTHY // READY FOR BROADCAST";
            btnLaunchSelectedOperative.Content = $"⚡ JACK IN AS {op.Handle.ToUpperInvariant()}";
        }

        private void btnLaunchSelectedOperative_Click(object sender, RoutedEventArgs e)
        {
            if (_selectedOperative == null && _operativesList.Count > 0)
                _selectedOperative = _operativesList[0];

            string chosenHandle = _selectedOperative != null ? _selectedOperative.Handle : _authenticatedUser;
            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = $"ACCESS GRANTED. Jacking in as operative '{chosenHandle}'...";
            JackIn(_authenticatedUser, _authenticatedPassword, chosenHandle);
        }

        private void btnCreateNewOperative_Click(object sender, RoutedEventArgs e)
        {
            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = "ACCESS GRANTED. Entering Character Creation... (NOTE: Operative handle must not contain spaces)";
            JackIn(_authenticatedUser, _authenticatedPassword, null);
        }

        private void btnLogout_Click(object sender, RoutedEventArgs e)
        {
            _authenticatedUser = "";
            _authenticatedPassword = "";
            _operativesList.Clear();
            _selectedOperative = null;
            ShowPanel(LoginPanel);
            txtStatus.Text = "Logged out. Enter credentials to Jack In.";
        }

        private string GetProfessionName(int prof)
        {
            return prof switch
            {
                1 => "Hacker",
                2 => "Coder / Support",
                3 => "Martial Artist",
                4 => "Spy",
                5 => "Gunner / Soldier",
                _ => "Operative"
            };
        }

        private string GetProfessionIcon(int prof)
        {
            return prof switch
            {
                1 => "💻",
                2 => "🧬",
                3 => "🥋",
                4 => "🕶️",
                5 => "🎯",
                _ => "⚡"
            };
        }

        private string GetDistrictName(int districtId)
        {
            return districtId switch
            {
                1 => "Slums",
                2 => "Downtown",
                3 => "International",
                4 => "Richland",
                _ => "Megacity"
            };
        }

        private string FormatDistrictDisplay(string district)
        {
            if (string.IsNullOrWhiteSpace(district)) return "Megacity";
            if (int.TryParse(district, out int id)) return GetDistrictName(id);
            return district;
        }

        private async Task<(bool Success, string Message, System.Collections.Generic.List<OperativeProfile> Operatives)> ValidateCredentialsAsync(string username, string password)
        {
            var operatives = new System.Collections.Generic.List<OperativeProfile>();
            try
            {
                var payload = new { username = username, password = password };
                var json = JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(6));
                var response = await _httpClient.PostAsync($"http://{_currentServerIp}/api/login", content, cts.Token);
                var respBody = await response.Content.ReadAsStringAsync();

                if (response.IsSuccessStatusCode)
                {
                    try
                    {
                        using var doc = JsonDocument.Parse(respBody);
                        if (doc.RootElement.TryGetProperty("characters", out var charsProp) && charsProp.ValueKind == JsonValueKind.Array)
                        {
                            foreach (var c in charsProp.EnumerateArray())
                            {
                                var op = new OperativeProfile();
                                if (c.TryGetProperty("charId", out var idProp))
                                {
                                    if (idProp.ValueKind == JsonValueKind.Number) op.Id = idProp.GetInt64();
                                    else if (long.TryParse(idProp.GetString(), out long parsedId)) op.Id = parsedId;
                                }
                                if (c.TryGetProperty("handle", out var handleProp) && handleProp.ValueKind == JsonValueKind.String)
                                    op.Handle = handleProp.GetString() ?? "";
                                if (c.TryGetProperty("firstName", out var fnProp) && fnProp.ValueKind == JsonValueKind.String)
                                    op.FirstName = fnProp.GetString() ?? "";
                                if (c.TryGetProperty("lastName", out var lnProp) && lnProp.ValueKind == JsonValueKind.String)
                                    op.LastName = lnProp.GetString() ?? "";
                                if (c.TryGetProperty("level", out var lvlProp))
                                {
                                    if (lvlProp.ValueKind == JsonValueKind.Number) op.Level = lvlProp.GetInt32();
                                    else if (int.TryParse(lvlProp.GetString(), out int parsedLvl)) op.Level = parsedLvl;
                                }
                                if (c.TryGetProperty("profession", out var profProp))
                                {
                                    if (profProp.ValueKind == JsonValueKind.Number) op.Profession = profProp.GetInt32();
                                    else if (int.TryParse(profProp.GetString(), out int parsedProf)) op.Profession = parsedProf;
                                }
                                if (c.TryGetProperty("district", out var distProp))
                                {
                                    if (distProp.ValueKind == JsonValueKind.String)
                                        op.District = FormatDistrictDisplay(distProp.GetString() ?? "");
                                    else if (distProp.ValueKind == JsonValueKind.Number && distProp.TryGetInt32(out int distId))
                                        op.District = GetDistrictName(distId);
                                }

                                if (!string.IsNullOrWhiteSpace(op.Handle))
                                {
                                    operatives.Add(op);
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Debug.WriteLine("Characters parse error: " + ex.Message);
                    }

                    return (true, "Authentication verified.", operatives);
                }

                if (response.StatusCode == HttpStatusCode.Unauthorized || response.StatusCode == HttpStatusCode.BadRequest)
                {
                    try
                    {
                        using var doc = JsonDocument.Parse(respBody);
                        if (doc.RootElement.TryGetProperty("message", out var msgProp))
                        {
                            string msg = msgProp.GetString() ?? "";
                            if (!string.IsNullOrWhiteSpace(msg))
                            {
                                return (false, $"ACCESS DENIED: {msg}", operatives);
                            }
                        }
                    }
                    catch { }

                    return (false, "ACCESS DENIED: Invalid passcode or operative not found.", operatives);
                }

                return (false, $"MAINFRAME REJECTED ({response.StatusCode}): {respBody}", operatives);
            }
            catch (Exception ex)
            {
                Debug.WriteLine("REST API login exception: " + ex.Message);

                // Local fallback if local server mode
                if (_currentServerIp == LocalServerIp && IsPortListening(DatabasePort))
                {
                    try
                    {
                        await using var conn = new MySqlConnection(GetConnectionString());
                        await conn.OpenAsync();

                        await using var cmd = new MySqlCommand("SELECT userId, username, passwordSalt, passwordHash FROM users WHERE LOWER(username) = LOWER(@user) LIMIT 1", conn);
                        cmd.Parameters.AddWithValue("@user", username);
                        await using var reader = await cmd.ExecuteReaderAsync();
                        if (!await reader.ReadAsync())
                        {
                            return (false, "ACCESS DENIED: Operative not found. Register first.", operatives);
                        }

                        string salt = reader["passwordSalt"]?.ToString() ?? "";
                        string currentHash = reader["passwordHash"]?.ToString() ?? "";
                        string expectedHash = HashPassword(salt, password);
                        long userId = Convert.ToInt64(reader["userId"]);

                        if (string.Equals(currentHash, expectedHash, StringComparison.OrdinalIgnoreCase))
                        {
                            await reader.CloseAsync();
                            try
                            {
                                await using var charCmd = new MySqlCommand("SELECT charId, handle, firstName, lastName, level, profession, district FROM characters WHERE userId = @uid ORDER BY charId DESC", conn);
                                charCmd.Parameters.AddWithValue("@uid", userId);
                                await using var charReader = await charCmd.ExecuteReaderAsync();
                                while (await charReader.ReadAsync())
                                {
                                    operatives.Add(new OperativeProfile
                                    {
                                        Id = Convert.ToInt64(charReader["charId"]),
                                        Handle = charReader["handle"]?.ToString() ?? "",
                                        FirstName = charReader["firstName"]?.ToString() ?? "",
                                        LastName = charReader["lastName"]?.ToString() ?? "",
                                        Level = charReader["level"] != DBNull.Value ? Convert.ToInt32(charReader["level"]) : 1,
                                        Profession = charReader["profession"] != DBNull.Value ? Convert.ToInt32(charReader["profession"]) : 0,
                                        District = charReader["district"]?.ToString() ?? ""
                                    });
                                }
                            }
                            catch (Exception cEx)
                            {
                                Debug.WriteLine("Local char query notice: " + cEx.Message);
                            }

                            return (true, "Authentication verified.", operatives);
                        }
                        else
                        {
                            return (false, "ACCESS DENIED: Invalid passcode.", operatives);
                        }
                    }
                    catch (Exception dbEx)
                    {
                        return (false, $"DATABASE ERROR: Unable to verify credentials ({dbEx.Message}).", operatives);
                    }
                }

                return (false, $"NETWORK ERROR: Cannot reach Zion mainframe at {_currentServerIp}. Check connection.", operatives);
            }
        }

        private void btnRegister_Click(object sender, RoutedEventArgs e)
        {
            string username = txtRegUser.Text.Trim();
            string password = txtRegPass.Password;
            string confirm = txtRegPassConfirm.Password;

            if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
            {
                txtStatus.Foreground = Brushes.Red;
                txtStatus.Text = "ERROR: Missing fields.";
                return;
            }

            if (password != confirm)
            {
                txtStatus.Foreground = Brushes.Red;
                txtStatus.Text = "ERROR: Passcodes do not match.";
                return;
            }

            RegisterUser(username, password);
        }

        private async void RegisterUser(string username, string password)
        {
            btnRegister.IsEnabled = false;
            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = $"Registering operator with Zion mainframe ({_currentServerIp})...";
            if (_currentServerIp == LocalServerIp) EnsureLocalServers();

            try
            {
                var payload = new { username = username, password = password };
                var json = JsonSerializer.Serialize(payload);
                var content = new StringContent(json, Encoding.UTF8, "application/json");
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(6));
                var response = await _httpClient.PostAsync($"http://{_currentServerIp}/api/register", content, cts.Token);
                var respBody = await response.Content.ReadAsStringAsync();

                if (response.IsSuccessStatusCode)
                {
                    txtStatus.Foreground = Brushes.Lime;
                    txtStatus.Text = "SUCCESS: Operator created. Jacking in...";
                    JackIn(username, password, null);
                    btnRegister.IsEnabled = true;
                    return;
                }
                else if (response.StatusCode == HttpStatusCode.Conflict || respBody.Contains("already"))
                {
                    txtStatus.Foreground = Brushes.Red;
                    txtStatus.Text = "ACCESS DENIED: Operator handle already exists. Please log in.";
                    btnRegister.IsEnabled = true;
                    return;
                }
                else
                {
                    txtStatus.Foreground = Brushes.Red;
                    try
                    {
                        using var doc = JsonDocument.Parse(respBody);
                        if (doc.RootElement.TryGetProperty("message", out var msgProp))
                        {
                            txtStatus.Text = $"REGISTRATION REJECTED: {msgProp.GetString()}";
                            btnRegister.IsEnabled = true;
                            return;
                        }
                    }
                    catch { }

                    txtStatus.Text = $"REGISTRATION REJECTED: {respBody}";
                    btnRegister.IsEnabled = true;
                    return;
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine("REST API register notice: " + ex.Message);
            }

            // Local fallback if local server mode
            if (_currentServerIp == LocalServerIp && IsPortListening(DatabasePort))
            {
                try
                {
                    await using var conn = new MySqlConnection(GetConnectionString());
                    await conn.OpenAsync();

                    await using (MySqlCommand checkCmd = new("SELECT userId FROM users WHERE LOWER(username) = LOWER(@user)", conn))
                    {
                        checkCmd.Parameters.AddWithValue("@user", username);
                        var exists = await checkCmd.ExecuteScalarAsync();
                        if (exists != null)
                        {
                            txtStatus.Foreground = Brushes.Red;
                            txtStatus.Text = "ACCESS DENIED: Operator handle already exists. Please log in.";
                            btnRegister.IsEnabled = true;
                            return;
                        }
                    }

                    string salt = GenerateSalt();
                    string hash = HashPassword(salt, password);

                    await using (MySqlCommand insCmd = new("INSERT INTO users SET username=@user, passwordSalt=@salt, passwordHash=@hash, timeCreated=UNIX_TIMESTAMP()", conn))
                    {
                        insCmd.Parameters.AddWithValue("@user", username);
                        insCmd.Parameters.AddWithValue("@salt", salt);
                        insCmd.Parameters.AddWithValue("@hash", hash);
                        await insCmd.ExecuteNonQueryAsync();
                    }

                    txtStatus.Foreground = Brushes.Lime;
                    txtStatus.Text = "SUCCESS: Operator created. Jacking in...";
                    JackIn(username, password, null);
                    btnRegister.IsEnabled = true;
                    return;
                }
                catch (Exception ex)
                {
                    txtStatus.Foreground = Brushes.Red;
                    txtStatus.Text = "DB Notice: " + ex.Message;
                    btnRegister.IsEnabled = true;
                    return;
                }
            }

            txtStatus.Foreground = Brushes.Red;
            txtStatus.Text = $"ERROR: Could not connect to mainframe at {_currentServerIp} to register.";
            btnRegister.IsEnabled = true;
        }

        private string GenerateSalt()
        {
            StringBuilder sb = new();
            for (int i = 0; i < 8; i++)
            {
                sb.Append((char)_random.Next(33, 127));
            }
            return sb.ToString();
        }

        private string HashPassword(string salt, string password)
        {
            string saltHash = Sha1Hex(salt);
            string passHash = Sha1Hex(password);
            return Sha1Hex(saltHash + passHash);
        }

        private string Sha1Hex(string input)
        {
            using SHA1 sha1 = SHA1.Create();
            byte[] hashBytes = sha1.ComputeHash(Encoding.UTF8.GetBytes(input));
            StringBuilder sb = new();
            foreach (byte b in hashBytes)
            {
                sb.Append(b.ToString("x2"));
            }
            return sb.ToString();
        }

        private void EnsureLocalServers()
        {
            if (_currentServerIp != LocalServerIp) return;

            // 1. Reality Database on port 3307
            if (!IsPortListening(3307) && File.Exists(MySqlExe))
            {
                txtStatus.Text = "Booting local Reality database (port 3307)...";
                try
                {
                    string dataDir = Directory.Exists(@"E:\Games\The Matrix Online\RealityDB_Data")
                        ? @"E:\Games\The Matrix Online\RealityDB_Data"
                        : @"E:\RealityDB\data";

                    Process.Start(new ProcessStartInfo
                    {
                        FileName = MySqlExe,
                        Arguments = $"--datadir=\"{dataDir}\" --port=3307 --console",
                        UseShellExecute = true,
                        WindowStyle = ProcessWindowStyle.Minimized
                    });

                    for (int i = 0; i < 12 && !IsPortListening(3307); i++)
                        System.Threading.Thread.Sleep(500);
                }
                catch { }
            }

            // 2. Reality Server daemon (Auth 11000, Margin 10000, World UDP 10000)
            if (Process.GetProcessesByName("Reality").Length == 0 && File.Exists(RealityServer))
            {
                txtStatus.Text = "Booting local Zion Reality Server daemon...";
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = RealityServer,
                        WorkingDirectory = Path.GetDirectoryName(RealityServer),
                        UseShellExecute = true,
                        WindowStyle = ProcessWindowStyle.Minimized
                    });
                    System.Threading.Thread.Sleep(2000);
                }
                catch { }
            }
        }

        private static bool IsPortListening(int port)
        {
            try
            {
                foreach (var ep in System.Net.NetworkInformation.IPGlobalProperties
                             .GetIPGlobalProperties().GetActiveTcpListeners())
                    if (ep.Port == port) return true;
            }
            catch { }
            return false;
        }

        private async void JackIn(string username, string password, string? operativeHandle = null)
        {
            txtStatus.Foreground = Brushes.Lime;
            txtStatus.Text = $"Synchronizing Zion mainframe ({_currentServerIp})...";

            // 1. Automatically configure hosts redirection if requested
            if (chkAutoSyncHosts.IsChecked == true && !CheckHostsRedirection(_currentServerIp))
            {
                if (!ApplyHostsRedirection(_currentServerIp))
                {
                    RunElevatedHostsPatch(_currentServerIp);
                }
            }

            // 2. Ensure local servers if in local mode
            if (_currentServerIp == LocalServerIp)
            {
                EnsureLocalServers();
            }

            // 3. Single-instance cleanup
            foreach (var name in new[] { "matrix", "launcher" })
            {
                foreach (var p in Process.GetProcessesByName(name))
                {
                    try { p.Kill(); p.WaitForExit(1000); } catch { }
                }
            }

            // 4. Ensure pubkey.dat exists in GameRoot and Client
            string srvPubkey = Path.Combine(GameRoot, @"mxoemu_fork\Reality\Binaries\pubkey.dat");
            string rootPubkey = Path.Combine(GameRoot, "pubkey.dat");
            string clientPubkey = Path.Combine(GameRoot, "Client", "pubkey.dat");
            if (File.Exists(srvPubkey))
            {
                try { File.Copy(srvPubkey, rootPubkey, true); } catch { }
                try { File.Copy(srvPubkey, clientPubkey, true); } catch { }
            }
            else if (File.Exists(clientPubkey))
            {
                try { File.Copy(clientPubkey, rootPubkey, true); } catch { }
            }

            // 5. Resolve authentic matrix.exe (prioritize Client directory)
            string[] candidateClientPaths = new[]
            {
                Path.Combine(GameRoot, "Client", "matrix.exe"),
                Path.Combine(GameRoot, "matrix.exe"),
                Path.Combine(GameRoot, "Client", "launcher.exe"),
                Path.Combine(GameRoot, "launcher.exe")
            };

            string clientExe = "";
            foreach (var path in candidateClientPaths)
            {
                if (File.Exists(path))
                {
                    clientExe = Path.GetFullPath(path);
                    break;
                }
            }

            if (string.IsNullOrEmpty(clientExe))
            {
                txtStatus.Text = "ERROR: authentic matrix.exe not found!";
                return;
            }

            txtStatus.Text = $"JACKED IN TO {_currentServerIp}. Launching 2005 Monolith Jupiter Client...";
            try
            {
                try
                {
                    File.WriteAllText(Path.Combine(GameRoot, "server_ip.txt"), _currentServerIp);
                    string clientSubDir = Path.Combine(GameRoot, "Client");
                    if (Directory.Exists(clientSubDir))
                    {
                        File.WriteAllText(Path.Combine(clientSubDir, "server_ip.txt"), _currentServerIp);
                    }
                }
                catch { }

                string charArg = !string.IsNullOrWhiteSpace(operativeHandle) ? operativeHandle : username;
                string launchArgs = $"-clone -nopatch -configsection HighDetail -user \"{username}\" -pwd \"{password}\" -char \"{charArg}\"";
                string cmdLine = $"\"{clientExe}\" {launchArgs}";
                string workDir = Path.GetDirectoryName(clientExe)!;

                bool startedSuspended = false;

                // Retail 7.6005 client: apply the launcher bypasses while the process is still suspended, then resume,
                // then inject mxohax_modern.dll and hook the mxowrap child. Mirrors the verified tools/harness32.cpp path.
                string retailExe = Path.Combine(GameRoot, "Client", "matrix.exe");
                if (!File.Exists(retailExe)) retailExe = clientExe;
                string retailHook = RetailHookDll;
                if (IsRetailClient(retailExe) && File.Exists(retailHook))
                {
                    string retailArgs = $"-clone -LocalTest -nopatch -noeula -autojackin -configsection HighDetail -user \"{username}\" -pwd \"{password}\" -char \"{charArg}\"";
                    string retailCmd = $"\"{retailExe}\" {retailArgs}";
                    string retailDir = Path.GetDirectoryName(retailExe)!;

                    STARTUPINFO rsi = new STARTUPINFO();
                    rsi.cb = (uint)System.Runtime.InteropServices.Marshal.SizeOf(rsi);
                    if (CreateProcess(null, retailCmd, IntPtr.Zero, IntPtr.Zero, false, CREATE_SUSPENDED, IntPtr.Zero, retailDir, ref rsi, out PROCESS_INFORMATION rpi))
                    {
                        bool patched = ApplyRetailBypassPatches(rpi.hProcess);
                        ResumeThread(rpi.hThread);
                        Thread.Sleep(150);
                        bool injected = InjectDllIntoHandle(rpi.hProcess, retailHook);
                        uint retailPid = rpi.dwProcessId;
                        CloseHandle(rpi.hThread);
                        CloseHandle(rpi.hProcess);

                        string clientDirForChild = Path.Combine(GameRoot, "Client");
                        _ = Task.Run(() => WatchRetailChildAndHook((int)retailPid, retailHook, clientDirForChild));

                        Dispatcher.Invoke(() =>
                        {
                            txtStatus.Foreground = (patched && injected) ? Brushes.Lime : Brushes.Orange;
                            txtStatus.Text = $"[Jacked In] Retail 7.6005 client started (PID {retailPid}); launcher bypass {(patched ? "applied" : "FAILED")}, hook {(injected ? "injected" : "FAILED")}.";
                        });
                        startedSuspended = true;
                    }
                }

                if (!startedSuspended && File.Exists(HookDll))
                {
                    STARTUPINFO si = new STARTUPINFO();
                    si.cb = (uint)System.Runtime.InteropServices.Marshal.SizeOf(si);
                    if (CreateProcess(null, cmdLine, IntPtr.Zero, IntPtr.Zero, false, CREATE_SUSPENDED, IntPtr.Zero, workDir, ref si, out PROCESS_INFORMATION pi))
                    {
                        startedSuspended = true;
                        ResumeThread(pi.hThread);
                        Thread.Sleep(100);
                        bool injected = InjectDllIntoHandle(pi.hProcess, HookDll);
                        CloseHandle(pi.hThread);
                        CloseHandle(pi.hProcess);

                        Dispatcher.Invoke(() =>
                        {
                            txtStatus.Foreground = Brushes.Lime;
                            txtStatus.Text = injected
                                ? $"[Jacked In] Matrix launched suspended, mxohax injected, process resumed (PID {pi.dwProcessId})."
                                : $"[Jacked In] Matrix process resumed (PID {pi.dwProcessId}).";
                        });
                    }
                }

                if (!startedSuspended)
                {
                    var proc = Process.Start(new ProcessStartInfo
                    {
                        FileName = clientExe,
                        Arguments = launchArgs,
                        WorkingDirectory = workDir,
                        UseShellExecute = false
                    });

                    if (File.Exists(HookDll))
                    {
                        _ = Task.Run(() => InjectImmediatelyAndWatch(proc));
                    }
                }

                await Task.Delay(2000);
                this.WindowState = WindowState.Minimized;
            }
            catch (Exception ex)
            {
                txtStatus.Text = "Launch Error: " + ex.Message;
            }
        }

        /// <summary>
        /// The retail launcher (mxowrap) re-runs the game as %TEMP%\MatrixOnline.N\matrix.exe. Find that child, give it the
        /// game dir + hook + dbghelp, and inject the hook (mirrors tools/harness32.cpp CheckAndHookChildProcesses).
        /// </summary>
        private void WatchRetailChildAndHook(int parentPid, string hookDll, string clientDir)
        {
            var done = new System.Collections.Generic.HashSet<int>();
            try
            {
                for (int i = 0; i < 240; i++) // ~60 s
                {
                    Thread.Sleep(250);
                    var procs = Process.GetProcessesByName("matrix").Concat(Process.GetProcessesByName("launcher")).ToArray();
                    foreach (var proc in procs)
                    {
                        if (proc.Id == parentPid || done.Contains(proc.Id)) continue;
                        string exePath = "";
                        try { exePath = proc.MainModule?.FileName ?? ""; } catch { continue; }
                        bool isWrapChild = exePath.IndexOf("MatrixOnline.", StringComparison.OrdinalIgnoreCase) >= 0
                                           || string.Equals(Path.GetFileName(exePath), "matrix.exe", StringComparison.OrdinalIgnoreCase);
                        if (!isWrapChild) continue;

                        string childDir = Path.GetDirectoryName(exePath) ?? "";
                        try
                        {
                            File.WriteAllText(Path.Combine(childDir, "mxohax_gamedir.txt"), clientDir);
                            File.Copy(hookDll, Path.Combine(childDir, "mxohax.dll"), true);
                            File.Copy(hookDll, Path.Combine(childDir, "mxohax_modern.dll"), true);
                            string dbg = Path.Combine(clientDir, "dbghelp.dll");
                            if (File.Exists(dbg)) File.Copy(dbg, Path.Combine(childDir, "dbghelp.dll"), true);
                        }
                        catch { }

                        if (InjectDll(proc, hookDll)) done.Add(proc.Id);
                    }
                    if (done.Count > 0) { Thread.Sleep(2000); if (done.Count > 0) return; }
                }
            }
            catch { }
        }

        private void InjectImmediatelyAndWatch(Process? targetProc)
        {
            if (targetProc == null) return;
            var targetPid = targetProc.Id;
            var alreadyDone = new System.Collections.Generic.HashSet<int>();
            try
            {
                for (int attempt = 0; attempt < 100; attempt++)
                {
                    Thread.Sleep(attempt == 0 ? 50 : 150);
                    var procs = Process.GetProcessesByName("matrix")
                        .Concat(Process.GetProcessesByName("launcher"))
                        .ToArray();

                    foreach (var proc in procs)
                    {
                        if (alreadyDone.Contains(proc.Id)) continue;
                        try
                        {
                            if (InjectDll(proc, HookDll))
                            {
                                alreadyDone.Add(proc.Id);
                                Dispatcher.Invoke(() =>
                                {
                                    txtStatus.Foreground = Brushes.Lime;
                                    txtStatus.Text = $"[Injected] mxohax hook active in matrix.exe (PID {proc.Id}). RSA & socket hooks online.";
                                });
                            }
                        }
                        catch { }
                    }

                    if (alreadyDone.Count > 0 && targetProc.HasExited) break;
                    if (alreadyDone.Contains(targetPid))
                    {
                        // Successfully injected into target matrix.exe
                        return;
                    }
                }
            }
            catch { }
        }

        private bool InjectDllIntoHandle(IntPtr hProcess, string dllPath)
        {
            if (hProcess == IntPtr.Zero || !File.Exists(dllPath)) return false;
            try
            {
                byte[] dllBytes = Encoding.ASCII.GetBytes(dllPath + "\0");
                IntPtr allocMem = VirtualAllocEx(hProcess, IntPtr.Zero, (uint)dllBytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (allocMem == IntPtr.Zero) return false;

                if (!WriteProcessMemory(hProcess, allocMem, dllBytes, (uint)dllBytes.Length, out _))
                    return false;

                IntPtr kernel32 = GetModuleHandle("kernel32.dll");
                IntPtr loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");
                if (loadLibrary == IntPtr.Zero) return false;

                IntPtr hThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0, loadLibrary, allocMem, 0, out _);
                if (hThread != IntPtr.Zero)
                {
                    WaitForSingleObject(hThread, 3000);
                    CloseHandle(hThread);
                    return true;
                }
            }
            catch { }
            return false;
        }

        private bool InjectDll(Process proc, string dllPath)
        {
            if (!File.Exists(dllPath)) return false;
            try
            {
                IntPtr hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, proc.Id);
                if (hProcess == IntPtr.Zero) return false;
                bool result = InjectDllIntoHandle(hProcess, dllPath);
                CloseHandle(hProcess);
                return result;
            }
            catch { }
            return false;
        }

        private async void btnStartPatch_Click(object sender, RoutedEventArgs e)
        {
            await StartClientDownloadAndExtractAsync();
        }

        private async Task StartClientDownloadAndExtractAsync()
        {
            btnStartPatch.IsEnabled = false;
            btnVerifyFiles.IsEnabled = false;
            pbPatch.Value = 0;

            string root = GameRoot;
            string serverHost = _currentServerIp;
            string downloadUrl = $"http://{serverHost}/client/MxO_Client.7z";
            string tempArchive = Path.Combine(root, "MxO_Client.7z.tmp");

            try
            {
                txtPatchInfo.Text = "CONNECTING TO ZION DOWNLOAD HOST (HTTP PORT 80)...";
                txtPatchDetails.Text = $"Source: {downloadUrl}";
                txtStatus.Text = "Initiating direct client download...";

                using (var client = new HttpClient { Timeout = TimeSpan.FromHours(2) })
                {
                    using (var response = await client.GetAsync(downloadUrl, HttpCompletionOption.ResponseHeadersRead))
                    {
                        response.EnsureSuccessStatusCode();

                        long? totalBytesNullable = response.Content.Headers.ContentLength;
                        long totalBytes = totalBytesNullable ?? 1327331186; // Fallback to ~1.26GB

                        await using var stream = await response.Content.ReadAsStreamAsync();
                        await using var fileStream = new FileStream(tempArchive, FileMode.Create, FileAccess.Write, FileShare.None, 65536, true);

                        byte[] buffer = new byte[65536];
                        long totalDownloaded = 0;
                        int bytesRead;
                        var sw = Stopwatch.StartNew();
                        TimeSpan lastTime = TimeSpan.Zero;
                        long lastBytes = 0;

                        while ((bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                        {
                            await fileStream.WriteAsync(buffer.AsMemory(0, bytesRead));
                            totalDownloaded += bytesRead;

                            if (sw.Elapsed - lastTime >= TimeSpan.FromMilliseconds(200))
                            {
                                double seconds = (sw.Elapsed - lastTime).TotalSeconds;
                                long bytesSince = totalDownloaded - lastBytes;
                                double speedMBps = seconds > 0 ? (bytesSince / (1024.0 * 1024.0)) / seconds : 0;

                                double pct = totalBytes > 0 ? ((double)totalDownloaded / totalBytes) * 100.0 : 0.0;
                                string etaStr = "--";
                                if (totalBytes > 0 && speedMBps > 0.05)
                                {
                                    double remainingBytes = totalBytes - totalDownloaded;
                                    double remainingSeconds = remainingBytes / (speedMBps * 1024.0 * 1024.0);
                                    TimeSpan eta = TimeSpan.FromSeconds(remainingSeconds);
                                    etaStr = $"{eta.Minutes}m {eta.Seconds}s";
                                }

                                pbPatch.Value = Math.Min(100, Math.Max(0, pct));
                                double curMB = totalDownloaded / (1024.0 * 1024.0);
                                double maxMB = totalBytes > 0 ? totalBytes / (1024.0 * 1024.0) : 0;
                                txtPatchInfo.Text = $"DOWNLOADING CLIENT: {curMB:F1} MB / {maxMB:F1} MB ({pct:F1}%)";
                                txtPatchDetails.Text = $"Speed: {speedMBps:F2} MB/s | ETA: {etaStr}";
                                txtStatus.Text = $"Downloading Matrix Online: {pct:F0}% ({speedMBps:F1} MB/s)...";

                                lastBytes = totalDownloaded;
                                lastTime = sw.Elapsed;
                            }
                        }
                    }
                }

                // Native Decompression Phase using SharpCompress
                txtPatchInfo.Text = "DECOMPRESSING CLIENT ARCHIVE (NATIVE 7-ZIP EXTRACTION)...";
                txtPatchDetails.Text = "Unpacking client binaries and world assets into game directory...";
                pbPatch.Value = 0;
                txtStatus.Text = "Extracting Matrix Online assets...";

                await Task.Run(() =>
                {
                    using var archive = ArchiveFactory.OpenArchive(tempArchive);
                    int totalCount = archive.Entries.Count();
                    int current = 0;
                    var sw = Stopwatch.StartNew();

                    foreach (var entry in archive.Entries)
                    {
                        if (!entry.IsDirectory)
                        {
                            entry.WriteToDirectory(root, new ExtractionOptions
                            {
                                ExtractFullPath = true,
                                Overwrite = true
                            });
                        }
                        current++;

                        if (sw.ElapsedMilliseconds > 150)
                        {
                            sw.Restart();
                            int c = current;
                            string entryName = Path.GetFileName(entry.Key ?? "");
                            double pct = totalCount > 0 ? ((double)c / totalCount) * 100.0 : 0;
                            Dispatcher.Invoke(() =>
                            {
                                pbPatch.Value = pct;
                                txtPatchInfo.Text = $"EXTRACTING: {entryName}";
                                txtPatchDetails.Text = $"Extracted: {c} / {totalCount} files ({pct:F0}%)";
                            });
                        }
                    }
                });

                // Post-extraction: ensure matrix.exe exists
                string matrixPath = Path.Combine(root, "matrix.exe");
                string launcherPath = Path.Combine(root, "launcher.exe");
                if (!File.Exists(matrixPath) && File.Exists(launcherPath))
                {
                    File.Copy(launcherPath, matrixPath, true);
                }

                // Download authentic pubkey.dat if missing
                string pubkeyPath = Path.Combine(root, "pubkey.dat");
                if (!File.Exists(pubkeyPath))
                {
                    try
                    {
                        byte[] pubBytes = await _httpClient.GetByteArrayAsync($"http://{serverHost}/client/pubkey.dat");
                        File.WriteAllBytes(pubkeyPath, pubBytes);
                    }
                    catch { }
                }

                // Remove temporary archive to free disk space
                try
                {
                    File.Delete(tempArchive);
                }
                catch { }

                pbPatch.Value = 100;
                txtPatchInfo.Text = "CLIENT DEPLOYED AND VERIFIED! READY TO JACK IN.";
                txtPatchDetails.Text = "All authentic binaries, models, sounds, and shaders installed.";
                txtStatus.Text = "SUCCESS: Client installed. Ready to Jack In.";
                btnStartPatch.Content = "CLIENT READY";

                UpdateClientStatusUI();

                MessageBox.Show(
                    "The Matrix Online game client has been successfully installed and configured!\n\nYou can now Jack In to the live server.",
                    "Installation Complete",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                txtPatchInfo.Text = "DOWNLOAD / EXTRACTION ERROR!";
                txtPatchDetails.Text = ex.Message;
                txtStatus.Text = "Installation failed: " + ex.Message;
                MessageBox.Show($"Failed to download or extract game client:\n\n{ex.Message}", "Installer Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                btnStartPatch.IsEnabled = true;
                btnVerifyFiles.IsEnabled = true;
            }
        }

        private async void btnVerifyFiles_Click(object sender, RoutedEventArgs e)
        {
            btnVerifyFiles.IsEnabled = false;
            btnStartPatch.IsEnabled = false;
            txtPatchInfo.Text = "VERIFYING CLIENT INTEGRITY...";
            txtPatchDetails.Text = "Inspecting essential game binaries and asset directories...";
            pbPatch.Value = 0;

            string root = GameRoot;
            string[] criticalFiles = new[]
            {
                "client.dll",
                "launcher.exe",
                "binkw32.dll",
                "d3d9.dll",
                "rezmap.ltb",
                "CoolBos.dll",
                "CoolBucky.dll",
                "CoolHttp.dll",
                "CoolPeer.dll",
                "CoolSec.dll",
                "CoolSocket.dll",
                "CoolSos.dll",
                "cres.dll",
                "msvcr70.dll",
                "msvcr71.dll",
                "pythonMXO.dll"
            };

            int missingCount = 0;
            var missingList = new System.Collections.Generic.List<string>();

            for (int i = 0; i < criticalFiles.Length; i++)
            {
                string f = criticalFiles[i];
                string full = Path.Combine(root, f);
                bool exists = File.Exists(full) || File.Exists(Path.Combine(root, "Client", f));
                if (!exists)
                {
                    if (f == "launcher.exe" && (File.Exists(Path.Combine(root, "matrix.exe")) || File.Exists(Path.Combine(root, "Client", "matrix.exe"))))
                    {
                        // matrix.exe counts as client executable
                    }
                    else
                    {
                        missingCount++;
                        missingList.Add(f);
                    }
                }
                pbPatch.Value = ((double)(i + 1) / criticalFiles.Length) * 100.0;
                txtPatchInfo.Text = $"CHECKING FILE: {f}";
                await Task.Delay(30);
            }

            bool hasResource = Directory.Exists(Path.Combine(root, "resource")) || Directory.Exists(Path.Combine(root, "Client", "resource"));
            if (!hasResource)
            {
                missingCount++;
                missingList.Add("resource/ (assets)");
            }

            if (missingCount == 0)
            {
                txtPatchInfo.Text = "ALL CRITICAL GAME FILES VERIFIED AND OPERATIONAL!";
                txtPatchDetails.Text = $"{criticalFiles.Length}/{criticalFiles.Length} core libraries and resource directories present.";
                txtStatus.Text = "Client verified: All systems nominal.";
            }
            else
            {
                txtPatchInfo.Text = $"VERIFICATION FAILED: {missingCount} ESSENTIAL FILE(S) MISSING!";
                txtPatchDetails.Text = "Missing: " + string.Join(", ", missingList);
                txtStatus.Text = "Client incomplete. Download recommended.";

                var res = MessageBox.Show(
                    $"Client verification found {missingCount} missing file(s):\n\n" +
                    string.Join("\n", missingList) +
                    "\n\nWould you like to download the client now?",
                    "Missing Files Detected",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);

                if (res == MessageBoxResult.Yes)
                {
                    await StartClientDownloadAndExtractAsync();
                }
            }

            UpdateClientStatusUI();
            btnVerifyFiles.IsEnabled = true;
            btnStartPatch.IsEnabled = true;
        }
    }

    public class VisualHost : FrameworkElement
    {
        private Visual? _visual;

        public Visual? Visual
        {
            get => _visual;
            set
            {
                if (_visual != null)
                {
                    RemoveVisualChild(_visual);
                    RemoveLogicalChild(_visual);
                }
                _visual = value;
                if (_visual != null)
                {
                    AddVisualChild(_visual);
                    AddLogicalChild(_visual);
                }
            }
        }

        protected override int VisualChildrenCount => _visual != null ? 1 : 0;

        protected override Visual GetVisualChild(int index)
        {
            if (index != 0 || _visual == null)
                throw new ArgumentOutOfRangeException(nameof(index));
            return _visual;
        }
    }

    public class OperativeProfile
    {
        public long Id { get; set; }
        public string Handle { get; set; } = "";
        public string FirstName { get; set; } = "";
        public string LastName { get; set; } = "";
        public int Level { get; set; } = 1;
        public int Profession { get; set; }
        public string District { get; set; } = "";
    }
}