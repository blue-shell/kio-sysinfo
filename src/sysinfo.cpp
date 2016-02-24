//////////////////////////////////////////////////////////////////////////
// sysinfo.cpp                                                          //
//                                                                      //
// Copyright (C)  2005, 2008, 2009  Lukas Tinkl <lukas.tinkl@suse.cz>   //
//                                              <ltinkl@redhat.com>     //
//           (C)  2008  Dirk Mueller <dmueller@suse.de>                 //
//                                                                      //
// This program is free software; you can redistribute it and/or        //
// modify it under the terms of the GNU General Public License          //
// as published by the Free Software Foundation; either version 2       //
// of the License, or (at your option) any later version.               //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program; if not, write to the Free Software          //
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA        //
// 02110-1301, USA.                                                     //
//////////////////////////////////////////////////////////////////////////

#include "sysinfo.h"

#include <config-kiosysinfo.h>

#ifdef HAVE_HD
#include <hd.h>
#endif

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QtGui/QX11Info>
#include <QDesktopWidget>

#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <string.h>
#include <sys/utsname.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kdeversion.h>
#include <kuser.h>
#include <kglobalsettings.h>
#include <kmountpoint.h>
#include <kcomponentdata.h>
#include <KDesktopFile>
#include <KConfigGroup>

#include <solid/networking.h>
#include <solid/device.h>
#include <solid/storagedrive.h>
#include <solid/storageaccess.h>
#include <solid/storagevolume.h>
#include <solid/block.h>
#include <solid/battery.h>
#include <solid/acadapter.h>
#include <solid/opticaldisc.h>

#define SOLID_MEDIALIST_PREDICATE \
    "[[ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]" \
    " OR " \
    "[ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"

#define SOLID_BATTERY_AC_PREDICATE                      \
    "[Battery.type == 'PrimaryBattery' OR IS AcAdapter]"

#define BR "<br>"

static QString formattedUnit( quint64 value, int post=1 )
{
    if (value >= (1024 * 1024))
        if (value >= (1024 * 1024 * 1024))
            return i18n("%1 GiB", KGlobal::locale()->formatNumber(value / (1024 * 1024 * 1024.0),
                        post));
        else
            return i18n("%1 MiB", KGlobal::locale()->formatNumber(value / (1024 * 1024.0), post));
    else
        return i18n("%1 KiB", KGlobal::locale()->formatNumber(value / 1024.0, post));
}

static QString htmlQuote(const QString& _s)
{
    QString s(_s);
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
}

static QString readFromFile( const QString & filename, const QString & info = QString(),
                             const char * sep = 0, bool returnlast = false )
{
    kDebug() << "Reading " << info << " from " << filename;

    QFile file( filename );

    if ( !file.exists() || !file.open( QIODevice::ReadOnly ) )
        return QString::null;

    QTextStream stream( &file );
    QString line, result;

    do
    {
        line = stream.readLine();
        if ( !line.isEmpty() )
        {
            if ( !sep )
                result = line;
            else if ( line.startsWith( info ) )
                result = line.section( sep, 1, 1 );

            if (!result.isEmpty() && !returnlast)
                return result;
        }
    } while (!line.isNull());

    return result;
}

static QString netStatus()
{
    switch (Solid::Networking::status())
    {
    case Solid::Networking::Disconnecting:
        return i18n( "Network is <strong>shutting down</strong>" );
    case Solid::Networking::Connecting:
        return i18n( "<strong>Establishing</strong> connection to the network" );
    case Solid::Networking::Connected:
        return i18n( "You are <strong>online</strong>" );
    case Solid::Networking::Unconnected:
        return i18n( "You are <strong>offline</strong>" );
    case Solid::Networking::Unknown:
    default:
        return QString();
    }
}

kio_sysinfoProtocol::kio_sysinfoProtocol( const QByteArray & pool_socket, const QByteArray & app_socket )
    : SlaveBase( "kio_sysinfo", pool_socket, app_socket )
{
    m_predicate = Solid::Predicate::fromString(SOLID_MEDIALIST_PREDICATE);
}

kio_sysinfoProtocol::~kio_sysinfoProtocol()
{
}

