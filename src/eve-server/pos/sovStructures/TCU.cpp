
/**
 * @name TCU.cpp
 *   Class for Territorial Claim Units.
 *
 * @Author:         James
 * @date:   8 April 2021
 */

/*
 * POS__ERROR
 * POS__WARNING
 * POS__MESSAGE
 * POS__DUMP
 * POS__DEBUG
 * POS__DESTINY
 * POS__SLIMITEM
 * POS__TRACE
 */


#include "Client.h"
#include "EntityList.h"
#include "EVEServerConfig.h"
#include "planet/Planet.h"
#include "pos/sovStructures/TCU.h"
#include "system/Container.h"
#include "system/Damage.h"
#include "system/SystemBubble.h"
#include "system/SystemManager.h"

TCUSE::TCUSE(StructureItemRef structure, PyServiceMgr& services, SystemManager* system, const FactionData& fData)
: StructureSE(structure, services, system, fData),
m_pShieldSE(nullptr)
{
    m_structs.clear();
    m_tdata = EVEPOS::TCUData();
}

TCUSE::~TCUSE()
{
    SafeDelete(m_destiny);
}

void TCUSE::Init()
{
    StructureSE::Init();

    if (!m_db.GetTCUData(m_tdata, m_data)) {
        _log(SE__TRACE, "TCUSE %s(%u) has no saved data.  Initializing default set.", m_self->name(), m_self->itemID());
        // invalid data....init to 0 as this will only hit for currently-launching items (or errors)
        InitData();
    }

    // this OSE needs destiny.
    m_destiny = new DestinyManager(this);

    // if TCU anchored, tell planet this is the TCU
    if (m_data.state > EVEPOS::StructureStatus::Unanchored)
        m_planetSE->SetTCU(this);

    // set TCU in bubble
    if (m_bubble == nullptr)
        assert(0);
    m_bubble->SetTCUSE(this);

}

void TCUSE::InitData() {
    // init base data first
    StructureSE::InitData();

    m_db.SaveTCUData(m_tdata, m_data);
}

void TCUSE::Scoop() {
    StructureSE::Scoop();
    m_planetSE->SetTCU(nullptr);
    m_tdata = EVEPOS::TCUData();
    m_self->ChangeSingleton(false);
    m_self->SaveItem();
}

void TCUSE::Process()
{
    StructureSE::Process();
}

/*
 * 556     anchoringDelay  1800000     NULL
 * 650     maxStructureDistance    50000   NULL
 * 676     unanchoringDelay    NULL    3600000
 * 677     onliningDelay   1800000     NULL
 * 680     shieldRadius    30000   NULL
 * 711     moonAnchorDistance  100000  NULL
 * 1214    posStructureControlDistanceMax  NULL    15000
 */

void TCUSE::SetOnline()
{
    //StructureSE::SetOnline();

    m_data.timestamp = GetFileTimeNow();
    m_self->SetFlag(flagStructureActive);
    m_data.state = EVEPOS::StructureStatus::Online;

    SendSlimUpdate();
    m_destiny->SendSpecialEffect(m_self->itemID(),m_self->itemID(),m_self->typeID(),0,0,"effects.StructureOnline",0,1,1,-1,0);

    m_db.UpdateBaseData(m_data);

}

void TCUSE::SetOffline()
{
    StructureSE::SetOffline();
}


void TCUSE::Online()
{
    StructureSE::Online();
}

void TCUSE::Reinforced()
{
    // something to do with SBUs should go here maybe?
}

void TCUSE::SetDeployFlags(int8 anchor/*0*/, int8 unanchor/*0*/, int8 online/*0*/, int8 offline/*0*/)
{
    m_tdata.anchor = anchor;
    m_tdata.unanchor = unanchor;
    m_tdata.online = online;
    m_tdata.offline = offline;

    m_db.UpdateTCUDeployFlags(m_data.itemID, m_tdata);
}

PyRep* TCUSE::GetDeployFlags()
{
    PyList* header = new PyList(4);
        header->SetItemString(0, "anchor");
        header->SetItemString(1, "unanchor");
        header->SetItemString(2, "online");
        header->SetItemString(3, "offline");
    PyList* line = new PyList(4);           // these are structure permissions for this tower
        line->SetItem(0, new PyInt(m_tdata.anchor));
        line->SetItem(1, new PyInt(m_tdata.unanchor));
        line->SetItem(2, new PyInt(m_tdata.online));
        line->SetItem(3, new PyInt(m_tdata.offline));

    PyDict* dict = new PyDict();
    dict->SetItemString("header", header);
    dict->SetItemString("line", line);

    return new PyObject("util.Row", dict);
}

PyDict* TCUSE::MakeSlimItem()
{
    _log(SE__SLIMITEM, "MakeSlimItem for TCUSE %u", m_self->itemID());
    _log(POS__SLIMITEM, "MakeSlimItem for TCUSE %u", m_self->itemID());

    PyDict *slim = new PyDict();
    slim->SetItemString("name",                     new PyString(m_self->itemName()));
    slim->SetItemString("itemID",                   new PyLong(m_self->itemID()));
    slim->SetItemString("typeID",                   new PyInt(m_self->typeID()));
    slim->SetItemString("ownerID",                  new PyInt(m_ownerID));
    slim->SetItemString("corpID",                   IsCorp(m_corpID) ? new PyInt(m_corpID) : PyStatic.NewNone());
    slim->SetItemString("allianceID",               IsAlliance(m_allyID) ? new PyInt(m_allyID) : PyStatic.NewNone());
    slim->SetItemString("warFactionID",             IsFaction(m_warID) ? new PyInt(m_warID) : PyStatic.NewNone());
    slim->SetItemString("posTimestamp",             new PyLong(m_data.timestamp));
    slim->SetItemString("posState",                 new PyInt(m_data.state));
    slim->SetItemString("incapacitated",            new PyInt((m_data.state == EVEPOS::StructureStatus::Incapacitated) ? 1 : 0));
    slim->SetItemString("posDelayTime",             new PyInt(m_delayTime));

    if (is_log_enabled(POS__SLIMITEM)) {
        _log( POS__SLIMITEM, "TCUSE::MakeSlimItem() - %s(%u)", GetName(), GetID());
        slim->Dump(POS__SLIMITEM, "     ");
    }
    return slim;
}