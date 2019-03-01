/*
 * deconz_lqi_plugin.cpp
 *
 *  Created on: 19.02.2019
 *      Author: ma-ca
 */

#include "deconz_lqi_plugin.h"

const QString sqliteDatabaseName = "/run/shm/lqi.db";

/*! Plugin constructor.
    \param parent - the parent object
 */
LqiPlugin::LqiPlugin(QObject *parent) : QObject(parent) {
    // keep a pointer to the ApsController
    m_apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(m_apsCtrl != 0);

    // APSDE-DATA.confirm handler
//    connect(m_apsCtrl, SIGNAL(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)),
//            this, SLOT(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)));

    // APSDE-DATA.indication handler
    connect(m_apsCtrl, SIGNAL(apsdeDataIndication(const deCONZ::ApsDataIndication&)),
            this, SLOT(apsdeDataIndication(const deCONZ::ApsDataIndication&)));

    db = nullptr;

    initDb();
}

/*! Deconstructor for plugin.
 */
LqiPlugin::~LqiPlugin() {
    m_apsCtrl = 0;
}

/*! APSDE-DATA.indication callback.
    \param ind - the indication primitive
    \note Will be called from the main application for every incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void LqiPlugin::apsdeDataIndication(const deCONZ::ApsDataIndication &ind)
{
    if (ind.profileId() != ZDP_PROFILE_ID || ind.clusterId() != ZDP_MGMT_LQI_RSP_CLID)
    {
        return;  // only use ZDP Mgmt_Lqi_rsp
    }

    int rc = sqlite3_open(qPrintable(sqliteDatabaseName), &db);

    if (rc != SQLITE_OK) {
        // failed
        DBG_Printf(DBG_ERROR, "Lqi Can't open database: %s\n", sqlite3_errmsg(db));
        db = nullptr;
        return;
    }

    QString srcAddr = QString("%1").arg(ind.srcAddress().ext(), 16, 16, QChar('0')).toUpper();

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    DBG_Printf(DBG_INFO, "  Lqi asdu.length = %d, asdu = %s \n", ind.asdu().length(), ind.asdu().toHex().data());

    quint8 zdpSeq;
    quint8 status;
    quint8 neighborTableEntries;
    quint8 startIndex;
    quint8 neighborTableListCount;

    stream >> zdpSeq;
    stream >> status;

    DBG_Printf(DBG_INFO, "  Lqi seq = %d (0x%02X), status = 0x%02X, srcAddr = %s \n",
            zdpSeq, zdpSeq, status, qPrintable(srcAddr));

    if (status != 0x00)
    {
        return;
    }

    stream >> neighborTableEntries;
    stream >> startIndex;
    stream >> neighborTableListCount;

    DBG_Printf(DBG_INFO, "  Lqi neighborTableEntries = %d, startIndex = %d, neighborTableListCount = %d \n",
            neighborTableEntries, startIndex, neighborTableListCount);

    quint8 tableIndex = startIndex;
    quint8 tableCount = 0;

    while (!stream.atEnd() && tableCount < neighborTableListCount)
    {
        quint64 panid;
        quint64 extAddr;
        quint16 networkAddr;
        quint8 tmpByteDevTypeRxRel;
        quint8 permitJoin;
        quint8 depth;
        quint8 lqiLinkQuality;

        stream >> panid;
        stream >> extAddr;
        stream >> networkAddr;
        stream >> tmpByteDevTypeRxRel;
        stream >> permitJoin;
        stream >> depth;
        stream >> lqiLinkQuality;

        QString panIdStr = QString("%1").arg(panid, 16, 16, QChar('0')).toUpper();
        QString extAddrStr = QString("%1").arg(extAddr, 16, 16, QChar('0')).toUpper();
        QString networkAddrStr = QString("%1").arg(networkAddr, 4, 16, QChar('0')).toUpper();

        DBG_Printf(DBG_INFO, "  Lqi %s %s %s 0x%02X 0x%02X 0x%02X 0x%02X\n",
                qPrintable( panIdStr ),
                qPrintable( extAddrStr ),
                qPrintable( networkAddrStr ),
                tmpByteDevTypeRxRel,
                permitJoin,
                depth,
                lqiLinkQuality);

        quint8 deviceType;      // 0000 00xx 0=CO, 1=RD, 2=ED, 3=unknown
        quint8 rxOnWhenIdle;    // 0000 xx00 0=no, 1=yes, 2=unknown
        quint8 relationship;    // 0xxx 0000 parent/child/sibling/none/previous child

        deviceType = tmpByteDevTypeRxRel & 0x03;
        rxOnWhenIdle = (tmpByteDevTypeRxRel >> 2) & 0x03;
        relationship = (tmpByteDevTypeRxRel >> 4) & 0x07;

        QString deviceTypeStr;
        if (deviceType == 0x00)
        {
            deviceTypeStr = "CO";
        }
        else if (deviceType == 0x01)
        {
            deviceTypeStr = "RD";
        }
        else if (deviceType == 0x02)
        {
            deviceTypeStr = "ED";
        }
        else if (deviceType == 0x03)
        {
            deviceTypeStr = "unknown";
        }
        QString relationshipStr;
        if (relationship == 0x00)
        {
            relationshipStr = "parent";
        }
        else if (relationship == 0x01)
        {
            relationshipStr = "child";
        }
        else if (relationship == 0x02)
        {
            relationshipStr = "sibling";
        }
        else if (relationship == 0x03)
        {
            relationshipStr = "none";
        }
        else if (relationship == 0x04)
        {
            relationshipStr = "previous_child";
        }

        DBG_Printf(DBG_INFO, "  Lqi deviceType = %s, rxOnWhenIdle = %d, relationship = %s, permitJoin = %d\n",
                qPrintable(deviceTypeStr),
                rxOnWhenIdle,
                qPrintable(relationshipStr),
                permitJoin);
        DBG_Printf(DBG_INFO, "  Lqi tableIndex = %d, depth = %d, lqiLinkQuality = %d\n",
                tableIndex,
                depth,
                lqiLinkQuality);

        insertDb(srcAddr, tableIndex, neighborTableEntries, panIdStr,
                extAddrStr, networkAddrStr, deviceTypeStr, rxOnWhenIdle,
                relationshipStr, permitJoin, depth, lqiLinkQuality);

        tableCount++;
        tableIndex++;
    }

    rc = sqlite3_close(db);

    db = nullptr;
}

/*! Create database and tables lqi, lqi_history
 *     and triggers lqi_history_trigger, lqi_history_cleanup_trigger
 *
   CREATE TABLE IF NOT EXISTS lqi
            (
              srcAddr TEXT,
              tableIndex INTEGER,
              tableEntries INTEGER,
              neighborExtPanId TEXT,
              neighborExtAddr TEXT,
              neighborNwkAddr TEXT,
              deviceType TEXT,
              rxOnWhenIdle INTEGER,
              relationship TEXT,
              permitJoin INTEGER,
              depth INTEGER,
              lqiLinkQuality INTEGER,
              timestamp TEXT,
              PRIMARY KEY (srcAddr, tableIndex)
            );

  CREATE TABLE IF NOT EXISTS lqi_history
            (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              srcAddr TEXT,
              tableIndex INTEGER,
              tableEntries INTEGER,
              neighborExtPanId TEXT,
              neighborExtAddr TEXT,
              neighborNwkAddr TEXT,
              deviceType TEXT,
              rxOnWhenIdle INTEGER,
              relationship TEXT,
              permitJoin INTEGER,
              depth INTEGER,
              lqiLinkQuality INTEGER,
              timestamp TEXT
            );


  // trigger add old entry to lqi_history table on update

  CREATE TRIGGER IF NOT EXISTS lqi_history_trigger
            AFTER UPDATE ON lqi
            WHEN old.neighborExtAddr <> new.neighborExtAddr
            OR old.neighborNwkAddr <> new.neighborNwkAddr
            BEGIN
              INSERT INTO lqi_history (
              srcAddr,
              tableIndex,
              tableEntries,
              neighborExtPanId,
              neighborExtAddr,
              neighborNwkAddr,
              deviceType,
              rxOnWhenIdle,
              relationship,
              permitJoin,
              depth,
              lqiLinkQuality,
              timestamp
              )
            VALUES
              (
              old.srcAddr,
              old.tableIndex,
              old.tableEntries,
              old.neighborExtPanId,
              old.neighborExtAddr,
              old.neighborNwkAddr,
              old.deviceType,
              old.rxOnWhenIdle,
              old.relationship,
              old.permitJoin,
              old.depth,
              old.lqiLinkQuality,
              old.timestamp
              );
            END;


  // trigger delete oldest id from history when more than 5 entries with same nwkAddr in neighborTable

  CREATE TRIGGER IF NOT EXISTS lqi_history_cleanup_trigger
            AFTER UPDATE ON lqi
            WHEN
              (SELECT count(*) FROM lqi_history
                  WHERE srcAddr = new.srcAddr
                    AND neighborNwkAddr = new.neighborNwkAddr
              ) > 4
            BEGIN
              DELETE FROM lqi_history
              WHERE id = (
                SELECT min(id)
                FROM lqi_history
                WHERE srcAddr = new.srcAddr
                AND neighborNwkAddr = new.neighborNwkAddr
              );
            END;

 *
 */