void kio_sysinfoProtocol::get( const KUrl & /*url*/ )
{
 //   mimeType( "application/x-sysinfo" );
    mimeType( "text/html" );

    // CPU info
    infoMessage( i18n( "Looking for CPU information..." ) );
    cpuInfo();

    // header
    QString location = KStandardDirs::locate( "data", "sysinfo/about/my-computer.html" );
    QFile f( location );
    f.open( QIODevice::ReadOnly );
    QTextStream t( &f );
    QString content = t.readAll();
    content = content.arg( i18n( "My Computer" ),
                           htmlQuote("file:" + KStandardDirs::locate( "data", "sysinfo/about/shared.css" )),
                           htmlQuote("file:" + KStandardDirs::locate( "data", "sysinfo/about/style.css" )),
                           i18n( "My Computer"),
                           i18n( "Folders, Harddisks, Removable Devices, System Information and more..." ));

    QString sysInfo = "<div id=\"column2\">"; // table with 2 cols
    QString dummy;

    osInfo();
    sysInfo += "<h2 id=\"sysinfo\">" +i18n( "OS Information" ) + "</h2>";
    sysInfo += "<table>";
    sysInfo += "<tr><td>" + i18n( "OS:" ) +  "</td><td>" + htmlQuote(m_info[OS_SYSTEM]) + "</td></tr>";
    sysInfo += "<tr><td>" + i18n( "Kernel:" ) + "</td><td>" + htmlQuote(m_info[OS_SYSNAME]) + " " +
               htmlQuote(m_info[OS_RELEASE]) + " " + htmlQuote(m_info[OS_MACHINE]) + "</td></tr>";
//     sysInfo += "<tr><td>" + i18n( "Current user:" ) + "</td><td>" + htmlQuote(m_info[OS_USER]) + "@"
//                + htmlQuote(m_info[OS_HOSTNAME]) + "</td></tr>";
    //TODO: Don't hardcode the filename here
    const QString filePath("/usr/share/xsessions/plasma.desktop");
    KDesktopFile desktopFile(filePath);
    QString plasmaVersion = desktopFile.desktopGroup().readEntry("X-KDE-PluginInfo-Version", KDE::versionString());
    sysInfo += "<tr><td>" + i18n( "Plasma:" ) + "</td><td>" + plasmaVersion + "</td></tr>";
    if ( kdeInfo() )
    {
        if (!m_info[KF5_VERSION].isNull())
            sysInfo += "<tr><td>" + i18n( "KDE Frameworks:" ) + "</td><td>" + htmlQuote(m_info[KF5_VERSION]) + "</td></tr>";
        if (!m_info[KDEAPPS_VERSION].isNull())
            sysInfo += "<tr><td>" + i18n( "KDE Applications:" ) + "</td><td>" + htmlQuote(m_info[KDEAPPS_VERSION]) + "</td></tr>";
        if (!m_info[QT5_VERSION].isNull())
            sysInfo += "<tr><td>" + i18n( "Qt:" ) + "</td><td>" + htmlQuote(m_info[QT5_VERSION]) + "</td></tr>";
    }
    sysInfo += "</table>";
    
    // Display Info START ////////

    // OpenGL info
    if ( glInfo() )
    {
        sysInfo += "<h2 id=\"display\">" + i18n( "Display Info" ) + "</h2>";
        sysInfo += "<table>";
        sysInfo += "<tr><td>" + i18n( "Vendor:" ) + "</td><td>" + htmlQuote(m_info[GFX_MODEL]) +  "</td></tr>";
//         sysInfo += "<tr><td>" + i18n( "Model:" ) + "</td><td>" + htmlQuote(m_info[GFX_MODEL]) + "</td></tr>";
        sysInfo += "<tr><td>" + i18n( "2D driver:" ) + "</td><td>" + htmlQuote(m_info[GFX_2D_DRIVER]) + "</td></tr>";
        if (!m_info[GFX_3D_DRIVER].isNull())
            sysInfo += "<tr><td>" + i18n( "3D driver:" ) + "</td><td>" + htmlQuote(m_info[GFX_3D_DRIVER]) + "</td></tr>";
    }
    waylandInfo();
    if (!m_info[WAYLAND_VER].isNull())
            sysInfo += "<tr><td>" + i18n( "Wayland:" ) + "</td><td>" + htmlQuote(m_info[WAYLAND_VER]) + "</td></tr>";
    sysInfo += "</table>";
    
    // Display Info END /////////

    // battery info
    infoMessage( i18n( "Looking for battery and AC information..." ) );
    if ( batteryInfo() )
    {
        sysInfo += "<h2 id=\"battery\">" + i18n( "Battery Information" ) + "</h2>";
        sysInfo += "<table>";
        if (!m_info[BATT_IS_PLUGGED].isEmpty())
            sysInfo += "<tr><td>" + i18n( "Battery present:" ) + "</td><td>" + m_info[BATT_IS_PLUGGED] + "</td></tr>";
        if (!m_info[BATT_CHARGE_STATE].isEmpty())
            sysInfo += "<tr><td>" + i18nc( "battery state", "State:" ) + "</td><td>" + m_info[BATT_CHARGE_STATE] + "</td></tr>";
        if (!m_info[BATT_CHARGE_PERC].isEmpty())
            sysInfo += "<tr><td>" + i18n( "Charge percent:" ) + "</td><td>" + m_info[BATT_CHARGE_PERC] + "</td></tr>";
        if (!m_info[BATT_IS_RECHARGEABLE].isEmpty())
            sysInfo += "<tr><td>" + i18n( "Rechargeable:" ) + "</td><td>" + m_info[BATT_IS_RECHARGEABLE] + "</td></tr>";
        if (!m_info[AC_IS_PLUGGED].isEmpty())
            sysInfo += "<tr><td>" + i18n( "AC plugged:" ) + "</td><td>" + m_info[AC_IS_PLUGGED] + "</td></tr>";
        sysInfo += "</table>";
    }

    // more CPU info
    if ( !m_info[CPU_MODEL].isNull() )
    {
        sysInfo += "<h2 id=\"cpu\">" + i18n( "CPU Information" ) + "</h2>";
        sysInfo += "<table>";
        sysInfo += "<tr><td>" + i18n( "Processor (CPU):" ) + "</td><td>" + htmlQuote(m_info[CPU_MODEL]) + "</td></tr>";
        sysInfo += "<tr><td>" + i18n( "Speed:" ) + "</td><td>" +
                   i18n( "%1 MHz" , KGlobal::locale()->formatNumber( m_info[CPU_SPEED].toFloat(), 2 ) ) + "</td></tr>";
        int core_num = m_info[CPU_CORES].toUInt() + 1;
        if ( core_num > 1 )
            sysInfo += "<tr><td>" + i18n("Cores:") + QString("</td><td>%1</td></tr>").arg(core_num);

        if (!m_info[CPU_TEMP].isEmpty())
        {
            sysInfo += "<tr><td>" + i18n("Temperature:") + QString("</td><td>%1</td></tr>").arg(m_info[CPU_TEMP]);
        }
        sysInfo += "</table>";
    }

    // memory info
    infoMessage( i18n( "Looking for memory information..." ) );
    memoryInfo();
    sysInfo += "<h2 id=\"memory\">" + i18n( "Memory Information" ) + "</h2>";
    sysInfo += "<table>";
    sysInfo += "<tr><td>" + i18n( "Total memory (RAM):" ) + "</td><td>" + m_info[MEM_TOTALRAM] + "</td></tr>";
    sysInfo += "<tr><td>" + i18n( "Free memory:" ) + "</td><td>" + m_info[MEM_FREERAM] + "</td></tr>";
    dummy = i18n( "Used Memory" );
    dummy += "<tr><td>" + i18n( "Total swap:" ) + "</td><td>" + m_info[MEM_TOTALSWAP] + "</td></tr>";
    sysInfo += "<tr><td>" + i18n( "Free swap:" ) + "</td><td>" + m_info[MEM_FREESWAP] + "</td></tr>";
    sysInfo += "</table>";

    sysInfo += "</div>";

    sysInfo += "</div><div id=\"column1\">"; // second column

    // OS info
    infoMessage( i18n( "Getting OS information...." ) );

//     // common folders
//     sysInfo += "<h2 id=\"dirs\">" + i18n( "Common Folders" ) + "</h2>"; sysInfo += "<ul>";
//     if ( KStandardDirs::exists( KGlobalSettings::documentPath() + "/" ) )
//         sysInfo += QString( "<li><a href=\"file:%1\">" ).arg( htmlQuote(KGlobalSettings::documentPath()) )
//                    + i18n( "My Documents" ) + "</a></li>";
//     sysInfo += QString( "<li><a href=\"file:%1\">" ).arg( htmlQuote(QDir::homePath()) ) + i18n( "My Home Folder" ) + "</a></li>";
//     sysInfo += QString( "<li><a href=\"file:%1\">" ).arg( htmlQuote(QDir::rootPath()) ) + i18n( "Root Folder" ) + "</a></li>";
//     sysInfo += "<li><a href=\"remote:/\">" + i18n( "Network Folders" ) + "</a></li>";
//     sysInfo += "</ul>";

    // net info
    infoMessage( i18n( "Looking up network status..." ) );
    QString state = netStatus();
    if ( !state.isEmpty() ) // assume no network manager / networkstatus
    {
        sysInfo += "<h2 id=\"net\">" + i18n( "Network Status" ) + "</h2>";
        sysInfo += "<ul>";
        sysInfo += "<li>" + state + "</li>";
        sysInfo += "</ul>";
    }

    // disk info
    infoMessage( i18n( "Looking for disk information..." ) );
    m_devices.clear();
    sysInfo += "<h2 id=\"hdds\">" + i18n( "Disk Information" ) + "</h2>";
    sysInfo += diskInfo();


    // Send the data
    content = content.arg( sysInfo ); // put the sysinfo text into the main box
    data( content.toUtf8() );
    data( QByteArray() ); // empty array means we're done sending the data
    finished();
}

