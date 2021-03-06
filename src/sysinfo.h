//////////////////////////////////////////////////////////////////////////
// sysinfo.h                                                            //
//                                                                      //
// Copyright (C)  2005  Lukas Tinkl <lukas.tinkl@suse.cz>               //
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

#ifndef _sysinfo_H_
#define _sysinfo_H_

#include <qmap.h>
#include <qstringlist.h>

#include <kurl.h>
#include <kicontheme.h>
#include <kio/slavebase.h>

#include <solid/predicate.h>

#define GFX_VENDOR_ATI "ATI Technologies Inc."
#define GFX_VENDOR_NVIDIA "NVIDIA Corporation"

struct DiskInfo
{
    // taken from media:/
    QString id;
    QString name;
    QString label;
    QString deviceNode;
    QString mountPoint;
    QString fsType;
    bool mounted;
    bool removable;
    QString iconName;

    // own stuff
    quint64 total, avail; // space on device
};


/**
 * System information IO slave.
 *
 * Produces an HTML page with system information overview
 */
class kio_sysinfoProtocol : public KIO::SlaveBase
{
public:
    kio_sysinfoProtocol( const QByteArray &pool_socket, const QByteArray &app_socket );
    virtual ~kio_sysinfoProtocol();
    virtual void mimetype( const KUrl& url );
    virtual void get( const KUrl& url );

    /**
     * Info field
     */
    enum
    {
        MEM_TOTALRAM = 0,       // in sysinfo.mem_unit
        MEM_FREERAM,
        MEM_TOTALSWAP,
        MEM_FREESWAP,
        SYSTEM_UPTIME,          // in seconds
        CPU_MODEL,
        CPU_SPEED,              // in MHz
        CPU_CORES,
        CPU_TEMP,
        OS_SYSNAME,             // man 2 uname
        OS_RELEASE,
        OS_VERSION,
        OS_MACHINE,
        OS_USER,                // username
        OS_SYSTEM,              // OS version
        OS_HOSTNAME,
        GFX_VENDOR,              // Display stuff
        GFX_MODEL,
        GFX_2D_DRIVER,
        GFX_3D_DRIVER,
        BATT_IS_PLUGGED,        // see Solid::Battery
        BATT_CHARGE_PERC,
        BATT_CHARGE_STATE,
        BATT_IS_RECHARGEABLE,
        AC_IS_PLUGGED,          // see Solid::AcAdapter
        SYSINFO_LAST,
        KF5_VERSION,
        QT5_VERSION,
        KDEAPPS_VERSION,
        WAYLAND_VER
    };

private:
    /**
     * Gather basic memory info
     */
    void memoryInfo();

    /**
     * Gather CPU info
     */
    void cpuInfo();

    /**
     * @return a formatted table with disk partitions
     */
    QString diskInfo();

    /**
     * Get info about kernel and OS version (uname)
     */
    void osInfo();

    /**
     * Gather basic OpenGL info
     */
    bool glInfo();
    
    /**
     * Gather KF5, Qt5 and KDE Apps info
     */
    bool kdeInfo();
    
    /**
     * Gather Wayland info
     */
    void waylandInfo();

    /**
     * Gather battery / AC adapter status
     */
    bool batteryInfo();
    
    /**
     * Helper function to return default hd icon
     * @return hdimage with 32x32 pixels
     */
    QString hdicon() const;

    /**
     * Helper function to locate a KDE icon
     * @return img tag with full path to the icon
     */
    QString icon( const QString & name, int size = KIconLoader::SizeSmall ) const;

    /**
     * Fill the list of devices (m_devices) with data from the media KIO protocol
     * @return true on success
     */
    bool fillMediaDevices();

    /**
     * Map holding the individual info attributes
     */
    QMap<int, QString> m_info;

    QList<DiskInfo> m_devices;
    Solid::Predicate m_predicate;
};

#endif