void LqiPlugin::initDb()
{
    int rc;
    char *errmsg = nullptr;

    rc = sqlite3_open(qPrintable(sqliteDatabaseName), &db);

    if (rc != SQLITE_OK) {
        // failed
        DBG_Printf(DBG_ERROR, "Can't open database: %s\n", sqlite3_errmsg(db));
        db = nullptr;
        return;
    }

    const char *sql1 =  "CREATE TABLE IF NOT EXISTS lqi "
            "("
            "  srcAddr TEXT,"
            "  tableIndex INTEGER,"
            "  tableEntries INTEGER,"
            "  neighborExtPanId TEXT,"
            "  neighborExtAddr TEXT,"
            "  neighborNwkAddr TEXT,"
            "  deviceType TEXT,"
            "  rxOnWhenIdle INTEGER,"
            "  relationship TEXT,"
            "  permitJoin INTEGER,"
            "  depth INTEGER,"
            "  lqiLinkQuality INTEGER,"
            "  timestamp TEXT,"
            "  PRIMARY KEY (srcAddr, tableIndex)"
            ")";

    rc = sqlite3_exec(db, sql1, nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql1, errmsg, rc);
            sqlite3_free(errmsg);
        }
    }

    const char *sql2 =  "CREATE TABLE IF NOT EXISTS lqi_history "
            "("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  srcAddr TEXT,"
            "  tableIndex INTEGER,"
            "  tableEntries INTEGER,"
            "  neighborExtPanId TEXT,"
            "  neighborExtAddr TEXT,"
            "  neighborNwkAddr TEXT,"
            "  deviceType TEXT,"
            "  rxOnWhenIdle INTEGER,"
            "  relationship TEXT,"
            "  permitJoin INTEGER,"
            "  depth INTEGER,"
            "  lqiLinkQuality INTEGER,"
            "  timestamp TEXT"
            ")";

    rc = sqlite3_exec(db, sql2, nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql2, errmsg, rc);
            sqlite3_free(errmsg);
        }
    }

    const char *sql3 =  "CREATE TRIGGER IF NOT EXISTS lqi_history_trigger "
            "AFTER UPDATE ON lqi "
            "WHEN old.neighborExtAddr <> new.neighborExtAddr "
            "OR old.neighborNwkAddr <> new.neighborNwkAddr "
            "BEGIN "
            "  INSERT INTO lqi_history ("
            "  srcAddr,"
            "  tableIndex,"
            "  tableEntries,"
            "  neighborExtPanId,"
            "  neighborExtAddr,"
            "  neighborNwkAddr,"
            "  deviceType,"
            "  rxOnWhenIdle,"
            "  relationship,"
            "  permitJoin,"
            "  depth,"
            "  lqiLinkQuality,"
            "  timestamp"
            "  ) "
            "VALUES "
            "  ("
            "  old.srcAddr,"
            "  old.tableIndex,"
            "  old.tableEntries,"
            "  old.neighborExtPanId,"
            "  old.neighborExtAddr,"
            "  old.neighborNwkAddr,"
            "  old.deviceType,"
            "  old.rxOnWhenIdle,"
            "  old.relationship,"
            "  old.permitJoin,"
            "  old.depth,"
            "  old.lqiLinkQuality,"
            "  old.timestamp"
            "  ); "
            "END;";

    rc = sqlite3_exec(db, sql3, nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql3, errmsg, rc);
            sqlite3_free(errmsg);
        }
    }

    const char *sql4 = "CREATE TRIGGER IF NOT EXISTS lqi_history_cleanup_trigger"
            "            AFTER UPDATE ON lqi"
            "            WHEN"
            "              (SELECT count(*) FROM lqi_history"
            "                  WHERE srcAddr = new.srcAddr"
            "                    AND neighborNwkAddr = new.neighborNwkAddr"
            "              ) > 4"
            "            BEGIN"
            "              DELETE FROM lqi_history"
            "              WHERE id = ("
            "                SELECT min(id)"
            "                FROM lqi_history"
            "                WHERE srcAddr = new.srcAddr"
            "                AND neighborNwkAddr = new.neighborNwkAddr"
            "              );"
            "            END;";

    rc = sqlite3_exec(db, sql4, nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql4, errmsg, rc);
            sqlite3_free(errmsg);
        }
    }
    rc = sqlite3_close(db);

    db = nullptr;

}