void kio_sysinfoProtocol::mimetype( const KUrl & /*url*/ )
{
    mimeType( "application/x-sysinfo" );
    finished();
}

static unsigned long int scan_one( const char* buff, const char *key )
{
    const char *b = strstr( buff, key );
    if ( !b )
        return 0;
    unsigned long int val = 0;
    if ( sscanf( b + strlen( key ), ": %lu", &val ) != 1 )
        return 0;
    //kDebug(1242) << "scan_one " << key << " " << val;
    return val;
}

static quint64 calculateFreeRam()
{
    FILE *fd = fopen( "/proc/meminfo", "rt" );
    if ( !fd )
        return 0;

    QString MemInfoBuf = QTextStream(fd).readAll();
    fclose( fd );

    quint64 MemFree = scan_one( MemInfoBuf.toLatin1(), "MemFree" );
    quint64 Buffers = scan_one( MemInfoBuf.toLatin1(), "Buffers" );
    quint64 Cached  = scan_one( MemInfoBuf.toLatin1(), "Cached" );
    quint64 Slab    = scan_one( MemInfoBuf.toLatin1(), "Slab" );

    MemFree += Cached + Buffers + Slab;
    if ( MemFree > 50 * 1024 )
        MemFree -= 50 * 1024;
    return MemFree;
}

