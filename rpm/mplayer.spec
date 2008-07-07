%define         date %(date --iso)
%define         svnbuild %(date +%Y%m%d)
%define         codecsdir %{_libdir}/codecs

Name:           mplayer
Version:        1.0
Release:        0.%{svnbuild}svn%{?dist}
Summary:        Movie player playing most video formats and DVDs

Group:          Applications/Multimedia
License:        GPL
URL:            http://www.mplayerhq.hu/
Source0:        http://www.mplayerhq.hu/MPlayer/releases/mplayer-export-snapshot.tar.bz2
Source1:        http://www.mplayerhq.hu/MPlayer/skins/Blue-1.7.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  SDL-devel
BuildRequires:  aalib-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  cdparanoia-devel
BuildRequires:  desktop-file-utils
BuildRequires:  em8300-devel
BuildRequires:  enca-devel
BuildRequires:  faac-devel
BuildRequires:  faad2-devel
BuildRequires:  fontconfig-devel
BuildRequires:  freetype-devel >= 2.0.9
BuildRequires:  fribidi-devel
BuildRequires:  giflib-devel
BuildRequires:  gtk2-devel
BuildRequires:  ladspa-devel
BuildRequires:  lame-devel
BuildRequires:  libGL-devel
BuildRequires:  libXinerama-devel
BuildRequires:  libXv-devel
BuildRequires:  libXvMC-devel
BuildRequires:  libXxf86dga-devel
BuildRequires:  libXxf86vm-devel
BuildRequires:  libcaca-devel
BuildRequires:  libdca-devel
BuildRequires:  libdv-devel
BuildRequires:  libdvdnav-devel
BuildRequires:  libjpeg-devel
BuildRequires:  libmpcdec-devel
BuildRequires:  libtheora-devel
BuildRequires:  libvorbis-devel
BuildRequires:  lirc-devel
BuildRequires:  live-devel
BuildRequires:  lzo-devel >= 2
BuildRequires:  speex-devel >= 1.1
BuildRequires:  twolame-devel
BuildRequires:  x264-devel
BuildRequires:  xvidcore-devel >= 0.9.2
%{?_with_arts:BuildRequires: arts-devel}
%{?_with_amr:BuildRequires: amrnb-devel amrwb-devel}
%{?_with_directfb:BuildRequires: directfb-devel}
%{?_with_esound:BuildRequires: esound-devel}
%{?_with_jack:BuildRequires: jack-audio-connection-kit-devel}
%{?_with_libmad:BuildRequires:  libmad-devel}
%{?_with_openal:BuildRequires: openal-devel}
%{?_with_samba:BuildRequires: samba-common}
%{?_with_svgalib:BuildRequires: svgalib-devel}
%{?_with_xmms:BuildRequires: xmms-devel}
# for XML docs, SVN only
BuildRequires:  docbook-dtds
BuildRequires:  docbook-style-xsl
BuildRequires:  libxml2
BuildRequires:  libxslt

