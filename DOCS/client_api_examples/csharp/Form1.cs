using System;
using System.Globalization;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace mpv
{
    public partial class Form1 : Form
    {
        const int MpvFormatString = 1;
        private IntPtr _libMpvDll;
        private IntPtr _mpvHandle;

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi, BestFitMapping = false)]
        internal static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi, BestFitMapping = false)]
        internal static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate IntPtr MpvCreate();
        private MpvCreate _mpvCreate;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvInitialize(IntPtr mpvHandle);
        private MpvInitialize _mpvInitialize;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvCommand(IntPtr mpvHandle, [MarshalAs(UnmanagedType.LPArray)] string[] args);
        private MpvCommand _mpvCommand;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvTerminateDestroy(IntPtr mpvHandle);
        private MpvTerminateDestroy _mpvTerminateDestroy;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvSetOption(IntPtr mpvHandle, byte[] name, int format, ref uint data);
        private MpvSetOption _mpvSetOption;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvSetOptionString(IntPtr mpvHandle, byte[] name, byte[] value);
        private MpvSetOptionString _mpvSetOptionString;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvGetPropertystring(IntPtr mpvHandle, byte[] name, int format, ref IntPtr data);
        private MpvGetPropertystring _mpvGetPropertyString;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int MpvSetProperty(IntPtr mpvHandle, byte[] name, int format, ref byte[] data);
        private MpvSetProperty _mpvSetProperty;

        public Form1()
        {
            InitializeComponent();
        }

        private object GetDllType(Type type, string name)
        {
            IntPtr address = GetProcAddress(_libMpvDll, name);
            if (address != IntPtr.Zero)
                return Marshal.GetDelegateForFunctionPointer(address, type);
            return null;
        }

        private void LoadMpvDynamic()
        {
            _libMpvDll = LoadLibrary("mpv-1.dll");
            _mpvCreate = (MpvCreate)GetDllType(typeof(MpvCreate), "mpv_create");
            _mpvInitialize = (MpvInitialize)GetDllType(typeof(MpvInitialize), "mpv_initialize");
            _mpvTerminateDestroy = (MpvTerminateDestroy)GetDllType(typeof(MpvTerminateDestroy), "mpv_terminate_destroy");
            _mpvCommand = (MpvCommand)GetDllType(typeof(MpvCommand), "mpv_command");
            _mpvSetOption = (MpvSetOption)GetDllType(typeof(MpvSetOption), "mpv_set_option");
            _mpvSetOptionString = (MpvSetOptionString)GetDllType(typeof(MpvSetOptionString), "mpv_set_option_string");
            _mpvGetPropertyString = (MpvGetPropertystring)GetDllType(typeof(MpvGetPropertystring), "mpv_get_property");
            _mpvSetProperty = (MpvSetProperty)GetDllType(typeof(MpvSetProperty), "mpv_set_property");
        }

        public void Pause()
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            var bytes = Encoding.UTF8.GetBytes("yes\0");
            _mpvSetProperty(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), MpvFormatString, ref bytes);
        }

        private void Play()
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            var bytes = Encoding.UTF8.GetBytes("no\0");
            _mpvSetProperty(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), MpvFormatString, ref bytes);
        }

        public bool IsPaused()
        {
            if (_mpvHandle == IntPtr.Zero)
                return true;

            IntPtr lpBuffer = Marshal.AllocHGlobal(10);
            _mpvGetPropertyString(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), MpvFormatString, ref lpBuffer);
            return Marshal.PtrToStringAnsi(lpBuffer) == "yes";
        }

        public void SetTime(double value)
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            _mpvCommand(_mpvHandle, new[] { "seek", value.ToString(CultureInfo.InvariantCulture), "absolute", null });
        }

        private void buttonPlay_Click(object sender, EventArgs e)
        {
            if (_mpvHandle != IntPtr.Zero)
                _mpvTerminateDestroy(_mpvHandle);

            LoadMpvDynamic();
            if (_libMpvDll == IntPtr.Zero)
                return;

            _mpvHandle = _mpvCreate.Invoke();
            if (_mpvHandle == IntPtr.Zero)
                return;

            _mpvInitialize.Invoke(_mpvHandle);
            _mpvSetOptionString(_mpvHandle, Encoding.UTF8.GetBytes("vo\0"), Encoding.UTF8.GetBytes("direct3d\0"));
            _mpvSetOptionString(_mpvHandle, Encoding.UTF8.GetBytes("keep-open\0"), Encoding.UTF8.GetBytes("always\0"));
            int mpvFormatInt64 = 4;
            uint windowId = (uint)pictureBox1.Handle.ToInt64();
            _mpvSetOption(_mpvHandle, Encoding.UTF8.GetBytes("wid\0"), mpvFormatInt64, ref windowId);
            _mpvCommand(_mpvHandle, new[] { "loadfile", textBoxVideoSampleFileName.Text, null });
        }

        private void buttonPlayPause_Click(object sender, EventArgs e)
        {
            if (IsPaused())
                Play();
            else
                Pause();
        }

        private void buttonStop_Click(object sender, EventArgs e)
        {
            Pause();
            SetTime(0);
        }

        private void buttonLoadVideo_Click(object sender, EventArgs e)
        {
            openFileDialog1.FileName = String.Empty;
            if (openFileDialog1.ShowDialog(this) == DialogResult.OK)
            {
                textBoxVideoSampleFileName.Text = openFileDialog1.FileName;
                buttonPlay_Click(null, null);
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (_mpvHandle != IntPtr.Zero)
                _mpvTerminateDestroy(_mpvHandle);
        }
    }
}