void kio_sysinfoProtocol::memoryInfo()
{
    struct sysinfo info;
    int retval = sysinfo( &info );

    if ( retval !=-1 )
    {
        quint64 mem_unit = info.mem_unit;

        m_info[MEM_TOTALRAM] = formattedUnit( quint64(info.totalram) * mem_unit );
        quint64 totalFree = calculateFreeRam() * 1024;
        kDebug(1242) << "total " << totalFree << " free " << info.freeram << " unit " << mem_unit;
        if ( totalFree > info.freeram * info.mem_unit || true )
            m_info[MEM_FREERAM] = i18n("%1 (+ %2 Caches)",
                                       formattedUnit( quint64(info.freeram) * mem_unit ),
                                       formattedUnit( totalFree - info.freeram * mem_unit ));
        else
            m_info[MEM_FREERAM] = formattedUnit( quint64(info.freeram) * mem_unit );

        m_info[MEM_TOTALSWAP] = formattedUnit( quint64(info.totalswap) * mem_unit );
        m_info[MEM_FREESWAP] = formattedUnit( quint64(info.freeswap) * mem_unit );

        m_info[SYSTEM_UPTIME] = KIO::convertSeconds( info.uptime );
    }
}

void kio_sysinfoProtocol::cpuInfo()
{
    QString speed = readFromFile( "/proc/cpuinfo", "cpu MHz", ":" );

    if ( speed.isNull() )    // PPC?
        speed = readFromFile( "/proc/cpuinfo", "clock", ":" );

    if ( speed.endsWith( "MHz", Qt::CaseInsensitive ) )
        speed = speed.left( speed.length() - 3 );

    m_info[CPU_SPEED] = speed;
    m_info[CPU_CORES] = readFromFile( "/proc/cpuinfo", "processor", ":", true );

    const char* const names[] = { "THM0", "THRM", "THM" };
    for ( unsigned i = 0; i < sizeof(names)/sizeof(*names); ++i )
    {
        m_info[CPU_TEMP] = readFromFile(QString("/proc/acpi/thermal_zone/%1/temperature").arg(names[i]), "temperature", ":");
        const QString temp = m_info[CPU_TEMP].trimmed().remove(" C");
        if (!temp.isEmpty())
        {
            m_info[CPU_TEMP] = i18nc("temperature", "%1 °C", temp.toInt());
            break;
        }
    }

    m_info[CPU_MODEL] = readFromFile( "/proc/cpuinfo", "model name", ":" );
    if ( m_info[CPU_MODEL].isNull() ) // PPC?
         m_info[CPU_MODEL] = readFromFile( "/proc/cpuinfo", "cpu", ":" );
}

QString kio_sysinfoProtocol::diskInfo()
{
    QString result = "<table>\n<tr><th></th><th>" + i18n( "Device" ) + "</th><th>" + i18n( "Filesystem" ) + "</th><th>" +
                     i18n( "Total space" ) + "</th><th>" + i18n( "Available space" ) + "</th><th></th></tr>\n";

    if ( fillMediaDevices() )
    {
        for ( QList<DiskInfo>::ConstIterator it = m_devices.constBegin(); it != m_devices.constEnd(); ++it )
        {
            QString tooltip = i18n("Press the right mouse button for more options (such as Mount or Eject.)");

            DiskInfo di = ( *it );
            unsigned int percent = 0;
            quint64 usage = di.total - di.avail;
            if (di.total)
                percent = usage / ( di.total / 100);

            QString media = "file://" + di.deviceNode;

            QString unmount;
            if (di.removable)
                unmount = QString("<a href=\"#unmount=%1\">%2</a>").
                          arg( di.id ).arg( icon( "media-eject", 16 ) );

            result += QString( "<tr><td rowspan=\"2\">%1</td><td><a href=\"%2\" title=\"%7\">%3</a></td>" \
                               "<td>%4</td><td>%5</td><td>%6</td><td rowspan=\"2\">%8</td></tr>\n" ).
                      arg( hdicon() ).arg( htmlQuote(media) ).arg( htmlQuote(di.label) ).arg( di.fsType ).
                      arg( di.total ? formattedUnit( di.total) : QString::null).
                      arg( di.mounted ? formattedUnit( di.avail ) : QString::null).
                      arg( htmlQuote( tooltip ) ).
                      arg(unmount);

            result += QString("<tr><td colspan=\"4\" %1>").arg( di.mounted ? "class=\"bar\"" : "");
            if (di.mounted)
            {
                QColor c;
                c.setHsv(100-percent, 180, 230);
                QString dp = formattedUnit(usage).replace(" ", "&nbsp;");
                QString dpl, dpr;
                if (percent >= 50)
                    dpl = dp;
                else
                    dpr = "<span>" + dp + "</span>";
                result += QString("<div><span class=\"filled\" style=\"width: %1%; background-color: %4\">"
                                  "%2</span>%3</div>\n")
                          .arg(percent).arg(dpl).arg(dpr).arg(c.name());
            }
            result += "</td></tr>\n";
        }
    }

    result += "</table>";

    return result;
}