/*! insert or update lqi values into database
 *
 */
void LqiPlugin::insertDb(QString &srcAddr, int tableIndex, int tableEntries, QString &neighborExtPanId,
        QString &neighborExtAddr, QString &neighborNwkAddr, QString &deviceType, int rxOnWhenIdle,
        QString &relationship, int permitJoin,  int depth, int lqiLinkQuality)
{
    QString timestamp = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");

    QString sql1 = QString("UPDATE lqi "
            "SET "
            "  srcAddr = '%1', "
            "  tableIndex=%2, "
            "  tableEntries = %3, "
            "  neighborExtPanId = '%4',"
            "  neighborExtAddr = '%5',"
            "  neighborNwkAddr  = '%6', "
            "  deviceType = '%7', "
            "  rxOnWhenIdle = %8, "
            "  relationship = '%9', "
            "  permitJoin = %10, "
            "  depth = %11,"
            "  lqiLinkQuality = %12, "
            "  timestamp = '%13' "
            "WHERE "
            "srcAddr = '%1' AND tableIndex = %2;")
                    .arg(srcAddr)
                    .arg(tableIndex)
                    .arg(tableEntries)
                    .arg(neighborExtPanId)
                    .arg(neighborExtAddr)
                    .arg(neighborNwkAddr)
                    .arg(deviceType)
                    .arg(rxOnWhenIdle)
                    .arg(relationship)
                    .arg(permitJoin)
                    .arg(depth)
                    .arg(lqiLinkQuality)
                    .arg(timestamp);

    QString sql2 = QString("INSERT OR IGNORE INTO lqi"
            "("
            "  srcAddr, "
            "  tableIndex, "
            "  tableEntries, "
            "  neighborExtPanId, "
            "  neighborExtAddr, "
            "  neighborNwkAddr, "
            "  deviceType, "
            "  rxOnWhenIdle, "
            "  relationship, "
            "  permitJoin, "
            "  depth, "
            "  lqiLinkQuality,  "
            "  timestamp  "
            ") "
            "values "
            "("
            "  '%1', %2, %3, '%4', '%5', '%6', '%7', %8, '%9', %10, %11, %12, '%13'"
            ");")
                    .arg(srcAddr)
                    .arg(tableIndex)
                    .arg(tableEntries)
                    .arg(neighborExtPanId)
                    .arg(neighborExtAddr)
                    .arg(neighborNwkAddr)
                    .arg(deviceType)
                    .arg(rxOnWhenIdle)
                    .arg(relationship)
                    .arg(permitJoin)
                    .arg(depth)
                    .arg(lqiLinkQuality)
                    .arg(timestamp);

    int rc;
    char *errmsg = nullptr;

    // Update if already exists
    rc = sqlite3_exec(db, qPrintable(sql1), nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql1, errmsg, rc);
            sqlite3_free(errmsg);
        }

        rc = sqlite3_close(db);
        db = nullptr;

        return;
    }

    // INSERT if not already exists otherwise IGNORE
    rc = sqlite3_exec(db, qPrintable(sql2), nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql2, errmsg, rc);
            sqlite3_free(errmsg);
        }
    }
}

/*! deCONZ will ask this plugin which features are supported.
    \param feature - feature to be checked
    \return true if supported
 */
bool LqiPlugin::hasFeature(Features feature) {
    switch (feature)
    {
    default:
        break;
    }

    return false;
}

/*! Returns the name of this plugin.
 */
const char *LqiPlugin::name() {
    return "deCONZ Lqi Plugin";
}

