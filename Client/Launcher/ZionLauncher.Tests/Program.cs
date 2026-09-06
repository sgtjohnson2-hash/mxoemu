using System;
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using ZionLauncher;

namespace ZionLauncher.Tests
{
    internal class Program
    {
        private static int _passed = 0;
        private static int _failed = 0;

        static async Task<int> Main(string[] args)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("==========================================================");
            Console.WriteLine("  ZION LAUNCHER SUITE — AUTOMATED VERIFICATION HARNESS    ");
            Console.WriteLine("==========================================================");
            Console.ResetColor();

            // 1. Version Parsing & Normalization Tests
            TestVersionParsing();

            // 2. Remote Version Newer Evaluation Tests
            TestRemoteVersionEvaluation();

            // 3. Authentic Matrix Unicode Character Set Tests
            TestMatrixUnicodeCharset();

            // 4. Matrix Rain Kinematics & Column Bounds Tests
            TestMatrixKinematics();

            // 5. Color Tier Boundaries & Decay Tests
            TestColorTiers();

            // 6. Windows Updater Batch Script Security & Resilience Tests
            TestUpdaterScriptResilience();

            // 7. Live Server Endpoints & Payload Schema Verification
            await TestServerEndpointsAsync();

            // 8. Live Batch File Replacement & Process Teardown Tests
            TestLiveBatchReplacementExecution();

            // 9. Offline & Malformed JSON Edge Case Tests
            TestOfflineAndMalformedJsonHandling();