#ifdef HAVE_GLXCHOOSEVISUAL
#include <GL/glx.h>
#endif

//-------------------------------------
bool hasDirectRendering ( QString &renderer ) {
    renderer = QString::null;

    Display *dpy = QX11Info::display();
    if (!dpy) return false;

#ifdef HAVE_GLXCHOOSEVISUAL
    int attribSingle[] = {
        GLX_RGBA,
        GLX_RED_SIZE,   1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE,  1,
        None
    };
    int attribDouble[] = {
      GLX_RGBA,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      GLX_DOUBLEBUFFER,
      None
    };

    XVisualInfo* visinfo = glXChooseVisual (
        dpy, QApplication::desktop()->primaryScreen(), attribSingle
    );
    if (visinfo)
    {
        GLXContext ctx = glXCreateContext ( dpy, visinfo, NULL, True );
        if (glXIsDirect(dpy, ctx))
        {
            glXDestroyContext (dpy,ctx);
            return true;
        }

        XSetWindowAttributes attr;
        unsigned long mask;
        Window root;
        XVisualInfo *visinfo;
        int width = 100, height = 100;
        int scrnum = QApplication::desktop()->primaryScreen();

        root = RootWindow(dpy, scrnum);

        visinfo = glXChooseVisual(dpy, scrnum, attribSingle);
        if (!visinfo)
        {
            visinfo = glXChooseVisual(dpy, scrnum, attribDouble);
            if (!visinfo)
            {
                fprintf(stderr, "Error: could not find RGB GLX visual\n");
                return false;
            }
        }

        attr.background_pixel = 0;
        attr.border_pixel = 0;
        attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
        attr.event_mask = StructureNotifyMask | ExposureMask;
        mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

        Window win = XCreateWindow(dpy, root, 0, 0, width, height,
                                   0, visinfo->depth, InputOutput,
                                   visinfo->visual, mask, &attr);

        if ( glXMakeCurrent(dpy, win, ctx))
            renderer = (const char *) glGetString(GL_RENDERER);
        XDestroyWindow(dpy, win);

        glXDestroyContext (dpy,ctx);
        return false;
    }
    else
    {
        return false;
    }
#else
    return false;
#endif
}


bool kio_sysinfoProtocol::glInfo()
{
    /* This leaks like sieve. Since gfx cards usually don't happen
       to change to something else while the computer is running,
       run this just once and keep the results. */
    static bool beenhere = false;
    static bool prevresult = false;
    if( beenhere )
        return prevresult;
    beenhere = true;

#ifdef HAVE_HD
    /* Prepare HD */
    static hd_data_t hd_data;
    static bool inited_hd = false;
    if ( !inited_hd )
    {
        memset(&hd_data, 0, sizeof(hd_data));
        inited_hd = true;
    }

    if (!hd_list(&hd_data, hw_display, 1, NULL))
        return false;

    hd_t *hd = hd_get_device_by_idx(&hd_data, hd_display_adapter(&hd_data));
#endif

    /* Build list of all loaded Xorg modules */
    QStringList loaded_modules;
    QFile file("/var/log/Xorg.0.log");
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        QString line;

        while (!stream.atEnd()) {
            line = stream.readLine();
            QRegExp rx_mod_load("\\(II\\) LoadModule: \"([\\S]+)\"");
            QRegExp rx_mod_unload("\\(II\\) UnloadModule: \"([\\S]+)\"");
            if (rx_mod_load.indexIn(line) > -1) {
                loaded_modules.append(rx_mod_load.cap(1));
            } else if (rx_mod_unload.indexIn(line) > -1) {
                loaded_modules.removeOne(rx_mod_unload.cap(1));
            }
        }
    }

    /* Names of possible 2D drivers. We will look for them in cached modules */
    QStringList possible_2d_drivers;
#ifdef HAVE_HD
    if (hd) {
        driver_info_t *di = hd->driver_info;
        for (di = di; di; di = di->next) {
            if (di->any.type == di_x11) {
                possible_2d_drivers.append(di->x11.server);
            } else if (di->any.type == di_module && di->module.names) {
                possible_2d_drivers.append(di->module.names->str);
            }
        }
    }