%description
MPlayer is a movie player that plays most MPEG, VOB, AVI, OGG/OGM,
VIVO, ASF/WMA/WMV, QT/MOV/MP4, FLI, RM, NuppelVideo, yuv4mpeg, FILM,
RoQ, and PVA files. You can also use it to watch VCDs, SVCDs, DVDs,
3ivx, RealMedia, and DivX movies.
It supports a wide range of output drivers including X11, XVideo, DGA,
OpenGL, SVGAlib, fbdev, AAlib, DirectFB etc. There are also nice
antialiased shaded subtitles and OSD.
Non-default rpmbuild options:
--with samba:   Enable Samba (smb://) support
--with xmms:    Enable XMMS input plugin support
--with amr:     Enable AMR support
--with libmad:  Enable libmad support
--with openal:  Enable OpenAL support
--with jack:    Enable JACK support
--with arts:    Enable aRts support
--with esound:  Enable EsounD support
--with directfb:Enable DirectFB support
--with svgalib: Enable SVGAlib support

%package        gui
Summary:        GUI for MPlayer
Group:          Applications/Multimedia
Requires:       mplayer = %{version}-%{release}

%description    gui
This package contains a GUI for MPlayer and a default skin for it.

%package     -n mencoder
Summary:        MPlayer movie encoder
Group:          Applications/Multimedia
Requires:       mplayer = %{version}-%{release}

%description -n mencoder
This package contains the MPlayer movie encoder. 

%package        doc
Summary:        MPlayer documentation in various languages
Group:          Documentation

%description    doc
MPlayer documentation in various languages.


%prep
%setup -q -n mplayer-export-%{date}

doconv() {
    iconv -f $1 -t $2 -o DOCS/man/$3/mplayer.1.utf8 DOCS/man/$3/mplayer.1 && \
    mv DOCS/man/$3/mplayer.1.utf8 DOCS/man/$3/mplayer.1
}
for lang in de es fr it ; do doconv iso-8859-1 utf-8 $lang ; done
for lang in hu pl ; do doconv iso-8859-2 utf-8 $lang ; done
for lang in ru ; do doconv koi8-r utf-8 $lang ; done

mv DOCS/man/zh DOCS/man/zh_CN

%build
./configure \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir}/mplayer \
    --mandir=%{_mandir} \
    --confdir=%{_sysconfdir}/mplayer \
    --libdir=%{_libdir} \
    --codecsdir=%{codecsdir} \
    \
    --disable-encoder=FAAC \
    --disable-encoder=MP3LAME \
    --disable-encoder=X264 \
    \
    --enable-gui \
    --enable-largefiles \
    --disable-termcap \
    --disable-bitmap-font \
    --enable-lirc \
    --enable-joystick \
    %{!?_with_samba:--disable-smb} \
    --disable-dvdread-internal \
    --disable-libdvdcss-internal \
    --enable-menu \
    \
    --disable-faad-internal \
    --disable-tremor-internal \
    %{!?_with_amr:--disable-libamr_nb --disable-libamr_wb} \
    %{!?_with_libmad:--disable-mad} \
    %{?_with_xmms:--enable-xmms} \
    \
    --disable-svga \
    --enable-xvmc \
    --%{?_with_directfb:enable}%{!?_with_directfb:disable}-directfb \
    %{!?_with_svgalib:--disable-svga} \
    \
    %{!?_with_arts:--disable-arts} \
    %{!?_with_esound:--disable-esd} \
    %{!?_with_jack:--disable-jack} \
    %{!?_with_openal:--disable-openal} \
    \
    --language=all \
    \
    %{?_with_xmms:--with-xmmslibdir=%{_libdir}} \
    --with-xvmclib=XvMCW

%{__make}

mv -f mplayer gmplayer
%{__make} distclean

./configure \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir}/mplayer \
    --mandir=%{_mandir} \
    --confdir=%{_sysconfdir}/mplayer \
    --libdir=%{_libdir} \
    --codecsdir=%{codecsdir} \
    \
    --disable-encoder=FAAC \
    --disable-encoder=MP3LAME \
    --disable-encoder=X264 \
    \
    --enable-largefiles \
    --disable-termcap \
    --disable-bitmap-font \
    --enable-lirc \
    --enable-joystick \
    %{!?_with_samba:--disable-smb} \
    --disable-dvdread-internal \
    --disable-libdvdcss-internal \
    --enable-menu \
    \
    --disable-faad-internal \
    --disable-tremor-internal \
    %{!?_with_amr:--disable-libamr_nb --disable-libamr_wb} \
    %{!?_with_libmad:--disable-mad} \
    %{?_with_xmms:--enable-xmms} \
    \
    --disable-svga \
    --enable-xvmc \
    --%{?_with_directfb:enable}%{!?_with_directfb:disable}-directfb \
    %{!?_with_svgalib:--disable-svga} \
    \
    %{!?_with_arts:--disable-arts} \
    %{!?_with_esound:--disable-esd} \
    %{!?_with_jack:--disable-jack} \
    %{!?_with_openal:--disable-openal} \
    \
    --language=all \
    \
    %{?_with_xmms:--with-xmmslibdir=%{_libdir}} \
    --with-xvmclib=XvMCW

%{__make}

# build HTML documentation from XML files 
pushd DOCS/xml
%{__make} html-chunked
popd

%install
rm -rf $RPM_BUILD_ROOT doc

make install DESTDIR=$RPM_BUILD_ROOT STRIPBINARIES=no
install -pm 755 TOOLS/midentify.sh $RPM_BUILD_ROOT%{_bindir}/

