using System;
using System.Globalization;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace mpv
{
    public partial class Form1 : Form
    {
        private IntPtr _libMpvDll;
        private IntPtr _mpvHandle;

        #region Win32 API

        // Win32 API functions for dynamically loading DLLs

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi, BestFitMapping = false)]
        internal static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi, BestFitMapping = false)]
        internal static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);

        #endregion Win32 API

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
        private delegate IntPtr MpvWaitEvent(IntPtr mpvHandle, double wait);
        private MpvWaitEvent _mpvWaitEvent;

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
        private delegate int MpvGetPropertyDouble(IntPtr mpvHandle, byte[] name, int format, ref double data);
        private MpvGetPropertyDouble _mpvGetPropertyDouble;

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
            _mpvWaitEvent = (MpvWaitEvent)GetDllType(typeof(MpvWaitEvent), "mpv_wait_event");
            _mpvCommand = (MpvCommand)GetDllType(typeof(MpvCommand), "mpv_command");
            _mpvSetOption = (MpvSetOption)GetDllType(typeof(MpvSetOption), "mpv_set_option");
            _mpvSetOptionString = (MpvSetOptionString)GetDllType(typeof(MpvSetOptionString), "mpv_set_option_string");
            _mpvGetPropertyString = (MpvGetPropertystring)GetDllType(typeof(MpvGetPropertystring), "mpv_get_property");
            _mpvGetPropertyDouble = (MpvGetPropertyDouble)GetDllType(typeof(MpvGetPropertyDouble), "mpv_get_property");
            _mpvSetProperty = (MpvSetProperty)GetDllType(typeof(MpvSetProperty), "mpv_set_property");
        }

        public void Pause()
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            int MPV_FORMAT_STRING = 1;
            var bytes = Encoding.UTF8.GetBytes("yes\0");
            _mpvSetProperty(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), MPV_FORMAT_STRING, ref bytes);
        }

        private void Play()
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            int MPV_FORMAT_STRING = 1;
            var bytes = Encoding.UTF8.GetBytes("no\0");
            _mpvSetProperty(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), MPV_FORMAT_STRING, ref bytes);
        }

        public bool IsPaused()
        {
            if (_mpvHandle == IntPtr.Zero)
                return true;

            int mpvFormatString = 1;
            IntPtr lpBuffer = Marshal.AllocHGlobal(10);
            _mpvGetPropertyString(_mpvHandle, Encoding.UTF8.GetBytes("pause\0"), mpvFormatString, ref lpBuffer);
            string str = Marshal.PtrToStringAnsi(lpBuffer);
            return str == "yes";
        }
      
        public double GetTime()
        {
            if (_mpvHandle == IntPtr.Zero)
                return 0;

            int mpvFormatDouble = 5;
            double time = 0;
            _mpvGetPropertyDouble(_mpvHandle, Encoding.UTF8.GetBytes("time-pos\0"), mpvFormatDouble, ref time);
            return time;
        }

        public void SetTime(double value)
        {
            if (_mpvHandle == IntPtr.Zero)
                return;

            string[] args = { "seek", value.ToString(CultureInfo.InvariantCulture), "absolute", null };
            _mpvCommand(_mpvHandle, args);
        }

        public double GetDuration()
        {
            if (_mpvHandle == IntPtr.Zero)
                return 0;

            int mpvFormatDouble = 5;
            double d = 0;
            _mpvGetPropertyDouble(_mpvHandle, Encoding.UTF8.GetBytes("duration\0"), mpvFormatDouble, ref d);
            return d;
        }

        public double GetWidth()
        {
            if (_mpvHandle == IntPtr.Zero)
                return 0;

            int mpvFormatDouble = 5;
            double d = 0;
            _mpvGetPropertyDouble(_mpvHandle, Encoding.UTF8.GetBytes("width\0"), mpvFormatDouble, ref d);
            return d;
        }

        public double GetHeight()
        {
            if (_mpvHandle == IntPtr.Zero)
                return 0;

            int mpvFormatDouble = 5;
            double d = 0;
            _mpvGetPropertyDouble(_mpvHandle, Encoding.UTF8.GetBytes("height\0"), mpvFormatDouble, ref d);
            return d;
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

            _mpvSetOptionString(_mpvHandle, Encoding.UTF8.GetBytes("vo\0"), Encoding.UTF8.GetBytes("direct3d_shaders\0")); 
            _mpvSetOptionString(_mpvHandle, Encoding.UTF8.GetBytes("keep-open\0"), Encoding.UTF8.GetBytes("always\0")); 

            int mpvFormatInt64 = 4;
            uint windowId = (uint)pictureBox1.Handle.ToInt64();
            _mpvSetOption(_mpvHandle, Encoding.UTF8.GetBytes("wid\0"), mpvFormatInt64, ref windowId);

            string[] args = { "loadfile", textBoxVideoSampleFileName.Text, null };
            _mpvCommand(_mpvHandle, args);

            textBox1.AppendText("start playing... " + Environment.NewLine);
            while (true)
            {
                label1.Text = String.Format("{0:00.000} / {1:00.000}    {2}x{3}", GetTime(), GetDuration(), GetWidth(), GetHeight());
                label1.Refresh();
                Application.DoEvents();

                if (_mpvHandle == IntPtr.Zero)
                    return;

                var eventId = _mpvWaitEvent(_mpvHandle, 0);
                var s2 = Convert.ToInt64(Marshal.PtrToStructure(eventId, typeof(int)));
                if (s2 != 0)
                    textBox1.AppendText("EventId: " + GetEventName(Convert.ToInt64(s2)) + Environment.NewLine);

                if (s2 == 1)
                    return; // SHUTDOWN
            }
        }

        private object GetEventName(long toInt64)
        {
            switch (toInt64)
            {
                case 1:
                    return "MPV_EVENT_SHUTDOWN";
                case 2:
                    return "MPV_EVENT_LOG_MESSAGE";
                case 3:
                    return "MPV_EVENT_GET_PROPERTY_REPLY";
                case 4:
                    return "MPV_EVENT_SET_PROPERTY_REPLY";
                case 5:
                    return "MPV_EVENT_COMMAND_REPLY";
                case 6:
                    return "MPV_EVENT_START_FILE";
                case 7:
                    return "MPV_EVENT_END_FILE";
                case 8:
                    return "MPV_EVENT_FILE_LOADED";
                case 9:
                    return "MPV_EVENT_TRACKS_CHANGED";
                case 10:
                    return "MPV_EVENT_TRACK_SWITCHED";
                case 11:
                    return "MPV_EVENT_IDLE";
                case 12:
                    return "MPV_EVENT_PAUSE";
                case 13:
                    return "MPV_EVENT_UNPAUSE";
                case 14:
                    return "MPV_EVENT_TICK";
                case 15:
                    return "MPV_EVENT_SCRIPT_INPUT_DISPATCH";
                case 16:
                    return "MPV_EVENT_CLIENT_MESSAGE";
                case 17:
                    return "MPV_EVENT_VIDEO_RECONFIG";
                case 18:
                    return "MPV_EVENT_AUDIO_RECONFIG";
                case 19:
                    return "MPV_EVENT_METADATA_UPDATE";
                case 20:
                    return "MPV_EVENT_SEEK";
                case 21:
                    return "MPV_EVENT_PLAYBACK_RESTART";
                case 22:
                    return "MPV_EVENT_PROPERTY_CHANGE";
                case 23:
                    return "MPV_EVENT_CHAPTER_CHANGE";
                case 24:
                    return "MPV_EVENT_QUEUE_OVERFLOW";
            }
            return "Uknwown " + toInt64;
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

        private void button1_Click(object sender, EventArgs e)
        {
            SetTime(1);
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