#endif
    possible_2d_drivers << "fglrx" << "intel" << "nouveau" << "nv" << "nvidia" << "openchrome" << "radeon" << "radeonhd" << "vboxvideo";
    possible_2d_drivers << "vesa" << "fbdev";

    /* Find first of possible 2D drivers that is actually loaded */
    QString driver = QString::null;
    for (int i = 0; i < possible_2d_drivers.size(); ++i) {
        QString curr_driver = possible_2d_drivers.at(i);
        if (loaded_modules.contains(curr_driver)) {
            m_info[GFX_2D_DRIVER] = curr_driver;
            driver = curr_driver; /* FIXME */
            break;
        }
    }

    /* Grab OpenGL info */
    QString opengl_vendor = QString::null;
    QString opengl_renderer = QString::null;
    QString opengl_version = QString::null;
    QString opengl_mesa = QString::null;
    /* FIXME: unsafe, replace popen with QProcess? */
    FILE *fd = popen("glxinfo", "r");
    if (fd) {
        QTextStream is(fd);
        while (!is.atEnd()) {
            QString line = is.readLine();
            if (line.startsWith("OpenGL vendor string:")) {
                opengl_vendor = line.section(':', 1, 1);
            } else if (line.startsWith("OpenGL renderer string:")) {
                opengl_renderer = line.section(':', 1, 1);
            } else if (line.startsWith("OpenGL version string:")) {
                opengl_version = line.section(':', 1, 1);
            }
        }
    }
    if (fd) {
        /* FIXME: this is hack to do not let QTextStream touch fd after closing
         * it. Prevents whole kio_sysinfo from crashing */
        pclose(fd);
    }
    QRegExp rx("Mesa (\\S+)");
    if (rx.indexIn(opengl_version) > -1) {
        opengl_mesa = rx.cap(1);
    }

    /* As first choice, use OpenGL info */
    m_info[GFX_VENDOR] = opengl_vendor;
    m_info[GFX_MODEL] = opengl_renderer;    

    /* Determine 3D driver from OpenGL info */
    QRegExp rx_g("Gallium.+on ([\\S]+)");
    if (opengl_renderer.contains("Software Rasterizer")) { /* swrast */
        m_info[GFX_3D_DRIVER] = QString("%1 (%2)").arg("swrast").arg(i18n("No 3D Acceleration"));
    } else if (rx_g.indexIn(opengl_renderer) > -1) { /* Gallium */
        m_info[GFX_MODEL] = rx_g.cap(1);
        if (opengl_vendor.contains("R300")) {
            m_info[GFX_VENDOR] = GFX_VENDOR_ATI;
            m_info[GFX_3D_DRIVER] = "R300";
        } else if (opengl_vendor.contains("R600")) {
            m_info[GFX_VENDOR] = GFX_VENDOR_ATI;
            m_info[GFX_3D_DRIVER] = "R600";
        } else if (opengl_vendor.contains("nouveau")) {
            m_info[GFX_VENDOR] = GFX_VENDOR_NVIDIA;
            m_info[GFX_3D_DRIVER] = "nouveau";
        } else {
            m_info[GFX_3D_DRIVER] = i18n("Unknown");
        }
        m_info[GFX_3D_DRIVER] += " Gallium";
    } else if (opengl_renderer.contains("Mesa DRI")) { /* Classic Mesa */
        QRegExp rx_r("(R[0-9]00) \\(([^\\)]+)\\)");
        if (rx_r.indexIn(opengl_renderer) > -1) {
            m_info[GFX_VENDOR] = GFX_VENDOR_ATI;
            m_info[GFX_MODEL] = rx_r.cap(2);
            m_info[GFX_3D_DRIVER] = rx_r.cap(1);
        } else {
            m_info[GFX_3D_DRIVER] = i18n("Mesa");
        }
        m_info[GFX_3D_DRIVER] += " classic";
    } else if (opengl_vendor.contains("ATI") || opengl_vendor.contains("Advanced Micro Devices")) { /* Proprietary ATI */
        m_info[GFX_VENDOR] = GFX_VENDOR_ATI;
        m_info[GFX_MODEL] = opengl_renderer;
        m_info[GFX_3D_DRIVER] = "ATI";
    } else if (opengl_vendor.contains("NVIDIA")) { /* Proprietary NVIDIA */
        m_info[GFX_VENDOR] = GFX_VENDOR_NVIDIA;
        m_info[GFX_MODEL] = opengl_renderer;
        QRegExp rx_n("(NVIDIA [0-9\\.]+)");
        if (rx_n.indexIn(opengl_version) > -1) {
            m_info[GFX_3D_DRIVER] = rx_n.cap(1);
        } else {
            m_info[GFX_3D_DRIVER] = "NVIDIA";
        }
    }
    
    if (!opengl_mesa.isNull())
        m_info[GFX_3D_DRIVER] += QString(" (%1)").arg(opengl_mesa);

#ifdef HAVE_HD
    /* Using HD (when possible) should gave the best result */
    if (hd) {
        m_info[GFX_VENDOR] = hd->vendor.name;
        /* GFX_MODEL maybe empty with newer models */
        if (!!hd->device.name) {
            m_info[GFX_MODEL] = hd->device.name;
        }
    }
#endif

    prevresult = true;
    return true;

