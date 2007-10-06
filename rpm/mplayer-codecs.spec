%define _codecsdir %{_libdir}/codecs

# don't try to strip or compress anything
%define __spec_install_post %{nil}
%define debug_package %{nil}

%define i386_ver 20061022
%define ppc_ver 20061022
%define alpha_ver 20061028
%define x86_64_ver 20061203

%define ver %{expand:%{%{_target_cpu}_ver}}

Summary: MPlayer essential binary codecs package
Name: mplayer-codecs
Version: %{ver}
Release: 1
URL: http://www.mplayerhq.hu/MPlayer/releases/codecs/
Group: Applications/Multimedia
Source0: http://www.mplayerhq.hu/MPlayer/releases/codecs/all-%{i386_ver}.tar.bz2
Source1: http://www.mplayerhq.hu/MPlayer/releases/codecs/all-alpha-%{alpha_ver}.tar.bz2
Source2: http://www.mplayerhq.hu/MPlayer/releases/codecs/all-ppc-%{ppc_ver}.tar.bz2
Source3: http://www.mplayerhq.hu/MPlayer/releases/codecs/essential-amd64-%{x86_64_ver}.tar.bz2
License: Unknown
ExclusiveArch: i386 ppc alpha x86_64
%ifarch i386
Provides: w32codec
%endif
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(id -u -n)

%description
All-in-one essential end-user package. Contains binary codecs that have
no native open-source decoder currently.

%package extra
Group: Applications/Multimedia
Summary: MPlayer optional binary codecs package

%description extra
MPlayer optional codecs package. Contains additional binary codecs
supported by MPlayer.

%prep
%ifarch i386
%setup -q -c %{name}-%{version}
%endif
%ifarch alpha
%setup -q -c %{name}-%{version} -T -a 1
%endif
%ifarch ppc
%setup -q -c %{name}-%{version} -T -a 2
%endif
%ifarch x86_64
%setup -q -c %{name}-%{version} -T -a 3
%endif

%build
# nothing to build

%install
rm -rf $RPM_BUILD_ROOT

