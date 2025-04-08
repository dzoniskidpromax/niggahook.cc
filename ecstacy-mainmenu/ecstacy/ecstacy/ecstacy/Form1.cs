using System;
using System.Drawing;
using System.Windows.Forms;
using System.IO;
using System.Net.Http;
using System.Diagnostics;
using System.Management;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace CheatLoader
{
    public partial class Form1 : Form
    {
        private Button injectButton;
        private Label subStatusLabel;
        private bool isDragging = false;
        private Point dragStartPoint;
        private string exePath;
        private string hwidLockFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EcstacyFunLock.txt");
        private const string DownloadUrl = "https://ecstacy.dev/jesuschristfuckmymodda/neverwallahi.exe";
        private string subStatus = "inactive";

        public Form1()
        {
            InitializeComponent();
            ShowLoginForm();
        }

        private void ShowLoginForm()
        {
            Form loginForm = new Form
            {
                Size = new Size(300, 220),
                FormBorderStyle = FormBorderStyle.None,
                BackColor = Color.FromArgb(84, 84, 84),
                StartPosition = FormStartPosition.CenterScreen
            };

            loginForm.Paint += (s, e) =>
            {
                using (Pen pen = new Pen(Color.FromArgb(218, 112, 214), 2))
                {
                    e.Graphics.DrawRectangle(pen, 1, 1, loginForm.Width - 2, loginForm.Height - 2);
                }
            };

            loginForm.MouseDown += (s, e) => { if (e.Button == MouseButtons.Left) { isDragging = true; dragStartPoint = new Point(e.X, e.Y); } };
            loginForm.MouseMove += (s, e) => { if (isDragging) { Point p = loginForm.PointToScreen(new Point(e.X, e.Y)); loginForm.Location = new Point(p.X - dragStartPoint.X, p.Y - dragStartPoint.Y); } };
            loginForm.MouseUp += (s, e) => { if (e.Button == MouseButtons.Left) isDragging = false; };

            Label title = new Label
            {
                Text = "Ecstacy.Fun Login",
                ForeColor = Color.FromArgb(218, 112, 214),
                Font = new Font("Arial", 14, FontStyle.Bold),
                AutoSize = true,
                Location = new Point((loginForm.Width - TextRenderer.MeasureText("Ecstacy.Fun Login", new Font("Arial", 14, FontStyle.Bold)).Width) / 2, 30)
            };
            loginForm.Controls.Add(title);

            TextBox usernameBox = new TextBox
            {
                Location = new Point(90, 80),
                Size = new Size(120, 20),
                BackColor = Color.FromArgb(84, 84, 84),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                TextAlign = HorizontalAlignment.Center
            };
            usernameBox.Paint += (s, e) =>
            {
                ControlPaint.DrawBorder(e.Graphics, usernameBox.ClientRectangle, Color.FromArgb(218, 112, 214), ButtonBorderStyle.Solid);
            };
            loginForm.Controls.Add(usernameBox);

            TextBox passwordBox = new TextBox
            {
                Location = new Point(90, 110),
                Size = new Size(120, 20),
                BackColor = Color.FromArgb(84, 84, 84),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle,
                TextAlign = HorizontalAlignment.Center,
                UseSystemPasswordChar = true
            };
            passwordBox.Paint += (s, e) =>
            {
                ControlPaint.DrawBorder(e.Graphics, passwordBox.ClientRectangle, Color.FromArgb(218, 112, 214), ButtonBorderStyle.Solid);
            };
            loginForm.Controls.Add(passwordBox);

            Button loginButton = new Button
            {
                Text = "Login",
                ForeColor = Color.FromArgb(218, 112, 214),
                BackColor = Color.FromArgb(84, 84, 84),
                FlatStyle = FlatStyle.Flat,
                Size = new Size(120, 40),
                Location = new Point(90, 150)
            };
            loginButton.FlatAppearance.BorderColor = Color.FromArgb(218, 112, 214);
            loginButton.FlatAppearance.BorderSize = 2;
            loginButton.Click += async (s, e) =>
            {
                string username = usernameBox.Text;
                string password = passwordBox.Text;
                await CheckLoginAsync(username, password, loginForm);
            };
            loginForm.Controls.Add(loginButton);

            loginForm.ShowDialog();
        }

        private async Task CheckLoginAsync(string username, string password, Form loginForm)
        {
            try
            {
                using (HttpClient client = new HttpClient())
                {
                    var requestData = new
                    {
                        action = "login",
                        username = username,
                        password = password
                    };
                    var content = new StringContent(JsonConvert.SerializeObject(requestData), System.Text.Encoding.UTF8, "application/json");
                    HttpResponseMessage response = await client.PostAsync("https://ecstacy.dev/v1/api.php", content);
                    string result = await response.Content.ReadAsStringAsync();

                    var jsonResponse = JsonConvert.DeserializeObject<dynamic>(result);
                    if (jsonResponse.message == "Login erfolgreich")
                    {
                        subStatus = jsonResponse.data.sub_status.ToString();
                        loginForm.Close();
                        SetupForm();
                        Task.Run(async () => await CheckAndDownloadFileAsync()).Wait();
                        VerifyHWIDLock();
                    }
                    else
                    {
                        MessageBox.Show("Login failed. Check your credentials.", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Login error: {ex.Message}", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void SetupForm()
        {
            Size = new Size(300, 220);
            FormBorderStyle = FormBorderStyle.None;
            BackColor = Color.FromArgb(84, 84, 84);
            Paint += Form1_Paint;
            MouseDown += Form1_MouseDown;
            MouseMove += Form1_MouseMove;
            MouseUp += Form1_MouseUp;

            Label title = new Label
            {
                Text = "Ecstacy.Fun",
                ForeColor = Color.FromArgb(218, 112, 214),
                Font = new Font("Arial", 14, FontStyle.Bold),
                AutoSize = true,
                Location = new Point((Width - TextRenderer.MeasureText("Ecstacy.Fun", new Font("Arial", 14, FontStyle.Bold)).Width) / 2, 30)
            };
            Controls.Add(title);

            subStatusLabel = new Label
            {
                Text = $"Sub status: {subStatus}",
                ForeColor = Color.White,
                Font = new Font("Arial", 10),
                Location = new Point(20, 70),
                AutoSize = true
            };
            Controls.Add(subStatusLabel);

            Controls.Add(new Label
            {
                Text = "Status: Undetected",
                ForeColor = Color.White,
                Font = new Font("Arial", 10),
                Location = new Point(20, 100),
                AutoSize = true
            });

            injectButton = new Button
            {
                Text = "Inject",
                ForeColor = Color.FromArgb(218, 112, 214),
                BackColor = Color.FromArgb(84, 84, 84),
                FlatStyle = FlatStyle.Flat,
                Size = new Size(120, 40),
                Location = new Point(90, 130)
            };
            injectButton.FlatAppearance.BorderColor = Color.FromArgb(218, 112, 214);
            injectButton.FlatAppearance.BorderSize = 2;
            injectButton.MouseEnter += InjectButton_MouseEnter;
            injectButton.MouseLeave += InjectButton_MouseLeave;
            injectButton.Click += InjectButton_Click;

            ToolTip toolTip = new ToolTip();
            toolTip.SetToolTip(injectButton, subStatus == "inactive" ? "Purchase a subscription to Inject." : "");
            injectButton.Enabled = subStatus == "active";
            Controls.Add(injectButton);

            string username = Environment.UserName;
            exePath = $@"C:\Users\{username}\AppData\Local\Temp\neverwallahi.exe";
        }

        private void Form1_Paint(object sender, PaintEventArgs e)
        {
            using (Pen pen = new Pen(Color.FromArgb(218, 112, 214), 2))
            {
                e.Graphics.DrawRectangle(pen, 1, 1, Width - 2, Height - 2);
            }
        }

        private void Form1_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                isDragging = true;
                dragStartPoint = new Point(e.X, e.Y);
            }
        }

        private void Form1_MouseMove(object sender, MouseEventArgs e)
        {
            if (isDragging)
            {
                Point p = PointToScreen(new Point(e.X, e.Y));
                Location = new Point(p.X - dragStartPoint.X, p.Y - dragStartPoint.Y);
            }
        }

        private void Form1_MouseUp(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left) isDragging = false;
        }

        private void InjectButton_MouseEnter(object sender, EventArgs e)
        {
            if (injectButton.Enabled)
            {
                injectButton.BackColor = Color.FromArgb(255, 105, 180);
                injectButton.ForeColor = Color.Black;
            }
        }

        private void InjectButton_MouseLeave(object sender, EventArgs e)
        {
            injectButton.BackColor = Color.FromArgb(84, 84, 84);
            injectButton.ForeColor = Color.FromArgb(218, 112, 214);
        }

        private void InjectButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(exePath))
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = exePath,
                        UseShellExecute = true,
                        CreateNoWindow = true
                    };
                    Process.Start(psi);
                    MessageBox.Show("Launch CS:2", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    MessageBox.Show("Error: injector not found!", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Injection failed: {ex.Message}", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async Task CheckAndDownloadFileAsync()
        {
            try
            {
                if (!File.Exists(exePath))
                {
                    using (HttpClient client = new HttpClient())
                    {
                        client.DefaultRequestHeaders.Add("User-Agent", "CheatLoader/1.0");
                        byte[] fileBytes = await client.GetByteArrayAsync(DownloadUrl);
                        File.WriteAllBytes(exePath, fileBytes);
                        FileInfo fi = new FileInfo(exePath);
                        if (fi.Length < 1024)
                        {
                            throw new Exception("Error: 216");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Download failed: {ex.Message}");
            }
        }

        private string GetHWID()
        {
            try
            {
                using (ManagementObjectSearcher searcher = new ManagementObjectSearcher("SELECT * FROM Win32_DiskDrive"))
                {
                    foreach (ManagementObject obj in searcher.Get())
                    {
                        return obj["SerialNumber"]?.ToString().Trim() ?? "ERROR";
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"HWID retrieval failed: {ex.Message}");
            }
            return "ERROR";
        }

        private void VerifyHWIDLock()
        {
            string currentHWID = GetHWID();

            if (!File.Exists(hwidLockFile))
            {
                try
                {
                    File.WriteAllText(hwidLockFile, currentHWID);
                    MessageBox.Show("HWID locked to this machine.", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"HWID lock failed: {ex.Message}", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Application.Exit();
                }
            }
            else
            {
                string lockedHWID = File.ReadAllText(hwidLockFile).Trim();
                if (currentHWID != lockedHWID)
                {
                    MessageBox.Show("HWID error", "Ecstacy.Fun", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Application.Exit();
                }
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }
    }
}