#if 0
#ifdef HAVE_HD
    QString renderer;
    bool dri = hasDirectRendering( renderer );

    if (!driver.isNull())
    {
        if (dri)
            m_info[GFX_3D_DRIVER] = i18n("%1 (3D Support)", driver);
        else
        {
            if ( renderer.contains( "Mesa GLX Indirect" ) )
                m_info[GFX_3D_DRIVER] = i18n("%1 (No 3D Support)", driver);
            else
                m_info[GFX_3D_DRIVER] = driver;
        }
    }
    else
        m_info[GFX_3D_DRIVER] = i18n( "Unknown" );
#else
    m_info[GFX_3D_DRIVER] = opengl_version;
#endif

    prevresult = true;
    return true;
#endif
}

bool kio_sysinfoProtocol::kdeInfo() 
{
    /* Grab KF5 & Qt5 info */
    QString qt5_version = QString::null;
    QString kf5_version = QString::null;
    /* FIXME: unsafe, replace popen with QProcess? */
    FILE *fd = popen("kf5-config --version", "r");
    if (fd) {
        QTextStream is(fd);
        while (!is.atEnd()) {
            QString line = is.readLine();
            if (line.startsWith("Qt:")) {
                qt5_version = line.section(':', 1, 1);
            } else if (line.startsWith("KDE Frameworks:")) {
                kf5_version = line.section(':', 1, 1);
            }
        }
    }
    if (fd) {
        /* FIXME: this is hack to do not let QTextStream touch fd after closing
         * it. Prevents whole kio_sysinfo from crashing */
        pclose(fd);
    }
    
    m_info[QT5_VERSION] = qt5_version;
    m_info[KF5_VERSION] = kf5_version;
    
    /* Grab KF5 & Qt5 info */
    QString kdeapps_version = QString::null;
    /* FIXME: unsafe, replace popen with QProcess? */
    FILE *fd1 = popen("dolphin --version", "r");
    if (fd) {
        QTextStream is(fd1);
        while (!is.atEnd()) {
            QString line = is.readLine();
            if (line.startsWith("dolphin")) {
                kdeapps_version = line.section(' ', 1, 1);
            }
        }
    }
    if (fd1) {
        /* FIXME: this is hack to do not let QTextStream touch fd after closing
         * it. Prevents whole kio_sysinfo from crashing */
        pclose(fd1);
    }
    
    m_info[KDEAPPS_VERSION] = kdeapps_version;
    
    return true;
}    

void kio_sysinfoProtocol::waylandInfo()
{
    QFile file("/usr/include/wayland-version.h");
    if (file.exists()) {
        m_info[WAYLAND_VER] = readFromFile ( "/usr/include/wayland-version.h", "#define WAYLAND_VERSION", "\"" );
    }
}


QString kio_sysinfoProtocol::hdicon() const
{
    QString hdimagePath = "file://" + KStandardDirs::locate( "data", "sysinfo/about/images/hdd.png");
    return QString( "<img src=\"%1\" width=\"32\" height=\"32\" valign=\"bottom\"/>").arg( hdimagePath );
}

QString kio_sysinfoProtocol::icon( const QString & name, int size ) const
{
    QString path = KIconLoader::global()->iconPath( name, -size );
    return QString( "<img src=\"file:%1\" width=\"%2\" height=\"%3\" valign=\"bottom\"/>" )
        .arg( htmlQuote(path) ).arg( size ).arg( size );
}

void kio_sysinfoProtocol::osInfo()
{
    struct utsname uts;
    uname( &uts );
    m_info[ OS_SYSNAME ] = uts.sysname;
    m_info[ OS_RELEASE ] = uts.release;
    m_info[ OS_VERSION ] = uts.version;
    m_info[ OS_MACHINE ] = uts.machine;
    m_info[ OS_HOSTNAME ] = uts.nodename;

//     m_info[ OS_USER ] = KUser().loginName();

#ifdef WITH_FEDORA
    m_info[ OS_SYSTEM ] = readFromFile( "/etc/redhat-release" );
#elif defined(WITH_SUSE)
    m_info[ OS_SYSTEM ] = readFromFile( "/etc/SuSE-release" );
#elif defined(WITH_DEBIAN)
    m_info[ OS_SYSTEM ] = readFromFile( "/etc/debian_version" );
#elif defined(WITH_UBUNTU)
    m_info[ OS_SYSTEM ] = readFromFile ( "/etc/os-release", "NAME", "=" ).remove(QChar('\"'), Qt::CaseInsensitive) + " " + readFromFile ( "/etc/os-release", "VERSION", "=" ).remove(QChar('\"'), Qt::CaseInsensitive);
#else
    m_info[ OS_SYSTEM ] = i18nc( "Unknown operating system version", "Unknown" );
#endif
    m_info[ OS_SYSTEM ].replace("X86-64", "x86_64");
}

extern "C" int KDE_EXPORT kdemain(int argc, char **argv)
{
    KComponentData componentData( "kio_sysinfo" );
    ( void ) KGlobal::locale();
    QCoreApplication a(argc, argv);

    kDebug(1242) << "*** Starting kio_sysinfo ";

    if (argc != 4) {
        kDebug(1242) << "Usage: kio_sysinfo  protocol domain-socket1 domain-socket2";
        exit(-1);
    }

    kio_sysinfoProtocol slave(argv[2], argv[3]);
    slave.dispatchLoop();

    kDebug(1242) << "*** kio_sysinfo Done";
    return 0;
}