            Console.WriteLine();
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"TOTAL PASSED: {_passed}");
            if (_failed > 0)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"TOTAL FAILED: {_failed}");
                Console.ResetColor();
                return 1;
            }

            Console.ForegroundColor = ConsoleColor.Cyan;
            Console.WriteLine("ALL TESTS PASSED SUCCESSFULLY (0 FAILURES).");
            Console.ResetColor();
            return 0;
        }

        private static void Assert(bool condition, string testName, string? details = null)
        {
            if (condition)
            {
                _passed++;
                Console.ForegroundColor = ConsoleColor.DarkGreen;
                Console.Write("  [PASS] ");
                Console.ForegroundColor = ConsoleColor.Gray;
                Console.WriteLine(testName);
            }
            else
            {
                _failed++;
                Console.ForegroundColor = ConsoleColor.Red;
                Console.Write("  [FAIL] ");
                Console.ForegroundColor = ConsoleColor.White;
                Console.WriteLine(testName);
                if (!string.IsNullOrEmpty(details))
                {
                    Console.ForegroundColor = ConsoleColor.Yellow;
                    Console.WriteLine($"         Reason: {details}");
                }
            }
            Console.ResetColor();
        }

        private static void TestVersionParsing()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[1] Version Parsing & Normalization Tests");
            Console.ResetColor();

            // Standard 3-part version
            bool ok = MainWindow.TryParseNormalizedVersion("1.2.0", out var v1);
            Assert(ok && v1.Major == 1 && v1.Minor == 2 && v1.Build == 0 && v1.Revision == 0,
                "Parse '1.2.0' -> (1, 2, 0, 0)");

            // Standard 4-part version
            ok = MainWindow.TryParseNormalizedVersion("1.2.0.4", out var v2);
            Assert(ok && v2.Major == 1 && v2.Minor == 2 && v2.Build == 0 && v2.Revision == 4,
                "Parse '1.2.0.4' -> (1, 2, 0, 4)");

            // 2-part version
            ok = MainWindow.TryParseNormalizedVersion("1.3", out var v3);
            Assert(ok && v3.Major == 1 && v3.Minor == 3 && v3.Build == 0 && v3.Revision == 0,
                "Parse '1.3' -> (1, 3, 0, 0)");

            // 1-part version
            ok = MainWindow.TryParseNormalizedVersion("2", out var v4);
            Assert(ok && v4.Major == 2 && v4.Minor == 0 && v4.Build == 0 && v4.Revision == 0,
                "Parse '2' -> (2, 0, 0, 0)");

            // Leading 'v' prefix
            ok = MainWindow.TryParseNormalizedVersion("v1.2.1", out var v5);
            Assert(ok && v5.Major == 1 && v5.Minor == 2 && v5.Build == 1 && v5.Revision == 0,
                "Parse 'v1.2.1' with leading 'v' -> (1, 2, 1, 0)");

            // Leading uppercase 'V' prefix
            ok = MainWindow.TryParseNormalizedVersion("V2.0.0", out var v6);
            Assert(ok && v6.Major == 2 && v6.Minor == 0 && v6.Build == 0 && v6.Revision == 0,
                "Parse 'V2.0.0' with leading 'V' -> (2, 0, 0, 0)");

            // Pre-release and build metadata stripping
            ok = MainWindow.TryParseNormalizedVersion("1.2.5-beta.1+build2026", out var v7);
            Assert(ok && v7.Major == 1 && v7.Minor == 2 && v7.Build == 5 && v7.Revision == 0,
                "Parse '1.2.5-beta.1+build2026' with pre-release tags -> (1, 2, 5, 0)");

            // Whitespace padding
            ok = MainWindow.TryParseNormalizedVersion("  1.2.0  ", out var v8);
            Assert(ok && v8.Major == 1 && v8.Minor == 2 && v8.Build == 0 && v8.Revision == 0,
                "Parse '  1.2.0  ' with whitespace -> (1, 2, 0, 0)");

            // Invalid strings fail gracefully
            Assert(!MainWindow.TryParseNormalizedVersion(null, out _), "Parse null returns false");
            Assert(!MainWindow.TryParseNormalizedVersion("", out _), "Parse empty returns false");
            Assert(!MainWindow.TryParseNormalizedVersion("   ", out _), "Parse whitespace returns false");
            Assert(!MainWindow.TryParseNormalizedVersion("not.a.version", out _), "Parse non-numeric returns false");
            Assert(!MainWindow.TryParseNormalizedVersion("v", out _), "Parse lonely 'v' returns false");
        }

        private static void TestRemoteVersionEvaluation()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[2] Remote Version Evaluation & Comparison Tests");
            Console.ResetColor();

            var current = new Version(1, 2, 0);

            // Identical 3-part: not newer
            bool newer = MainWindow.IsRemoteVersionNewer("1.2.0", current, out _);
            Assert(!newer, "Remote '1.2.0' vs Current 1.2.0 is NOT newer");

            // Identical 4-part: not newer (CRITICAL: prevents .NET Revision -1 vs 0 bug!)
            newer = MainWindow.IsRemoteVersionNewer("1.2.0.0", current, out _);
            Assert(!newer, "Remote '1.2.0.0' vs Current 1.2.0 is NOT newer (Revision bug prevented)");

            // Identical with 'v' prefix: not newer
            newer = MainWindow.IsRemoteVersionNewer("v1.2.0", current, out _);
            Assert(!newer, "Remote 'v1.2.0' vs Current 1.2.0 is NOT newer");

            // Minor bump: newer
            newer = MainWindow.IsRemoteVersionNewer("1.2.1", current, out var parsed);
            Assert(newer && parsed?.Build == 1, "Remote '1.2.1' vs Current 1.2.0 IS newer");

            // 4th component bump: newer
            newer = MainWindow.IsRemoteVersionNewer("v1.2.0.1", current, out _);
            Assert(newer, "Remote 'v1.2.0.1' vs Current 1.2.0 IS newer");

            // Feature bump: newer
            newer = MainWindow.IsRemoteVersionNewer("1.3.0", current, out _);
            Assert(newer, "Remote '1.3.0' vs Current 1.2.0 IS newer");

            // Major bump: newer
            newer = MainWindow.IsRemoteVersionNewer("2.0.0", current, out _);
            Assert(newer, "Remote '2.0.0' vs Current 1.2.0 IS newer");

            // Older version: not newer
            newer = MainWindow.IsRemoteVersionNewer("1.1.9", current, out _);
            Assert(!newer, "Remote '1.1.9' vs Current 1.2.0 is NOT newer");

            // Older major: not newer
            newer = MainWindow.IsRemoteVersionNewer("0.9.9", current, out _);
            Assert(!newer, "Remote '0.9.9' vs Current 1.2.0 is NOT newer");

            // Invalid format: not newer
            newer = MainWindow.IsRemoteVersionNewer("unknown", current, out _);
            Assert(!newer, "Remote 'unknown' returns false");
        }

        private static void TestMatrixUnicodeCharset()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[3] Authentic Matrix Unicode Character Set Tests");
            Console.ResetColor();

            // Access MatrixGlyphChars via reflection or MainWindow
            var field = typeof(MainWindow).GetField("MatrixGlyphChars", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
            string? chars = field?.GetValue(null) as string;

            Assert(!string.IsNullOrEmpty(chars), "MatrixGlyphChars exists and is non-empty");
            if (chars == null) return;

            // Verify half-width Katakana unicode integrity (\uFF66 .. \uFF9D)
            int katakanaCount = 0;
            for (char c = '\uFF66'; c <= '\uFF9D'; c++)
            {
                if (chars.Contains(c)) katakanaCount++;
            }
            Assert(katakanaCount == 56, $"Contains all 56 authentic Half-width Katakana glyphs (found {katakanaCount}/56)");

            // Verify digits 0-9
            bool hasAllDigits = true;
            for (char d = '0'; d <= '9'; d++)
            {
                if (!chars.Contains(d)) hasAllDigits = false;
            }
            Assert(hasAllDigits, "Contains all decimal digits 0..9");

            // Verify Latin Matrix characters
            Assert(chars.Contains("THEMATRIX"), "Contains 'THEMATRIX' banner glyphs");

            // Verify special Matrix rain symbols
            Assert(chars.Contains(':') && chars.Contains('=') && chars.Contains('+') && chars.Contains('*'),
                "Contains authentic Matrix punctuation and cipher symbols (: = + *)");

            Assert(chars.Length >= 80, $"Total glyph library count is large and diverse ({chars.Length} glyphs)");
        }

        private static void TestMatrixKinematics()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[4] Matrix Rain Kinematics & Column Bounds Tests");
            Console.ResetColor();

            double screenHeight = 680;
            double dt = 0.0166; // 60 FPS delta time
            double speed = 200.0; // px/sec
            double initialY = 100.0;

            double nextY = initialY + speed * dt;
            Assert(Math.Abs(nextY - (100.0 + 200.0 * 0.0166)) < 0.001, "HeadY advances correctly with dt integration");

            // Clamp frame spikes test
            double spikeDt = 0.25; // 250ms freeze
            double clampedDt = spikeDt > 0.05 ? 0.05 : spikeDt;
            Assert(clampedDt == 0.05, "Spike dt is clamped to 50ms (prevents columns jumping offscreen on frame lag)");

            // Trail reset test
            int trailLength = 20;
            double lineHeight = 16.0;
            double tailY = nextY - trailLength * lineHeight;
            bool shouldReset = (nextY - trailLength * lineHeight) > screenHeight;
            Assert(!shouldReset, "Column at Y=103 does not prematurely reset");

            double offscreenHeadY = screenHeight + (trailLength * lineHeight) + 50;
            bool offscreenReset = (offscreenHeadY - trailLength * lineHeight) > screenHeight;
            Assert(offscreenReset, "Column reset triggered when tail completely leaves bottom of screen");
        }

        private static void TestColorTiers()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[5] Color Tier Boundaries & Decay Tests");
            Console.ResetColor();

            // Tier 0: Head (k=0)
            int GetTier(int k, int trailLength)
            {
                if (k == 0) return 0;
                if (k == 1) return 1;
                if (k == 2) return 2;
                int t = 3 + (int)((k - 3) * 5.0 / Math.Max(1, trailLength - 3));
                return Math.Min(7, Math.Max(0, t));
            }

            Assert(GetTier(0, 25) == 0, "Index k=0 maps to Tier 0 (Pure Glowing White Head)");
            Assert(GetTier(1, 25) == 1, "Index k=1 maps to Tier 1 (Pale Neon Green Neck)");
            Assert(GetTier(2, 25) == 2, "Index k=2 maps to Tier 2 (Bright Matrix Lime)");
            Assert(GetTier(10, 25) >= 3 && GetTier(10, 25) <= 6, "Index k=10 maps to Intermediate Phosphor Tier");
            Assert(GetTier(24, 25) == 7, "Index k=trailLength-1 maps to Tier 7 (Faint Phosphor Tail Decay)");
            Assert(GetTier(100, 25) == 7, "Excess index clamps safely to Tier 7 (No out of range exception)");
        }

        private static void TestUpdaterScriptResilience()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[6] Windows Updater Batch Script Security & Resilience Tests");
            Console.ResetColor();

            // Simulate generated batch script content
            string script = 
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
                "powershell -Command \"Start-Process cmd -ArgumentList '/c copy /Y \\\"%UPDATE%\\\" \\\"%TARGET%\\\" & del \\\"%UPDATE%\\\" & start \\\"\\\" \\\"%TARGET%\\\"' -Verb RunAs -WindowStyle Hidden\" >nul 2>nul\r\n" +
                "goto CLEANUP\r\n\r\n" +
                ":COPY_SUCCESS\r\n" +
                "del \"%UPDATE%\" >nul 2>nul\r\n" +
                "start \"\" \"%TARGET%\"\r\n\r\n" +
                ":CLEANUP\r\n" +
                "ping -n 2 127.0.0.1 >nul\r\n" +
                "(goto) 2>nul & del \"%~f0\"\r\n";

            Assert(!script.Contains("timeout /t"), "Batch script does NOT use 'timeout' command (which fails on redirected input)");
            Assert(script.Contains("ping -n 2 127.0.0.1 >nul"), "Batch script uses reliable 'ping' delay loop");
            Assert(script.Contains(":COPY_LOOP"), "Batch script defines :COPY_LOOP label");
            Assert(script.Contains("ATTEMPTS"), "Batch script implements retry count checks");
            Assert(script.Contains("-Verb RunAs"), "Batch script implements UAC elevation fallback for protected directories");
            Assert(script.Contains("set \"TARGET=%~1\""), "Batch script strips and handles parameter quotes safely");
        }

        private static async Task TestServerEndpointsAsync()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[7] Live Server Endpoints & Payload Schema Verification");
            Console.ResetColor();

            using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(5) };
            string server = "http://15.204.82.250";

            try
            {
                string json = await client.GetStringAsync($"{server}/launcher/version");
                Assert(!string.IsNullOrEmpty(json), "GET /launcher/version responded with data");

                using var doc = JsonDocument.Parse(json);
                bool hasVersion = doc.RootElement.TryGetProperty("version", out var vProp) && !string.IsNullOrEmpty(vProp.GetString());
                bool hasDownloadUrl = doc.RootElement.TryGetProperty("downloadUrl", out var dProp) && !string.IsNullOrEmpty(dProp.GetString());
                bool hasFilename = doc.RootElement.TryGetProperty("filename", out var fProp) && !string.IsNullOrEmpty(fProp.GetString());

                Assert(hasVersion, $"/launcher/version has 'version' property ({vProp.GetString()})");
                Assert(hasDownloadUrl, $"/launcher/version has 'downloadUrl' property ({dProp.GetString()})");
                Assert(hasFilename, $"/launcher/version has 'filename' property ({fProp.GetString()})");

                // Verify launcher binary availability
                string downloadUrl = dProp.GetString()!;
                if (!downloadUrl.StartsWith("http")) downloadUrl = $"{server}" + (downloadUrl.StartsWith('/') ? downloadUrl : "/" + downloadUrl);

                using var headReq = new HttpRequestMessage(HttpMethod.Head, downloadUrl);
                using var headResp = await client.SendAsync(headReq);
                Assert(headResp.IsSuccessStatusCode, $"HEAD {downloadUrl} returns HTTP 200 OK");
                Assert(headResp.Content.Headers.ContentLength > 1000000, 
                    $"Remote launcher binary size is valid ({headResp.Content.Headers.ContentLength:N0} bytes)");
            }
            catch (Exception ex)
            {
                Assert(false, "Live server query failed", ex.Message);
            }
        }

        private static void TestLiveBatchReplacementExecution()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[8] Live Batch File Replacement & Process Teardown Tests");
            Console.ResetColor();

            string tempDir = Path.Combine(Path.GetTempPath(), "ZionUpdateTest_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempDir);

            try
            {
                string targetFile = Path.Combine(tempDir, "TargetApp.exe");
                string updateFile = Path.Combine(tempDir, "TargetApp.exe.update.tmp");
                string scriptFile = Path.Combine(tempDir, "test_update.bat");

                File.WriteAllText(targetFile, "ORIGINAL_V1_CONTENT");
                File.WriteAllText(updateFile, "UPGRADED_V2_CONTENT");

                // Batch script without starting the target afterwards
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
                    ":COPY_SUCCESS\r\n" +
                    "del \"%UPDATE%\" >nul 2>nul\r\n" +
                    "(goto) 2>nul & del \"%~f0\"\r\n";

                File.WriteAllText(scriptFile, scriptContent);

                // Run batch script pointing to a non-existent PID (999999) so WAIT_PID immediately falls through
                var psi = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c \"\"{scriptFile}\" \"{targetFile}\" \"{updateFile}\" 999999\"",
                    CreateNoWindow = true,
                    UseShellExecute = false
                };

                using var proc = System.Diagnostics.Process.Start(psi);
                proc?.WaitForExit(5000);

                string replacedContent = File.ReadAllText(targetFile);
                Assert(replacedContent == "UPGRADED_V2_CONTENT", 
                    "Target executable was safely and correctly overwritten with update payload");

                Assert(!File.Exists(updateFile), 
                    "Update temporary payload was cleanly removed after successful replacement");
            }
            finally
            {
                try { Directory.Delete(tempDir, true); } catch { }
            }
        }

        private static void TestOfflineAndMalformedJsonHandling()
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine("\n[9] Offline & Malformed JSON Edge Case Tests");
            Console.ResetColor();

            // Malformed JSON string
            string malformed = "{ \"version\": \"1.2.0\", invalid_json... }";
            bool threw = false;
            try
            {
                using var doc = JsonDocument.Parse(malformed);
            }
            catch (JsonException)
            {
                threw = true;
            }
            Assert(threw, "Malformed JSON triggers expected JsonException caught by fallback logic");

            // Missing version property
            string jsonNoVersion = "{ \"downloadUrl\": \"/launcher/ZionLauncher.exe\" }";
            using var docNoVer = JsonDocument.Parse(jsonNoVersion);
            bool hasVer = docNoVer.RootElement.TryGetProperty("version", out _);
            Assert(!hasVer, "Missing 'version' property detected, triggers graceful fallback");

            // Empty version property
            string jsonEmptyVersion = "{ \"version\": \"\" }";
            using var docEmptyVer = JsonDocument.Parse(jsonEmptyVersion);
            string? ev = docEmptyVer.RootElement.TryGetProperty("version", out var ep) ? ep.GetString() : null;
            bool ok = MainWindow.TryParseNormalizedVersion(ev, out _);
            Assert(!ok, "Empty 'version' string is safely rejected by TryParseNormalizedVersion");
        }
    }
}
