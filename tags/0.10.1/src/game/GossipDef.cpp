/* 
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "QuestDef.h"
#include "GossipDef.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Util.h"

GossipMenu::GossipMenu()
{
    m_gItems.reserve(16);                                   // can be set for max from most often sizes to speedup push_back and less memory use
}

GossipMenu::~GossipMenu()
{
    ClearMenu();
}

void GossipMenu::AddMenuItem(uint8 Icon, std::string Message, uint32 dtSender, uint32 dtAction, std::string BoxMessage, uint32 BoxMoney, bool Coded)
{
    ASSERT( m_gItems.size() <= GOSSIP_MAX_MENU_ITEMS  );

    GossipMenuItem gItem;

    gItem.m_gIcon       = Icon;
    gItem.m_gMessage    = Message;
    gItem.m_gCoded      = Coded;
    gItem.m_gSender     = dtSender;
    gItem.m_gAction     = dtAction;
    gItem.m_gBoxMessage = BoxMessage;
    gItem.m_gBoxMoney   = BoxMoney;

    m_gItems.push_back(gItem);
}

void GossipMenu::AddMenuItem(uint8 Icon, std::string Message, bool Coded)
{
    AddMenuItem( Icon, Message, 0, 0, "", 0, Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""),Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, std::string(BoxMessage ? BoxMessage : ""), BoxMoney, Coded);
}

uint32 GossipMenu::MenuItemSender( unsigned int ItemId )
{
    if ( ItemId >= m_gItems.size() ) return 0;

    return m_gItems[ ItemId ].m_gSender;
}

uint32 GossipMenu::MenuItemAction( unsigned int ItemId )
{
    if ( ItemId >= m_gItems.size() ) return 0;

    return m_gItems[ ItemId ].m_gAction;
}

bool GossipMenu::MenuItemCoded( unsigned int ItemId )
{
    if ( ItemId >= m_gItems.size() ) return 0;

    return m_gItems[ ItemId ].m_gCoded;
}

void GossipMenu::ClearMenu()
{
    m_gItems.clear();
}

PlayerMenu::PlayerMenu( WorldSession *Session )
{
    pGossipMenu = new GossipMenu();
    pQuestMenu  = new QuestMenu();
    pSession    = Session;
}

PlayerMenu::~PlayerMenu()
{
    delete pGossipMenu;
    delete pQuestMenu;
}

void PlayerMenu::ClearMenus()
{
    pGossipMenu->ClearMenu();
    pQuestMenu->ClearMenu();
}

uint32 PlayerMenu::GossipOptionSender( unsigned int Selection )
{
    return pGossipMenu->MenuItemSender( Selection );
}

uint32 PlayerMenu::GossipOptionAction( unsigned int Selection )
{
    return pGossipMenu->MenuItemAction( Selection );
}

bool PlayerMenu::GossipOptionCoded( unsigned int Selection )
{
    return pGossipMenu->MenuItemCoded( Selection );
}

void PlayerMenu::SendGossipMenu( uint32 TitleTextId, uint64 npcGUID )
{
    WorldPacket data( SMSG_GOSSIP_MESSAGE, (100) );         // guess size
    data << npcGUID;
    data << uint32( TitleTextId );
    data << uint32( pGossipMenu->MenuItemCount() );

    for ( unsigned int iI = 0; iI < pGossipMenu->MenuItemCount(); iI++ )
    {
        GossipMenuItem const& gItem = pGossipMenu->GetItem(iI);
        data << uint32( iI );
        data << uint8( gItem.m_gIcon );
        // icons:
        // 0 unlearn talents/misc
        // 1 trader
        // 2 taxi
        // 3 trainer
        // 9 BG/arena
        data << uint8( gItem.m_gCoded );                    // makes pop up box password
        data << uint32(gItem.m_gBoxMoney);                  // money required to open menu, 2.0.3
        data << gItem.m_gMessage;                           // text for gossip item
        data << gItem.m_gBoxMessage;                        // accept text (related to money) pop up box, 2.0.3
    }

    data << uint32( pQuestMenu->MenuItemCount() );

    for ( uint16 iI = 0; iI < pQuestMenu->MenuItemCount(); iI++ )
    {
        QuestMenuItem const& qItem = pQuestMenu->GetItem(iI);
        uint32 questID = qItem.m_qId;
        Quest const* pQuest = objmgr.GetQuestTemplate(questID);

        data << questID;
        data << uint32( qItem.m_qIcon );
        data << uint32( pQuest ? pQuest->GetQuestLevel() : 0 );
        std::string Title = pQuest->GetTitle();

        int loc_idx = pSession->GetSessionLocaleIndex();
        if (loc_idx >= 0)
        {
            QuestLocale const *ql = objmgr.GetQuestLocale(questID);
            if (ql)
            {
                if (ql->Title.size() > loc_idx && !ql->Title[loc_idx].empty())
                    Title=ql->Title[loc_idx];
            }
        }
        data << Title;
    }

    pSession->SendPacket( &data );
    //sLog.outDebug( "WORLD: Sent SMSG_GOSSIP_MESSAGE NPCGuid=%u",GUID_LOPART(npcGUID) );
}

void PlayerMenu::CloseGossip()
{
    WorldPacket data( SMSG_GOSSIP_COMPLETE, 0 );
    pSession->SendPacket( &data );

    //sLog.outDebug( "WORLD: Sent SMSG_GOSSIP_COMPLETE" );
}

void PlayerMenu::SendPointOfInterest( float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const * locName )
{
    WorldPacket data( SMSG_GOSSIP_POI, (4+4+4+4+4+10) );    // guess size
    data << Flags;
    data << X << Y;
    data << uint32(Icon);
    data << uint32(Data);
    data << locName;

    pSession->SendPacket( &data );
    //sLog.outDebug("WORLD: Sent SMSG_GOSSIP_POI");
}

void PlayerMenu::SendTalking( uint32 textID )
{
    GossipText *pGossip;
    std::string GossipStr;

    pGossip = objmgr.GetGossipText(textID);

    WorldPacket data( SMSG_NPC_TEXT_UPDATE, 100 );          // guess size
    data << textID;

    if (!pGossip)
    {
        data << uint32( 0 );
        data << "Greetings $N";
        data << "Greetings $N";
    }
    else
    {
        std::string Text_0[8],Text_1[8];
        for (int i=0;i<8;i++)
        {
            Text_0[i]=pGossip->Options[i].Text_0;
            Text_1[i]=pGossip->Options[i].Text_1;
        }
        int loc_idx = pSession->GetSessionLocaleIndex();
        if (loc_idx >= 0)
        {
            NpcTextLocale const *nl = objmgr.GetNpcTextLocale(textID);
            if (nl)
            {
                for (int i=0;i<8;i++)
                {
                    if (nl->Text_0[i].size() > loc_idx && !nl->Text_0[i][loc_idx].empty())
                        Text_0[i]=nl->Text_0[i][loc_idx];
                    if (nl->Text_1[i].size() > loc_idx && !nl->Text_1[i][loc_idx].empty())
                        Text_1[i]=nl->Text_1[i][loc_idx];
                }
            }
        }
        for (int i=0; i<8; i++)
        {
            data << pGossip->Options[i].Probability;

            if ( Text_0[i].empty() )
                data << Text_1[i];
            else
                data << Text_0[i];

            if ( Text_1[i].empty() )
                data << Text_0[i];
            else
                data << Text_1[i];

            data << pGossip->Options[i].Language;

            data << pGossip->Options[i].Emotes[0]._Delay;
            data << pGossip->Options[i].Emotes[0]._Emote;

            data << pGossip->Options[i].Emotes[1]._Delay;
            data << pGossip->Options[i].Emotes[1]._Emote;

            data << pGossip->Options[i].Emotes[2]._Delay;
            data << pGossip->Options[i].Emotes[2]._Emote;
        }
    }
    pSession->SendPacket( &data );

    sLog.outDebug(  "WORLD: Sent SMSG_NPC_TEXT_UPDATE " );
}

void PlayerMenu::SendTalking( char const * title, char const * text )
{
    WorldPacket data( SMSG_NPC_TEXT_UPDATE, 50 );           // guess size
    data << uint32( 0 );
    data << uint32( 0 );
    data << title;
    data << text;

    pSession->SendPacket( &data );

    sLog.outDebug( "WORLD: Sent SMSG_NPC_TEXT_UPDATE " );
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

QuestMenu::QuestMenu()
{
    m_qItems.reserve(16);                                   // can be set for max from most often sizes to speedup push_back and less memory use
}

QuestMenu::~QuestMenu()
{
    ClearMenu();
}

void QuestMenu::AddMenuItem( uint32 QuestId, uint8 Icon)
{
    Quest const* qinfo = objmgr.GetQuestTemplate(QuestId);
    if (!qinfo) return;

    ASSERT( m_qItems.size() <= GOSSIP_MAX_MENU_ITEMS  );

    QuestMenuItem qItem;

    qItem.m_qId        = QuestId;
    qItem.m_qIcon      = Icon;

    m_qItems.push_back(qItem);
}

bool QuestMenu::HasItem( uint32 questid )
{
    for (QuestMenuItemList::iterator i = m_qItems.begin(); i != m_qItems.end(); i++)
    {
        if(i->m_qId==questid)
        {
            return true;
        }
    }
    return false;
}

void QuestMenu::ClearMenu()
{
    m_qItems.clear();
}

void PlayerMenu::SendQuestGiverQuestList( QEmote eEmote, std::string Title, uint64 npcGUID )
{
    WorldPacket data( SMSG_QUESTGIVER_QUEST_LIST, 100 );    // guess size
    data << uint64(npcGUID);
    data << Title;
    data << uint32(eEmote._Delay );                         // player emote
    data << uint32(eEmote._Emote );                         // NPC emote
    data << uint8 ( pQuestMenu->MenuItemCount() );

    for ( uint16 iI = 0; iI < pQuestMenu->MenuItemCount(); iI++ )
    {
        QuestMenuItem qmi=pQuestMenu->GetItem(iI);
        uint32 questID = qmi.m_qId;
        Quest const *pQuest = objmgr.GetQuestTemplate(questID);

        data << uint32(questID);
        data << uint32(qmi.m_qIcon);
        data << uint32(pQuest ? pQuest->GetQuestLevel() : 0);
        data << ( pQuest ? pQuest->GetTitle() : "" );
    }
    pSession->SendPacket( &data );
    //uint32 fqid=pQuestMenu->GetItem(0).m_qId;
    //sLog.outDebug( "WORLD: Sent SMSG_QUESTGIVER_QUEST_LIST NPC Guid=%u, questid-0=%u",npcGUID,fqid);
}

void PlayerMenu::SendQuestGiverStatus( uint32 questStatus, uint64 npcGUID )
{
    WorldPacket data( SMSG_QUESTGIVER_STATUS, 12 );
    data << uint64(npcGUID);
    data << uint32(questStatus);

    pSession->SendPacket( &data );
    //sLog.outDebug( "WORLD: Sent SMSG_QUESTGIVER_STATUS NPC Guid=%u, status=%u",GUID_LOPART(npcGUID),questStatus);
}

void PlayerMenu::SendQuestGiverQuestDetails( Quest const *pQuest, uint64 npcGUID, bool ActivateAccept )
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_DETAILS, 100);   // guess size

    std::string Title      = pQuest->GetTitle();
    std::string Details    = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();
    std::string EndText    = pQuest->GetEndText();

    int loc_idx = pSession->GetSessionLocaleIndex();
    if (loc_idx >= 0)
    {
        QuestLocale const *ql = objmgr.GetQuestLocale(pQuest->GetQuestId());
        if (ql)
        {
            if (ql->Title.size() > loc_idx && !ql->Title[loc_idx].empty())
                Title=ql->Title[loc_idx];
            if (ql->Details.size() > loc_idx && !ql->Details[loc_idx].empty())
                Details=ql->Details[loc_idx];
            if (ql->Objectives.size() > loc_idx && !ql->Objectives[loc_idx].empty())
                Objectives=ql->Objectives[loc_idx];
        }
    }

    data << uint64(npcGUID);
    data << uint32(pQuest->GetQuestId());
    data << Title << Details << Objectives;
    data << uint32(ActivateAccept);
    data << uint32(pQuest->GetSuggestedPlayers());

    ItemPrototype const* IProto;

    data << uint32(pQuest->GetRewChoiceItemsCount());
    for (uint32 i=0; i < QUEST_REWARD_CHOICES_COUNT; i++)
    {
        if ( !pQuest->RewChoiceItemId[i] ) continue;
        data << uint32(pQuest->RewChoiceItemId[i]);
        data << uint32(pQuest->RewChoiceItemCount[i]);
        IProto = objmgr.GetItemPrototype(pQuest->RewChoiceItemId[i]);
        if ( IProto )
            data << uint32(IProto->DisplayInfoID);
        else
            data << uint32( 0x00 );
    }

    data << uint32(pQuest->GetRewItemsCount());
    for (uint32 i=0; i < QUEST_REWARDS_COUNT; i++)
    {
        if ( !pQuest->RewItemId[i] ) continue;
        data << uint32(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemCount[i]);
        IProto = objmgr.GetItemPrototype(pQuest->RewItemId[i]);
        if ( IProto )
            data << uint32(IProto->DisplayInfoID);
        else
            data << uint32(0);
    }

    data << uint32(pQuest->GetRewOrReqMoney());

    data << uint32(0);                                      // Honor points reward, not impelmented, and possible depricated

    // check if RewSpell is teaching another spell
    if(pQuest->GetRewSpell())
    {
        SpellEntry const *rewspell = sSpellStore.LookupEntry(pQuest->GetRewSpell());
        if(rewspell && rewspell->Effect[0] == SPELL_EFFECT_LEARN_SPELL)
            data << uint32(rewspell->EffectTriggerSpell[0]);
        else
            data << uint32(0);
    }
    else
        data << uint32(0);                                  // reward spell

    data << uint32(pQuest->GetRewSpell());                  // casted spell

    data << uint32(QUEST_EMOTE_COUNT);
    for (uint32 i=0; i < QUEST_EMOTE_COUNT; i++)
    {
        data << uint32(pQuest->DetailsEmote[i]);
        data << uint32(0);                                  // DetailsEmoteDelay
    }
    pSession->SendPacket( &data );

    //sLog.outDebug("WORLD: Sent SMSG_QUESTGIVER_QUEST_DETAILS NPCGuid=%u, questid=%u",GUID_LOPART(npcGUID),pQuest->GetQuestId());
}

void PlayerMenu::SendQuestQueryResponse( Quest const *pQuest )
{
    std::string Title,Details,Objectives,EndText;
    std::string ObjectiveText[QUEST_OBJECTIVES_COUNT];
    Title = pQuest->GetTitle();
    Details = pQuest->GetDetails();
    Objectives = pQuest->GetObjectives();
    EndText = pQuest->GetEndText();
    for (int i=0;i<QUEST_OBJECTIVES_COUNT;i++)
        ObjectiveText[i]=pQuest->ObjectiveText[i];

    int loc_idx = pSession->GetSessionLocaleIndex();
    if (loc_idx >= 0)
    {
        QuestLocale const *ql = objmgr.GetQuestLocale(pQuest->GetQuestId());
        if (ql)
        {
            if (ql->Title.size() > loc_idx && !ql->Title[loc_idx].empty())
                Title=ql->Title[loc_idx];
            if (ql->Details.size() > loc_idx && !ql->Details[loc_idx].empty())
                Details=ql->Details[loc_idx];
            if (ql->Objectives.size() > loc_idx && !ql->Objectives[loc_idx].empty())
                Objectives=ql->Objectives[loc_idx];

            for (int i=0;i<QUEST_OBJECTIVES_COUNT;i++)
                if (ql->ObjectiveText[i].size() > loc_idx && !ql->ObjectiveText[i][loc_idx].empty())
                    ObjectiveText[i]=ql->ObjectiveText[i][loc_idx];
        }
    }

    WorldPacket data( SMSG_QUEST_QUERY_RESPONSE, 100 );     // guess size

    data << uint32(pQuest->GetQuestId());
    data << uint32(pQuest->GetMinLevel());                  // not MinLevel. Accepted values: 0, 1 or 2 Possible theory for future dev: 0==cannot in quest log, 1==can in quest log session only(removed on log out), 2==can in quest log always (save to db)
    data << uint32(pQuest->GetQuestLevel());
    data << uint32(pQuest->GetZoneOrSort());

    data << uint32(pQuest->GetType());
    data << uint32(pQuest->GetSuggestedPlayers());

    data << uint32(pQuest->GetRepObjectiveFaction());       // shown in quest log as part of quest objective
    data << uint32(pQuest->GetRepObjectiveValue());         // shown in quest log as part of quest objective

    data << uint32(0);                                      // unknown, not RequiredMaxRepFaction
    data << uint32(0);                                      // unknown, not RequiredMaxRepValue

    data << uint32(pQuest->GetNextQuestInChain());
    data << uint32(pQuest->GetRewOrReqMoney());
    data << uint32(pQuest->GetRewMoneyMaxLevel());          // used in XP claculation at client

    // check if RewSpell is teaching another spell
    if(pQuest->GetRewSpell())
    {
        SpellEntry const *rewspell = sSpellStore.LookupEntry(pQuest->GetRewSpell());
        if(rewspell && rewspell->Effect[0] == SPELL_EFFECT_LEARN_SPELL)
            data << uint32(rewspell->EffectTriggerSpell[0]);
        else
            data << uint32(0);
    }
    else
        data << uint32(0);

    data << uint32(pQuest->GetRewSpell());                  // casted spell
    data << uint32(0);                                      // Honor points reward, not impelmented, and possible depricated
    data << uint32(pQuest->GetSrcItemId());
    data << uint32(pQuest->GetFlags() & 0xFFFF);

    int iI;

    for (iI = 0; iI < QUEST_REWARDS_COUNT; iI++)
    {
        data << uint32(pQuest->RewItemId[iI]);
        data << uint32(pQuest->RewItemCount[iI]);
    }
    for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; iI++)
    {
        data << uint32(pQuest->RewChoiceItemId[iI]);
        data << uint32(pQuest->RewChoiceItemCount[iI]);
    }

    data << pQuest->GetPointMapId();
    data << pQuest->GetPointX();
    data << pQuest->GetPointY();
    data << pQuest->GetPointOpt();

    data << Title;
    data << Objectives;
    data << Details;
    data << EndText;

    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; iI++)
    {
        if (pQuest->ReqCreatureOrGOId[iI] < 0)
        {
            // client expected gameobject template id in form (id|0x80000000)
            data << uint32((pQuest->ReqCreatureOrGOId[iI]*(-1))|0x80000000);
        }
        else
        {
            data << uint32(pQuest->ReqCreatureOrGOId[iI]);
        }
        data << uint32(pQuest->ReqCreatureOrGOCount[iI]);
        data << uint32(pQuest->ReqItemId[iI]);
        data << uint32(pQuest->ReqItemCount[iI]);
    }

    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; iI++)
        data << ObjectiveText[iI];

    pSession->SendPacket( &data );
    //sLog.outDebug( "WORLD: Sent SMSG_QUEST_QUERY_RESPONSE questid=%u",pQuest->GetQuestId() );
}

void PlayerMenu::SendQuestGiverOfferReward( Quest const* pQuest, uint64 npcGUID, bool EnbleNext )
{
    std::string Title = pQuest->GetTitle();
    std::string OfferRewardText = pQuest->GetOfferRewardText();

    int loc_idx = pSession->GetSessionLocaleIndex();
    if (loc_idx >= 0)
    {
        QuestLocale const *ql = objmgr.GetQuestLocale(pQuest->GetQuestId());
        if (ql)
        {
            if (ql->Title.size() > loc_idx && !ql->Title[loc_idx].empty())
                Title=ql->Title[loc_idx];
            if (ql->OfferRewardText.size() > loc_idx && !ql->OfferRewardText[loc_idx].empty())
                OfferRewardText=ql->OfferRewardText[loc_idx];
        }
    }

    WorldPacket data( SMSG_QUESTGIVER_OFFER_REWARD, 50 );   // guess size

    data << npcGUID;
    data << pQuest->GetQuestId();
    data << Title;
    data << OfferRewardText;

    data << uint32( EnbleNext );
    data << uint32(0);                                      // unk

    uint32 EmoteCount = 0;
    for (uint32 i = 0; i < QUEST_EMOTE_COUNT; i++)
    {
        if(pQuest->OfferRewardEmote[i] <= 0)
            break;
        ++EmoteCount;
    }

    data << EmoteCount;                                     // Emote Count
    for (uint32 i = 0; i < EmoteCount; i++)
    {
        data << uint32(0);                                  // Delay Emote
        data << pQuest->OfferRewardEmote[i];
    }

    ItemPrototype const *pItem;

    data << uint32(pQuest->GetRewChoiceItemsCount());
    for (uint32 i=0; i < pQuest->GetRewChoiceItemsCount(); i++)
    {
        pItem = objmgr.GetItemPrototype( pQuest->RewChoiceItemId[i] );

        data << uint32(pQuest->RewChoiceItemId[i]);
        data << uint32(pQuest->RewChoiceItemCount[i]);

        if ( pItem )
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    data << uint32(pQuest->GetRewItemsCount());
    for (uint16 i=0; i < pQuest->GetRewItemsCount(); i++)
    {
        pItem = objmgr.GetItemPrototype(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemId[i]);
        data << uint32(pQuest->RewItemCount[i]);

        if ( pItem )
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    data << uint32(pQuest->GetRewOrReqMoney());
    data << uint32(0x00);                                   // new 2.3.0. Honor points
    data << uint32(0x08);

    // check if RewSpell is teaching another spell
    if(pQuest->GetRewSpell())
    {
        SpellEntry const *rewspell = sSpellStore.LookupEntry(pQuest->GetRewSpell());
        if(rewspell && rewspell->Effect[0] == SPELL_EFFECT_LEARN_SPELL)
            data << uint32(rewspell->EffectTriggerSpell[0]);
        else
            data << uint32(0);
    }
    else
        data << uint32(0);

    data << uint32(pQuest->GetRewSpell());                  // casted spell
    data << uint32(0);                                      // Honor points reward, not impelmented, and possible depricated

    pSession->SendPacket( &data );
    //sLog.outDebug( "WORLD: Sent SMSG_QUESTGIVER_OFFER_REWARD NPCGuid=%u, questid=%u",GUID_LOPART(npcGUID),quest_id );
}

void PlayerMenu::SendQuestGiverRequestItems( Quest const *pQuest, uint64 npcGUID, bool Completable, bool CloseOnCancel )
{
    // We can always call to RequestItems, but this packet only goes out if there are actually
    // items.  Otherwise, we'll skip straight to the OfferReward

    // We may wish a better check, perhaps checking the real quest requirements
    if (pQuest->GetRequestItemsText().size() == 0)
    {
        SendQuestGiverOfferReward(pQuest, npcGUID, true);
        return;
    }

    std::string Title,RequestItemsText;
    Title = pQuest->GetTitle();
    RequestItemsText = pQuest->GetRequestItemsText();

    int loc_idx = pSession->GetSessionLocaleIndex();
    if (loc_idx >= 0)
    {
        QuestLocale const *ql = objmgr.GetQuestLocale(pQuest->GetQuestId());
        if (ql)
        {
            if (ql->Title.size() > loc_idx && !ql->Title[loc_idx].empty())
                Title=ql->Title[loc_idx];
            if (ql->RequestItemsText.size() > loc_idx && !ql->RequestItemsText[loc_idx].empty())
                RequestItemsText=ql->RequestItemsText[loc_idx];
        }
    }

    WorldPacket data( SMSG_QUESTGIVER_REQUEST_ITEMS, 50 );  // guess size
    data << npcGUID;
    data << pQuest->GetQuestId();
    data << Title;
    data << RequestItemsText;

    data << uint32(0x00);                                   // unk

    if(Completable)
        data << pQuest->GetCompleteEmote();
    else
        data << pQuest->GetIncompleteEmote();

    // Close Window after cancel
    if (CloseOnCancel)
        data << uint32(0x01);
    else
        data << uint32(0x00);

    // Req Money
    data << uint32(pQuest->GetRewOrReqMoney() < 0 ? -pQuest->GetRewOrReqMoney() : 0);

    data << uint32(0x00);                                   // unk

    data << uint32( pQuest->GetReqItemsCount() );
    ItemPrototype const *pItem;
    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
    {
        if ( !pQuest->ReqItemId[i] ) continue;
        pItem = objmgr.GetItemPrototype(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemCount[i]);

        if ( pItem )
            data << uint32(pItem->DisplayInfoID);
        else
            data << uint32(0);
    }

    if ( !Completable )
        data << uint32(0x00);
    else
        data << uint32(0x03);

    data << uint32(0x04) << uint32(0x08) << uint32(0x10);

    pSession->SendPacket( &data );
    //sLog.outDebug( "WORLD: Sent SMSG_QUESTGIVER_REQUEST_ITEMS NPCGuid=%u, questid=%u",GUID_LOPART(npcGUID),pQuest->GetQuestId() );
}
