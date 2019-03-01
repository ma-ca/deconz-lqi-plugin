/*
 * deconz_lqi_plugin.h
 *
 *  Created on: 19.02.2019
 *      Author: ma-ca
 */

#ifndef DECONZ_LQI_PLUGIN_H_
#define DECONZ_LQI_PLUGIN_H_

#include <QObject>
#include <list>
#include <sqlite3.h>
#include <deconz.h>

#if DECONZ_LIB_VERSION < 0x010100
  #error "The basic aps plugin requires at least deCONZ library version 1.1.0."
#endif


/*! \class LqiPlugin
    Plugin to process ZCL Attributes from APS frames via
    APSDE-DATA.request, APSDE-DATA.confirm and APSDE-DATA.indication primitives.
    The plugin writes Mgt_Lqi_mgnt responses to a sqlite3 database

 */
class LqiPlugin : public QObject, public deCONZ::NodeInterface {
    Q_OBJECT
    Q_INTERFACES(deCONZ::NodeInterface)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "LqiPlugin")
#endif

public:
    explicit LqiPlugin(QObject *parent = 0);
    ~LqiPlugin();
    // node interface
    const char *name();
    bool hasFeature(Features feature);

public Q_SLOTS:
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);

public:
    void initDb();
    void insertDb(QString &srcAddr, int tableIndex, int tableEntries, QString &neighborExtPanId,
            QString &neighborExtAddr, QString &neighborNwkAddr, QString &deviceType, int rxOnWhenIdle,
            QString &relationship, int permitJoin,  int depth, int lqiLinkQuality);

    sqlite3 *db;

private:
    std::list<deCONZ::ApsDataRequest> m_apsReqQueue; //!< queue of active APS requests
    deCONZ::ApsController *m_apsCtrl; //!< pointer to ApsController instance
};



#endif /* DECONZ_LQI_PLUGIN_H_ */
