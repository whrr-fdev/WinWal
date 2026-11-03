using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Diagnostics;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: AssemblyTitle("WinWal Setup")]
[assembly: AssemblyProduct("WinWal — Windows 10 Control Center")]
[assembly: AssemblyVersion("1.2.0.0")]

namespace WinWalSetup {
    public class SetupForm : Form {
        private TextBox txtPath;
        private Button btnBrowse;
        private CheckBox chkDesktop;
        private CheckBox chkStartMenu;
        private CheckBox chkAutostart;
        private CheckBox chkLaunchNow;
        private ProgressBar progressBar;
        private Label lblStatus;
        private Button btnInstall;
        private Button btnCancel;

        public SetupForm() {
            this.Text = "Установка WinWal — Windows 10 Control Center";
            this.Size = new Size(560, 410);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(14, 15, 20);
            this.ForeColor = Color.White;
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            try {
                this.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            } catch {}

            Label lblTitle = new Label {
                Text = "WinWal — Центр кастомизации Windows 10",
                Font = new Font("Segoe UI", 14f, FontStyle.Bold),
                ForeColor = Color.White,
                Location = new Point(24, 18),
                AutoSize = true
            };
            Label lblSub = new Label {
                Text = "Окна • Панель задач Win11 • Живые видеообои • Виджеты • Живая атмосфера",
                Font = new Font("Segoe UI", 9f, FontStyle.Regular),
                ForeColor = Color.FromArgb(160, 174, 192),
                Location = new Point(25, 48),
                AutoSize = true
            };

            Label lblPath = new Label {
                Text = "Папка установки:",
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                ForeColor = Color.FromArgb(226, 232, 240),
                Location = new Point(24, 84),
                AutoSize = true
            };

            string defaultDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "WinWal");
            txtPath = new TextBox {
                Text = defaultDir,
                Location = new Point(24, 108),
                Size = new Size(390, 28),
                BackColor = Color.FromArgb(26, 28, 36),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                Font = new Font("Segoe UI", 10f)
            };

            btnBrowse = new Button {
                Text = "Обзор...",
                Location = new Point(424, 106),
                Size = new Size(96, 29),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(38, 42, 54),
                ForeColor = Color.White,
                Cursor = Cursors.Hand
            };
            btnBrowse.FlatAppearance.BorderColor = Color.FromArgb(70, 78, 98);
            btnBrowse.Click += (s, e) => {
                using (var fbd = new FolderBrowserDialog()) {
                    fbd.Description = "Выберите папку для установки WinWal:";
                    fbd.SelectedPath = txtPath.Text;
                    if (fbd.ShowDialog() == DialogResult.OK && !string.IsNullOrWhiteSpace(fbd.SelectedPath)) {
                        string sel = fbd.SelectedPath;
                        if (!sel.EndsWith("WinWal", StringComparison.OrdinalIgnoreCase))
                            sel = Path.Combine(sel, "WinWal");
                        txtPath.Text = sel;
                    }
                }
            };

            chkDesktop = new CheckBox {
                Text = "Создать ярлык на Рабочем столе",
                Checked = true,
                Location = new Point(26, 152),
                AutoSize = true,
                ForeColor = Color.FromArgb(226, 232, 240)
            };
            chkStartMenu = new CheckBox {
                Text = "Создать ярлыки в меню «Пуск»",
                Checked = true,
                Location = new Point(26, 180),
                AutoSize = true,
                ForeColor = Color.FromArgb(226, 232, 240)
            };
            chkAutostart = new CheckBox {
                Text = "Запускать WinWal в фоновом режиме при входе в Windows",
                Checked = true,
                Location = new Point(26, 208),
                AutoSize = true,
                ForeColor = Color.FromArgb(226, 232, 240)
            };
            chkLaunchNow = new CheckBox {
                Text = "Запустить WinWal сразу после завершения установки",
                Checked = true,
                Location = new Point(26, 236),
                AutoSize = true,
                ForeColor = Color.FromArgb(226, 232, 240)
            };

            lblStatus = new Label {
                Text = "Готов к установке  •  Автор: whrr  •  Лицензия: MIT",
                Location = new Point(24, 272),
                Size = new Size(496, 20),
                ForeColor = Color.FromArgb(148, 163, 184)
            };

            progressBar = new ProgressBar {
                Location = new Point(24, 296),
                Size = new Size(496, 16),
                Minimum = 0,
                Maximum = 100,
                Value = 0
            };

            btnInstall = new Button {
                Text = "Установить",
                Location = new Point(296, 326),
                Size = new Size(116, 34),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(59, 130, 246),
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                Cursor = Cursors.Hand
            };
            btnInstall.FlatAppearance.BorderSize = 0;
            btnInstall.Click += async (s, e) => await RunInstallAsync();

            btnCancel = new Button {
                Text = "Выход",
                Location = new Point(422, 326),
                Size = new Size(98, 34),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(38, 42, 54),
                ForeColor = Color.White,
                Cursor = Cursors.Hand
            };
            btnCancel.FlatAppearance.BorderColor = Color.FromArgb(70, 78, 98);
            btnCancel.Click += (s, e) => this.Close();

            this.Controls.AddRange(new Control[] {
                lblTitle, lblSub, lblPath, txtPath, btnBrowse,
                chkDesktop, chkStartMenu, chkAutostart, chkLaunchNow,
                lblStatus, progressBar, btnInstall, btnCancel
            });
        }

        private static void CreateShortcut(string lnkPath, string targetPath, string args, string workDir, string iconPath, string desc) {
            Type t = Type.GetTypeFromProgID("WScript.Shell");
            dynamic shell = Activator.CreateInstance(t);
            dynamic sc = shell.CreateShortcut(lnkPath);
            sc.TargetPath = targetPath;
            sc.Arguments = args;
            sc.WorkingDirectory = workDir;
            if (!string.IsNullOrEmpty(iconPath) && File.Exists(iconPath))
                sc.IconLocation = iconPath;
            sc.Description = desc;
            sc.Save();
        }