# Clean up documentation
mkdir doc
cp -pR DOCS/* doc/
rm -r doc/man doc/xml doc/README
mv doc/HTML/* doc/
rm -rf doc/HTML

# Default config files
install -Dpm 644 etc/example.conf \
    $RPM_BUILD_ROOT%{_sysconfdir}/mplayer/mplayer.conf
# use Nimbus Sans L font for OSD (via fontconfig)
echo "fontconfig=yes" >>$RPM_BUILD_ROOT%{_sysconfdir}/mplayer/mplayer.conf
echo "font=\"Sans\"" >>$RPM_BUILD_ROOT%{_sysconfdir}/mplayer/mplayer.conf

install -pm 644 etc/{input,menu}.conf $RPM_BUILD_ROOT%{_sysconfdir}/mplayer/

# GUI mplayer
install -pm 755 g%{name} $RPM_BUILD_ROOT%{_bindir}/

# Default skin
install -dm 755 $RPM_BUILD_ROOT%{_datadir}/mplayer/skins
tar xjC $RPM_BUILD_ROOT%{_datadir}/mplayer/skins --exclude=.svn -f %{SOURCE1}
ln -s Blue $RPM_BUILD_ROOT%{_datadir}/mplayer/skins/default

# Icons
install -dm 755 $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/32x32/apps
install -pm 644 etc/mplayer.xpm \
    $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/32x32/apps

# Desktop file
desktop-file-install \
        --dir $RPM_BUILD_ROOT%{_datadir}/applications \
        etc/%{name}.desktop

# Codec dir
install -dm 755 $RPM_BUILD_ROOT%{codecsdir}


%post gui
gtk-update-icon-cache -qf %{_datadir}/icons/hicolor &>/dev/null || :
update-desktop-database &>/dev/null || :


%postun gui
gtk-update-icon-cache -qf %{_datadir}/icons/hicolor &>/dev/null || :
update-desktop-database &>/dev/null || :


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-, root, root, -)
%doc AUTHORS Changelog LICENSE README
%dir %{_sysconfdir}/mplayer
%config(noreplace) %{_sysconfdir}/mplayer/mplayer.conf
%config(noreplace) %{_sysconfdir}/mplayer/input.conf
%config(noreplace) %{_sysconfdir}/mplayer/menu.conf
%{_bindir}/midentify.sh
%{_bindir}/mplayer
%dir %{codecsdir}/
%dir %{_datadir}/mplayer/
%{_mandir}/man1/mplayer.1*
%lang(cs) %{_mandir}/cs/man1/mplayer.1*
%lang(de) %{_mandir}/de/man1/mplayer.1*
%lang(es) %{_mandir}/es/man1/mplayer.1*
%lang(fr) %{_mandir}/fr/man1/mplayer.1*
%lang(hu) %{_mandir}/hu/man1/mplayer.1*
%lang(it) %{_mandir}/it/man1/mplayer.1*
%lang(pl) %{_mandir}/pl/man1/mplayer.1*
%lang(ru) %{_mandir}/ru/man1/mplayer.1*
%lang(zh_CN) %{_mandir}/zh_CN/man1/mplayer.1*

%files gui
%defattr(-, root, root, -)
%{_bindir}/gmplayer
%{_datadir}/applications/*mplayer.desktop
%{_datadir}/icons/hicolor/32x32/apps/mplayer.xpm
%{_datadir}/mplayer/skins/

%files -n mencoder
%defattr(-, root, root, -)
%{_bindir}/mencoder
%{_mandir}/man1/mencoder.1*
%lang(cs) %{_mandir}/cs/man1/mencoder.1*
%lang(de) %{_mandir}/de/man1/mencoder.1*
%lang(es) %{_mandir}/es/man1/mencoder.1*
%lang(fr) %{_mandir}/fr/man1/mencoder.1*
%lang(hu) %{_mandir}/hu/man1/mencoder.1*
%lang(it) %{_mandir}/it/man1/mencoder.1*
%lang(pl) %{_mandir}/pl/man1/mencoder.1*
%lang(ru) %{_mandir}/ru/man1/mencoder.1*
%lang(zh_CN) %{_mandir}/zh_CN/man1/mencoder.1*

%files doc
%defattr(-, root, root, -)
%doc doc/en/ doc/tech/
%lang(cs) %doc doc/cs/
%lang(de) %doc doc/de/
%lang(es) %doc doc/es/
%lang(fr) %doc doc/fr/
%lang(hu) %doc doc/hu/
%lang(pl) %doc doc/pl/
%lang(ru) %doc doc/ru/
%lang(zh_CN) %doc doc/zh_CN/


%changelog
* Sat Oct 06 2007 Dominik Mierzejewski <rpm at greysector.net>
- adapted livna specfile for inclusion in SVN