bool kio_sysinfoProtocol::fillMediaDevices()
{
    QEventLoop e;
    while (e.processEvents()) {}

    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(m_predicate);

    if (deviceList.isEmpty())
    {
        kDebug(1242) << "DEVICE LIST IS EMPTY!";
        return false;
    }

    m_devices.clear();

    Q_FOREACH (const Solid::Device &device, deviceList)
    {
        if (!device.isValid())
            continue;

        const Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
        const Solid::StorageVolume *volume = device.as<Solid::StorageVolume>();
        const Solid::Block *block = device.as<Solid::Block>();
        const Solid::StorageDrive *drive = device.parent().as<Solid::StorageDrive>();

        DiskInfo di;

        di.id = device.udi();
        if (block)
            di.deviceNode = block->device();
        if (volume)
            di.fsType = volume->fsType();
        di.mounted = access && access->isAccessible();
        di.removable = (di.mounted || device.is<Solid::OpticalDisc>()) && drive && drive->isRemovable();
        if ( di.mounted )
            di.mountPoint = access->filePath();

        if (volume && !volume->label().isEmpty())
            di.label = volume->label();
        else if (!di.mountPoint.isEmpty())
            di.label = di.mountPoint;
        else
            di.label = di.deviceNode;

        di.iconName = device.icon();

        di.total = di.avail = 0;

        if (volume)
            di.total = volume->size();

        // calc the free/total space
        struct statfs sfs;
        if ( di.mounted && statfs( QFile::encodeName( di.mountPoint ), &sfs ) == 0 )
        {
            di.total = ( unsigned long long )sfs.f_blocks * sfs.f_bsize;
            di.avail = ( unsigned long long )( getuid() ? sfs.f_bavail : sfs.f_bfree ) * sfs.f_bsize;
        }

        m_devices.append( di );
    }

    // this is ugly workaround, should be in HAL but there is no LVM support
    QRegExp rxlvm("^/dev/mapper/\\S*-\\S*");

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName);

    Q_FOREACH ( KMountPoint::Ptr mountPoint, mountPoints)
    {
        if ( rxlvm.exactMatch( mountPoint->realDeviceName() ) )
        {
            DiskInfo di;

            di.mountPoint = mountPoint->mountPoint();
            di.label = di.mountPoint;
            di.mounted = true;
            di.removable = false; /* we don't want Solid to unmount KMountPoint */
            di.deviceNode = mountPoint->realDeviceName();
            di.fsType = mountPoint->mountType();
            di.iconName = QString::fromLatin1( "drive-harddisk" );

            di.total = di.avail = 0;

            // calc the free/total space
            struct statfs sfs;
            if ( di.mounted && statfs( QFile::encodeName( di.mountPoint ), &sfs ) == 0 )
            {
                di.total = ( unsigned long long )sfs.f_blocks * sfs.f_bsize;
                di.avail = ( unsigned long long )( getuid() ? sfs.f_bavail : sfs.f_bfree ) * sfs.f_bsize;
            }

            m_devices.append( di );
        }
    }


    return true;
}

bool kio_sysinfoProtocol::batteryInfo()
{
    const QList<Solid::Device> & deviceList = Solid::Device::listFromQuery( SOLID_BATTERY_AC_PREDICATE );

    if (deviceList.isEmpty()) {
        return false;
    }

    Q_FOREACH ( const Solid::Device & device, deviceList )
    {
        if ( device.is<Solid::Battery>() )
        {
            const Solid::Battery * battery = device.as<Solid::Battery>();
            m_info[BATT_IS_PLUGGED] = battery->isPlugged() ? i18n( "yes" ) : i18n( "no" );
            m_info[BATT_CHARGE_PERC] =  i18nc( "battery charge percent label", "%1%", battery->chargePercent() );
            switch ( battery->chargeState() )
            {
            case Solid::Battery::NoCharge:
                m_info[BATT_CHARGE_STATE] = i18nc( "battery charge state", "No Charge" );
                break;
            case Solid::Battery::Charging:
                m_info[BATT_CHARGE_STATE] = i18nc( "battery charge state", "Charging" );
                break;
            case Solid::Battery::Discharging:
                m_info[BATT_CHARGE_STATE] = i18nc( "battery charge state", "Discharging" );
                break;
            default:
                m_info[BATT_CHARGE_STATE] = i18nc( "battery charge state", "Unknown" );
            }
            m_info[BATT_IS_RECHARGEABLE] = battery->isRechargeable() ? i18n( "yes" ) : i18n( "no" );
        }
        else if ( device.is<Solid::AcAdapter>() )
        {
            const Solid::AcAdapter * ac = device.as<Solid::AcAdapter>();
            m_info[AC_IS_PLUGGED] = ac->isPlugged() ? i18n( "yes" ) : i18n( "no" );
        }
    }

    return true;
}