install -d $RPM_BUILD_ROOT%{_codecsdir}
%ifarch i386
install -pm644 all-%{version}/* $RPM_BUILD_ROOT%{_codecsdir}/
%endif
%ifarch alpha
install -pm644 all-alpha-%{alpha_ver}/* $RPM_BUILD_ROOT%{_codecsdir}/
%endif
%ifarch ppc
install -pm644 all-ppc-%{ppc_ver}/* $RPM_BUILD_ROOT%{_codecsdir}/
%endif
%ifarch x86_64
install -pm644 essential-amd64-%{x86_64_ver}/* $RPM_BUILD_ROOT%{_codecsdir}/
%endif
rm -f $RPM_BUILD_ROOT%{_codecsdir}/README

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0644,root,root,0755)
%ifarch i386
%doc all-%{i386_ver}/README
%{_codecsdir}/acelpdec.ax
%{_codecsdir}/alf2cd.acm
%{_codecsdir}/aslcodec_dshow.dll
%{_codecsdir}/atrac3.acm
%{_codecsdir}/atrc.so.6.0
%{_codecsdir}/AvidQTAVUICodec.qtx
%{_codecsdir}/BeHereiVideo.qtx
%{_codecsdir}/CLRVIDDC.DLL
%{_codecsdir}/clrviddd.dll
%{_codecsdir}/cook.so
%{_codecsdir}/CtWbJpg.DLL
%{_codecsdir}/DECVW_32.DLL
%{_codecsdir}/drvc.so
%{_codecsdir}/dspr.so.6.0
%{_codecsdir}/iac25_32.ax
%{_codecsdir}/icmw_32.dll
%{_codecsdir}/imc32.acm
%{_codecsdir}/ir41_32.dll
%{_codecsdir}/ir50_32.dll
%{_codecsdir}/ivvideo.dll
%{_codecsdir}/jp2avi.dll
%{_codecsdir}/LCMW2.dll
%{_codecsdir}/LCODCCMW2E.dll
%{_codecsdir}/lhacm.acm
%{_codecsdir}/lsvxdec.dll
%{_codecsdir}/m3jp2k32.dll
%{_codecsdir}/mi-sc4.acm
%{_codecsdir}/msh261.drv
%{_codecsdir}/msms001.vwp
%{_codecsdir}/msscds32.ax
%{_codecsdir}/nsrt2432.acm
%{_codecsdir}/qpeg32.dll
%{_codecsdir}/qtmlClient.dll
%{_codecsdir}/QuickTimeEssentials.qtx
%{_codecsdir}/QuickTimeInternetExtras.qtx
%{_codecsdir}/QuickTime.qts
%{_codecsdir}/rt32dcmp.dll
%{_codecsdir}/sipr.so.6.0
%{_codecsdir}/tm20dec.ax
%{_codecsdir}/tokf.so.6.0
%{_codecsdir}/tokr.so.6.0
%{_codecsdir}/tsd32.dll
%{_codecsdir}/tssoft32.acm
%{_codecsdir}/tvqdec.dll
%{_codecsdir}/VDODEC32.dll
%{_codecsdir}/vdowave.drv
%{_codecsdir}/vid_3ivX.xa
%{_codecsdir}/ViVD2.dll
%{_codecsdir}/vivog723.acm
%{_codecsdir}/vmnc.dll
%{_codecsdir}/voxmsdec.ax
%{_codecsdir}/vp4vfw.dll
%{_codecsdir}/vp5vfw.dll
%{_codecsdir}/vp6vfw.dll
%{_codecsdir}/vp7vfw.dll
%{_codecsdir}/vssh264core.dll
%{_codecsdir}/vssh264dec.dll
%{_codecsdir}/vssh264.dll
%{_codecsdir}/vsshdsd.dll
%{_codecsdir}/vsslight.dll
%{_codecsdir}/vsswlt.dll
%{_codecsdir}/wma9dmod.dll
%{_codecsdir}/wmadmod.dll
%{_codecsdir}/wmsdmod.dll
%{_codecsdir}/wmspdmod.dll
%{_codecsdir}/wmv9dmod.dll
%{_codecsdir}/wmvadvd.dll
%{_codecsdir}/wmvdmod.dll
%{_codecsdir}/wnvwinx.dll
%{_codecsdir}/wvc1dmod.dll
%endif
%ifarch alpha
%doc all-alpha-%{alpha_ver}/README
%endif
%ifarch alpha ppc
%{_codecsdir}/28_8.so.6.0
%{_codecsdir}/atrc.so.6.0
%{_codecsdir}/cook.so.6.0
%{_codecsdir}/ddnt.so.6.0
%{_codecsdir}/dnet.so.6.0
%{_codecsdir}/drv2.so.6.0
%{_codecsdir}/dspr.so.6.0
%{_codecsdir}/sipr.so.6.0
%{_codecsdir}/tokr.so.6.0
%endif
%ifarch ppc
%doc all-ppc-%{ppc_ver}/README
%{_codecsdir}/vid_iv41.xa
%{_codecsdir}/vid_iv50.xa
%endif
%ifarch x86_64
%doc essential-amd64-%{x86_64_ver}/README
%{_codecsdir}/atrc.so
%{_codecsdir}/cook.so
%{_codecsdir}/drvc.so
%{_codecsdir}/sipr.so
%endif

%files extra
%ifarch i386
%defattr(0644,root,root,0755)
%{_codecsdir}/LCodcCMP.dll
%{_codecsdir}/aslcodec_vfw.dll
%{_codecsdir}/asusasv2.dll
%{_codecsdir}/asusasvd.dll
%{_codecsdir}/ativcr2.dll
%{_codecsdir}/avimszh.dll
%{_codecsdir}/avizlib.dll
%{_codecsdir}/cook.so.6.0
%{_codecsdir}/ctadp32.acm
%{_codecsdir}/ddnt.so.6.0
%{_codecsdir}/divx.dll
%{_codecsdir}/divx_c32.ax
%{_codecsdir}/divxa32.acm
%{_codecsdir}/divxc32.dll
%{_codecsdir}/divxdec.ax
%{_codecsdir}/dnet.so.6.0
%{_codecsdir}/drv2.so.6.0
%{_codecsdir}/drv3.so.6.0
%{_codecsdir}/drv4.so.6.0
%{_codecsdir}/huffyuv.dll
%{_codecsdir}/i263_32.drv
%{_codecsdir}/iccvid.dll
%{_codecsdir}/imaadp32.acm
%{_codecsdir}/ir32_32.dll
%{_codecsdir}/l3codeca.acm
%{_codecsdir}/l3codecx.ax
%{_codecsdir}/m3jpeg32.dll
%{_codecsdir}/m3jpegdec.ax
%{_codecsdir}/mcdvd_32.dll
%{_codecsdir}/mcmjpg32.dll
%{_codecsdir}/mpg4c32.dll
%{_codecsdir}/mpg4ds32.ax
%{_codecsdir}/msadp32.acm
%{_codecsdir}/msg711.acm
%{_codecsdir}/msgsm32.acm
%{_codecsdir}/msnaudio.acm
%{_codecsdir}/msrle32.dll
%{_codecsdir}/msvidc32.dll
%{_codecsdir}/mvoiced.vwp
%{_codecsdir}/pclepim1.dll
%{_codecsdir}/qdv.dll
%{_codecsdir}/scg726.acm
%{_codecsdir}/sp5x_32.dll
%{_codecsdir}/tsccvid.dll
%{_codecsdir}/ubv263d+.ax
%{_codecsdir}/ubvmp4d.dll
%{_codecsdir}/ultimo.dll
%{_codecsdir}/vgpix32d.dll
%{_codecsdir}/vid_cvid.xa
%{_codecsdir}/vid_cyuv.xa
%{_codecsdir}/vid_h261.xa
%{_codecsdir}/vid_h263.xa
%{_codecsdir}/vid_iv32.xa
%{_codecsdir}/vid_iv41.xa
%{_codecsdir}/vid_iv50.xa
%{_codecsdir}/vp31vfw.dll
%{_codecsdir}/wmv8ds32.ax
%{_codecsdir}/wmvds32.ax
%{_codecsdir}/wnvplay1.dll
%{_codecsdir}/zmbv.dll
%endif
%ifarch alpha ppc
%{_codecsdir}/14_4.so.6.0
%{_codecsdir}/drv3.so.6.0
%endif
%ifarch ppc
%{_codecsdir}/vid_cvid.xa
%{_codecsdir}/vid_iv32.xa
%endif

%changelog
* Sat Oct 06 2007 Dominik Mierzejewski <rpm at greysector.net>
- specfile cleanups
- added backwards compatibility Provides:
- dropped old history