        private async Task RunInstallAsync() {
            string installDir = txtPath.Text.Trim();
            if (string.IsNullOrEmpty(installDir)) {
                MessageBox.Show("Укажите корректный путь установки.", "WinWal Setup", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            btnInstall.Enabled = false;
            btnBrowse.Enabled = false;
            txtPath.Enabled = false;
            chkDesktop.Enabled = chkStartMenu.Enabled = chkAutostart.Enabled = chkLaunchNow.Enabled = false;

            try {
                lblStatus.Text = "Остановка запущенных процессов WinWal...";
                foreach (var p in Process.GetProcessesByName("WinWal")) {
                    try { p.Kill(); p.WaitForExit(1500); } catch {}
                }

                await Task.Run(() => {
                    Directory.CreateDirectory(installDir);
                    using (Stream resStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("WinWalPayload.zip")) {
                        if (resStream == null)
                            throw new Exception("Не найден встроенный архив WinWalPayload.zip.");
                        using (ZipArchive archive = new ZipArchive(resStream, ZipArchiveMode.Read)) {
                            int total = Math.Max(1, archive.Entries.Count);
                            int current = 0;
                            foreach (ZipArchiveEntry entry in archive.Entries) {
                                current++;
                                string fullDest = Path.Combine(installDir, entry.FullName.Replace('/', Path.DirectorySeparatorChar));
                                if (string.IsNullOrEmpty(entry.Name)) {
                                    Directory.CreateDirectory(fullDest);
                                    continue;
                                }
                                Directory.CreateDirectory(Path.GetDirectoryName(fullDest));
                                entry.ExtractToFile(fullDest, true);
                                int pct = Math.Min(95, (current * 95) / total);
                                this.Invoke((Action)(() => {
                                    progressBar.Value = pct;
                                    lblStatus.Text = "Распаковка: " + entry.Name;
                                }));
                            }
                        }
                    }
                });

                string exePath = Path.Combine(installDir, "WinWal.exe");
                string icoPath = Path.Combine(installDir, "winwal.ico");
                string uninstBat = Path.Combine(installDir, "Uninstall-WinWal.bat");

                string batContent = "@echo off\r\n" +
                    "chcp 65001 >nul\r\n" +
                    "taskkill /F /IM WinWal.exe >nul 2>&1\r\n" +
                    "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"WinWal\" /f >nul 2>&1\r\n" +
                    "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinWal\" /f >nul 2>&1\r\n" +
                    "del /F /Q \"%USERPROFILE%\\Desktop\\WinWal.lnk\" >nul 2>&1\r\n" +
                    "rmdir /S /Q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\WinWal\" >nul 2>&1\r\n" +
                    "cd /d \"%TEMP%\"\r\n" +
                    "rmdir /S /Q \"" + installDir + "\" >nul 2>&1\r\n";
                File.WriteAllText(uninstBat, batContent, System.Text.Encoding.UTF8);

                if (chkDesktop.Checked) {
                    string deskLnk = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "WinWal.lnk");
                    CreateShortcut(deskLnk, exePath, "", installDir, icoPath, "WinWal — Центр кастомизации Windows 10");
                }

                if (chkStartMenu.Checked) {
                    string smDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.StartMenu), "Programs", "WinWal");
                    Directory.CreateDirectory(smDir);
                    CreateShortcut(Path.Combine(smDir, "WinWal.lnk"), exePath, "", installDir, icoPath, "WinWal — Центр кастомизации Windows 10");
                    CreateShortcut(Path.Combine(smDir, "Удалить WinWal.lnk"), uninstBat, "", installDir, icoPath, "Удалить WinWal");
                }

                using (RegistryKey runKey = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run", true)) {
                    if (runKey != null) {
                        if (chkAutostart.Checked)
                            runKey.SetValue("WinWal", "\"" + exePath + "\" --daemon");
                        else
                            runKey.DeleteValue("WinWal", false);
                    }
                }

                using (RegistryKey unKey = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\WinWal")) {
                    if (unKey != null) {
                        unKey.SetValue("DisplayName", "WinWal — Windows 10 Control Center");
                        unKey.SetValue("DisplayIcon", icoPath);
                        unKey.SetValue("InstallLocation", installDir);
                        unKey.SetValue("Publisher", "whrr");
                        unKey.SetValue("DisplayVersion", "1.2.0");
                        unKey.SetValue("UninstallString", "\"" + uninstBat + "\"");
                        unKey.SetValue("NoModify", 1, RegistryValueKind.DWord);
                        unKey.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                    }
                }

                progressBar.Value = 100;
                lblStatus.Text = "Установка успешно завершена!";
                btnCancel.Text = "Готово";

                if (chkLaunchNow.Checked && File.Exists(exePath)) {
                    Process.Start(new ProcessStartInfo {
                        FileName = exePath,
                        WorkingDirectory = installDir,
                        UseShellExecute = true
                    });
                }

                MessageBox.Show("WinWal успешно установлен в:\n" + installDir, "Установка завершена", MessageBoxButtons.OK, MessageBoxIcon.Information);
                this.Close();
            } catch (Exception ex) {
                lblStatus.Text = "Ошибка: " + ex.Message;
                MessageBox.Show("Ошибка при установке:\n" + ex.Message, "WinWal Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
                btnInstall.Enabled = true;
                btnBrowse.Enabled = true;
                txtPath.Enabled = true;
            }
        }

        [STAThread]
        public static void Main() {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new SetupForm());
        }
    }
}