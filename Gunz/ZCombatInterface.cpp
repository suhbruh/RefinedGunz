#include "stdafx.h"

#include "ZGameClient.h"
#include "ZCombatInterface.h"
#include "ZGameInterface.h"
#include "ZInterfaceItem.h"
#include "ZInterfaceListener.h"
#include "ZApplication.h"
#include "ZCharacter.h"
#include "ZCharacterManager.h"
#include "RealSpace2.h"
#include "MComboBox.h"
#include "RTypes.h"
#include "ZScreenEffectManager.h"
#include "ZActionDef.h"
#include "ZEffectFlashBang.h"
#include "ZConfiguration.h"
#include "ZPost.h"
#include "ZWeaponScreenEffect.h"
#include "MemPool.h"
#include "ZMyInfo.h"
#include <algorithm>

#include "ZApplication.h"
#include "ZCombatQuestScreen.h"
#include "ZBmNumLabel.h"
#include "ZModule_QuestStatus.h"
#include "ZLocale.h"

#include "ZRuleDuel.h"
#include "ZInput.h"

using namespace std;


// CONSTANTS
#define BACKGROUND_COLOR1					0xff202020
#define BACKGROUND_COLOR2					0xff000000
#define BACKGROUND_COLOR_MYCHAR_DEATH_MATCH	MINT_ARGB(255*40/100,140,180,255)
#define BACKGROUND_COLOR_MYCHAR_RED_TEAM	MINT_ARGB(255*40/100,255,50,50)
#define BACKGROUND_COLOR_MYCHAR_BLUE_TEAM	MINT_ARGB(255*40/100,50,50,255)

#define BACKGROUND_COLOR_COMMANDER			MINT_ARGB(255*40/100,255,88,255)

#define TEXT_COLOR_TITLE			0xFFAAAAAA
#define TEXT_COLOR_DEATH_MATCH		0xfffff696
#define TEXT_COLOR_DEATH_MATCH_DEAD	0xff807b4b
#define TEXT_COLOR_BLUE_TEAM		0xff8080ff
#define TEXT_COLOR_BLUE_TEAM_DEAD	0xff606080
#define TEXT_COLOR_RED_TEAM			0xffff8080
#define TEXT_COLOR_RED_TEAM_DEAD	0xff806060
#define TEXT_COLOR_SPECTATOR		0xff808080
#define TEXT_COLOR_CLAN_NAME		0xffffffff

struct ZScoreBoardItem : public CMemPoolSm<ZScoreBoardItem>{
	MUID uidUID;
	char szName[64];
	char szClan[CLAN_NAME_LENGTH];
	int nDuelQueueIdx;
	int	nClanID;
	int nTeam;
	bool bDeath;
	int nExp;
	int nKills;
	int nDeaths;
	int nPing;
	bool bMyChar;
	bool bCommander;
	bool bGameRoomUser;

	MCOLOR SpColor;
	bool  bSpColor;

	ZScoreBoardItem( const MUID& _uidUID, char *_szName,char *_szClan,int _nTeam,bool _bDeath,int _nExp,int _nKills,int _nDeaths,int _nPing,bool _bMyChar,bool _bGameRoomUser, bool _bCommander = false)
	{
		uidUID=_uidUID;
		strcpy_safe(szName,_szName);
		strcpy_safe(szClan,_szClan);
		nTeam=_nTeam;
		bDeath=_bDeath;
		nExp=_nExp;
		nKills=_nKills;
		nDeaths=_nDeaths;
		nPing=_nPing;
		bMyChar = _bMyChar;
		bCommander = _bCommander;
		bSpColor = false;
		SpColor = MCOLOR(0,0,0);
		bGameRoomUser = _bGameRoomUser;
	}
	ZScoreBoardItem() {
		bSpColor = false;
		SpColor = MCOLOR(0,0,0);
	}

	void SetColor(MCOLOR c) { 
		SpColor = c;
		bSpColor = true;
	}

	MCOLOR GetColor() {
		return SpColor;
	}
};

ZCombatInterface::ZCombatInterface(const char* szName, MWidget* pParent, MListener* pListener)
: ZInterface(szName, pParent, pListener)
{
	m_nBullet = 0;
	m_nBulletAMagazine = 0;
	m_nMagazine = 0;
	memset(m_szItemName, 0, sizeof(m_szItemName));

//	m_pScoreBoard = NULL;
	m_pIDLResource = ZApplication::GetGameInterface()->GetIDLResource();

	m_bMenuVisible = false;
	
	m_bPickTarget = false;
	m_pLastItemDesc = NULL;
	
	m_bReserveFinish = false;
	
	m_pTargetLabel = NULL;
	m_szTargetName[0] = 0;

	m_nBulletImageIndex = 0;
	m_nMagazineImageIndex = 0;

	m_nReserveFinishTime = 0;

	m_pWeaponScreenEffect = NULL;

	m_pResultPanel=NULL;
	m_pResultPanel_Team = NULL;
	m_pResultLeft = NULL;
	m_pResultRight = NULL;

	m_pQuestScreen = NULL;

//	m_bKickPlayerListVisible = false;

	m_nClanIDRed = 0;
	m_nClanIDBlue = 0;
	m_szRedClanName[0] = 0;
	m_szBlueClanName[0] = 0;
}

ZCombatInterface::~ZCombatInterface()
{
	OnDestroy();	
}

bool ZCombatInterface::OnCreate()
{
	m_Observer.Create(ZApplication::GetGameInterface()->GetCamera(), 
					  ZApplication::GetGameInterface()->GetIDLResource());


	m_pTargetLabel = new MLabel("", this, this);
	m_pTargetLabel->SetTextColor(0xffff0000);
	m_pTargetLabel->SetSize(100, 30);

	ShowInfo(true);

	m_pResultBgImg = NULL;
	m_bDrawScoreBoard = false;

	EnableInputChat(false);

	m_Chat.Create( "CombatChatOutput",true);
	m_Chat.ShowOutput(ZGetConfiguration()->GetViewGameChat());
	m_Chat.m_pChattingOutput->ReleaseFocus();

	m_AdminMsg.Create( "CombatChatOutputAdmin",false);
	MFont* pFont = MFontManager::Get( "FONTb11b");
	m_AdminMsg.SetFont( pFont);
	m_AdminMsg.m_pChattingOutput->ReleaseFocus();

	if (ZGetMyInfo()->IsAdminGrade()) {
		MMatchObjCache* pCache = ZGetGameClient()->FindObjCache(ZGetMyUID());
		if (pCache && pCache->GetUGrade()==MMUG_EVENTMASTER && pCache->CheckFlag(MTD_PlayerFlags_AdminHide)) {
			ShowChatOutput(false);
		}
	}

	m_ppIcons[0]=MBitmapManager::Get("medal_A.tga");
	m_ppIcons[1]=MBitmapManager::Get("medal_U.tga");
	m_ppIcons[2]=MBitmapManager::Get("medal_E.tga");
	m_ppIcons[3]=MBitmapManager::Get("medal_F.tga");
	m_ppIcons[4]=MBitmapManager::Get("medal_H.tga");

	m_CrossHair.Create();
	m_CrossHair.ChangeFromOption();

	m_pWeaponScreenEffect = new ZWeaponScreenEffect;
	m_pWeaponScreenEffect->Create();

	if (ZGetGameTypeManager()->IsQuestDerived(g_pGame->GetMatch()->GetMatchType()))
	{
		m_pQuestScreen = new ZCombatQuestScreen();
	}

	MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatTDMInfo");
	if ( pWidget)
	{
		if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2)
		{
			int nMargin[ BMNUM_NUMOFCHARSET] = { 13,9,13,13,13,13,13,13,13,13,8,10,8 };

			ZBmNumLabel* pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Blue");
			if ( pBmNumLabel)
			{
				pBmNumLabel->SetAlignmentMode( MAM_HCENTER);
				pBmNumLabel->SetCharMargin( nMargin);
				pBmNumLabel->SetNumber( 0);
			}

			pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Red");
			if ( pBmNumLabel)
			{
				pBmNumLabel->SetAlignmentMode( MAM_HCENTER);
				pBmNumLabel->SetIndexOffset( 16);
				pBmNumLabel->SetCharMargin( nMargin);
				pBmNumLabel->SetNumber( 0);
			}

			pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Max");
			if ( pBmNumLabel)
			{
				pBmNumLabel->SetAlignmentMode( MAM_HCENTER);
				pBmNumLabel->SetIndexOffset( 32);
				int nMargin2[ BMNUM_NUMOFCHARSET] = { 18,12,18,18,18,18,18,18,18,18,18,18,18 };
				pBmNumLabel->SetCharMargin( nMargin2);
				pBmNumLabel->SetNumber( 0);
			}

			pWidget->Show( true);


			MWidget *pPicture = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_RedWin");
			if ( pPicture)
				pPicture->Show( true);
			pPicture = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_BlueWin");
			if ( pPicture)
				pPicture->Show( true);
		}
		else
		{
			pWidget->Show( false);

			MWidget *pPicture = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_RedWin");
			if ( pPicture)
				pPicture->Show( false);
			pPicture = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_BlueWin");
			if ( pPicture)
				pPicture->Show( false);
		}
	}

#ifdef _BIRDSOUND

#else
	ZGetSoundEngine()->Set3DSoundUpdate( true );
#endif

	m_bOnFinish = false;
	m_bShowResult = false;

	pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "Option");
	if ( pWidget)
		pWidget->Show( false);

	return true;
}


void ZCombatInterface::OnDestroy()
{
	if(m_nClanIDBlue) {
		ZGetEmblemInterface()->DeleteClanInfo(m_nClanIDBlue);
		m_nClanIDBlue = 0;
	}
	if(m_nClanIDRed) {
		ZGetEmblemInterface()->DeleteClanInfo(m_nClanIDRed);
		m_nClanIDRed = 0;
	}

	if (m_pQuestScreen){ delete m_pQuestScreen; m_pQuestScreen=NULL; }

	m_Observer.Destroy();

	m_ResultItems.Destroy();
	SAFE_DELETE(m_pResultPanel);
	SAFE_DELETE(m_pResultPanel_Team);
	SAFE_DELETE(m_pResultLeft);
	SAFE_DELETE(m_pResultRight);

	EnableInputChat(false);

	m_Chat.Destroy();
	m_AdminMsg.Destroy();

	/*
	if (m_pScoreBoard)
	{
		m_pScoreBoard->OnDestroy();
		delete m_pScoreBoard;
		m_pScoreBoard = NULL;
	}
	*/
	m_CrossHair.Destroy();

	if (m_pTargetLabel)
	{
		delete m_pTargetLabel;
		m_pTargetLabel = NULL;
	}
	ShowInfo(false);

#ifdef _BIRDSOUND

#else
	ZGetSoundEngine()->Set3DSoundUpdate( false );
#endif

	MPicture *pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "GameResult_Background");
	if ( pPicture)
		pPicture->SetBitmap( NULL);

	if ( m_pResultBgImg != NULL)
	{
		delete m_pResultBgImg;
		m_pResultBgImg = NULL;
	}


	MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatTDMInfo");
	if ( pWidget)
		pWidget->Show( false);



	m_pWeaponScreenEffect->Destroy();
	SAFE_DELETE(m_pWeaponScreenEffect);

	ZScoreBoardItem::Release();
}

void TextRelative(MDrawContext* pDC,float x,float y,const char *szText,bool bCenter)
{
	int screenx=x*MGetWorkspaceWidth();
	if(bCenter)
	{
		MFont *pFont=pDC->GetFont();
		screenx-=pFont->GetWidth(szText)/2;
	}

	pDC->Text(screenx,y*MGetWorkspaceHeight(),szText);
}

void ZCombatInterface::DrawNPCName(MDrawContext* pDC)
{
	for(ZObjectManager::iterator itor = ZGetObjectManager()->begin();
		itor != ZGetObjectManager()->end(); ++itor)
	{
		rvector pos, screen_pos;
		ZObject* pObject= (*itor).second;
		if (!pObject->IsVisible()) continue;
		if (pObject->IsDie()) continue;
		if(!pObject->IsNPC()) continue;

		ZActor *pActor = (ZActor*)pObject;
//		if(!pActor->CheckFlag(AF_MY_CONTROL)) continue;


		pos = pObject->GetPosition();
		RVisualMesh* pVMesh = pObject->m_pVMesh;
		RealSpace2::rboundingbox box;

		if (pVMesh == NULL) continue;
		
		box.vmax = pos + rvector(50.f, 50.f, 190.f);
		box.vmin = pos + rvector(-50.f, -50.f, 0.f);

		if (isInViewFrustum(&box, RGetViewFrustum()))
		{
			screen_pos = RGetTransformCoord(pObject->GetPosition()+rvector(0,0,100.f));

			MFont *pFont=NULL;
			pFont = pActor->CheckFlag(AF_MY_CONTROL) ? MFontManager::Get("FONTa12_O1Blr") : MFontManager::Get("FONTa12_O1Red");
			pDC->SetColor(MCOLOR(0xFF00FF00));
			pDC->SetBitmap(NULL);
			pDC->SetFont(pFont);

			int x = screen_pos.x - pDC->GetFont()->GetWidth(pActor->m_szOwner) / 2;
			pDC->Text(x, screen_pos.y - 12, pActor->m_szOwner);

			// 마지막 정보를 받은시간이 이상하면 찍어준다
			float fElapsedTime = g_pGame->GetTime() - pActor->m_fLastBasicInfo;
			if(!pActor->CheckFlag(AF_MY_CONTROL) && fElapsedTime>.2f) {
				int y= screen_pos.y;
				y+=pFont->GetHeight();
				char temp[256];
				sprintf_s(temp,"%2.2f",fElapsedTime);
				x = screen_pos.x - pDC->GetFont()->GetWidth(temp) / 2;
				pDC->Text(x, y - 12, temp);
			}
		}
	}
}

void ZCombatInterface::DrawTDMScore(MDrawContext* pDC)
{
	int nBlueKills = ZGetGame()->GetMatch()->GetTeamKills(MMT_BLUE);
	int nRedKills = ZGetGame()->GetMatch()->GetTeamKills(MMT_RED);
	int nTargetKills = ZGetGameClient()->GetMatchStageSetting()->GetRoundMax();


	ZBmNumLabel* pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Blue");
	if ( pBmNumLabel)
		pBmNumLabel->SetNumber( nBlueKills);

	pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Red");
	if ( pBmNumLabel)
		pBmNumLabel->SetNumber( nRedKills);

	pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_Score_Max");
		pBmNumLabel->SetNumber( nTargetKills);


	MWidget* pRed = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_RedWin");
	MWidget* pBlue = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "TDM_BlueWin");
	if ( pRed && pBlue)
	{
		int nTime[] = { 1, 1400, 1400, 900, 900, 200 };
		int nDiff = min( abs( nBlueKills - nRedKills) , 5);
		int nCurrTime = timeGetTime() % nTime[ nDiff];

		if (nDiff == 0)
		{
			pRed->Show( false);
			pBlue->Show( false);

			return;
		}
		if ( (nDiff == 1) || (nDiff == 2) || ( nDiff >= 5))
		{
			if ( nCurrTime > 100)
			{
				pRed->Show( false);
				pBlue->Show( false);

				return;
			}
		}
		else if ( (nDiff == 3) || (nDiff == 4))
		{
			if ( ((nCurrTime > 100) && (nCurrTime < 200)) || (nCurrTime > 300))
			{
				pRed->Show( false);
				pBlue->Show( false);

				return;
			}
		}


		if ( nRedKills > nBlueKills)
		{
			pRed->Show( true);
			pBlue->Show( false);
		}
		else if ( nRedKills < nBlueKills)
		{
			pRed->Show( false);
			pBlue->Show( true);
		}
		else
		{
			pRed->Show( false);
			pBlue->Show( false);
		}
	}
}

void ZCombatInterface::OnDraw(MDrawContext* pDC)
{
	if ( m_bShowResult)
		return;

	ZCharacter* pCharacter = GetTargetCharacter();
	if (pCharacter == NULL) return;

	bool bDrawAllPlayerName = false;

	if(g_pGame->m_pMyCharacter->IsAdminHide()
		&& MEvent::GetAltState() && ZGetCamera()->GetLookMode()!=ZCAMERA_MINIMAP)
		bDrawAllPlayerName = true;

	if(ZGetCamera()->GetLookMode()==ZCAMERA_FREELOOK || bDrawAllPlayerName)
		DrawAllPlayerName(pDC);
	else 
	{
		if(!ZGetGameInterface()->IsMiniMapEnable()) 
		{
			DrawFriendName(pDC);
			DrawEnemyName(pDC);
		}
	}

	// 디버그용 누가 npc를 조종하는가 보여준다
//	DrawNPCName(pDC);

//	MFont *pFont = MFontManager::Get("FONTa30");
//	pDC->SetFont( pFont );
//	pDC->SetColor(MCOLOR(0xFF999999));
//	pDC->Text(100, 100, "사람살려");


	//	DrawCharacterIcons(pDC);
	//DrawKickPlayerList(pDC);
	GetVoteInterface()->DrawVoteTargetlist(pDC);
	GetVoteInterface()->DrawVoteMessage(pDC);

	/*
	ZGetScreenEffectManager()->SetGuages(
	(float)pCharacter->GetStatus()->nHP/(float)pCharacter->GetProperty()->nMaxHP,
	(float)pCharacter->GetStatus()->nAP/(float)pCharacter->GetProperty()->nMaxAP,
	0.5f);
	ZGetScreenEffectManager()->SetWeapon(pCharacter->GetVisualMesh()->m_SelectWeaponType);
	*/

	m_pWeaponScreenEffect->Draw(pDC);

	ZGetScreenEffectManager()->Draw();

	if (!m_Observer.IsVisible())
	{
//		if(g_pGame&&g_pGame->GetMatch()->GetRoundState()==MMATCH_ROUNDSTATE_PLAY)
		{
			MFont *pFont=GetGameFont();

			pDC->SetFont(pFont);
			pDC->SetColor(MCOLOR(0xFFFFFFFF));

			char buffer[256];

			sprintf_s(buffer,"%d  %s", pCharacter->GetProperty()->nLevel, pCharacter->GetProperty()->szName);

			if ( (ZApplication::GetGame()->GetMatch()->GetMatchType() != MMATCH_GAMETYPE_DUEL) || ( !pCharacter->IsObserverTarget()))
			{
				float fCenterVert = 0.018f - (float)pFont->GetHeight()/(float)RGetScreenHeight()/2;
				TextRelative(pDC,100.f/800.f,fCenterVert,buffer);
			}

			TextRelative(pDC,660.f/800.f,510.f/600.f,m_szItemName);

			// melee일때는 탄알수 표시를 하지 않는다.
			if (pCharacter->GetItems()->GetSelectedWeaponParts() != MMCIP_MELEE) 
			{
				sprintf_s(buffer, "%d / %d", m_nBulletAMagazine, m_nBullet);
				TextRelative(pDC,720.f/800.f,585.f/600.f,buffer);
			}

			sprintf_s(buffer, "%d", pCharacter->GetHP());
			TextRelative(pDC, 160.f / 1920, 44.f / 1080, buffer);
			sprintf_s(buffer, "%d", pCharacter->GetAP());
			TextRelative(pDC, 160.f / 1920, 92.f / 1080, buffer);
		}

		// 듀얼 모드일때(옵져버 모드 아님)
		if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
		{
			char	charName[ 3][ 32];
			charName[0][0] = charName[1][0] = charName[2][0] = 0;
			float fRx = (float)MGetWorkspaceWidth()  / 800.0f;
			float fRy = (float)MGetWorkspaceHeight() / 600.0f;

			MFont *pFont = MFontManager::Get( "FONTa10_O2Wht");
			if ( pFont == NULL)
				_ASSERT(0);
			pDC->SetFont( pFont);
			pDC->SetColor( MCOLOR(0xFFFFFFFF));

			bool bIsChallengerDie = false;
			int nMyChar = -1;

			ZRuleDuel* pDuel = (ZRuleDuel*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
			if ( pDuel)
			{
				for (ZCharacterManager::iterator itor = ZGetCharacterManager()->begin(); itor != ZGetCharacterManager()->end(); ++itor)
				{
					ZCharacter* pCharacter = (*itor).second;

					// Player
					if ( pCharacter->GetUID() == pDuel->QInfo.m_uidChampion)
					{
						if ( ZGetMyUID() == pDuel->QInfo.m_uidChampion)
						{
							// Draw victory
							ZGetCombatInterface()->DrawVictory( pDC, 210, 86, pDuel->QInfo.m_nVictory);
						}
						else
						{
							sprintf_s( charName[ 0], "%s%d  %s", ZMsg( MSG_CHARINFO_LEVELMARKER), pCharacter->GetProperty()->nLevel, pCharacter->GetUserName());

							if ( (ZGetMyUID() == pDuel->QInfo.m_uidChampion) || (ZGetMyUID() == pDuel->QInfo.m_uidChallenger))
							{
								// Draw victory
								int nTextWidth = pFont->GetWidth( charName[ 0]);
								int nWidth = ZGetCombatInterface()->DrawVictory( pDC, 162, 300, pDuel->QInfo.m_nVictory, true);
								ZGetCombatInterface()->DrawVictory( pDC, 43+nTextWidth+nWidth, 157, pDuel->QInfo.m_nVictory);
							}
						}
					}

					else if ( pCharacter->GetUID() == pDuel->QInfo.m_uidChallenger)
					{
						if ( ZGetMyUID() != pDuel->QInfo.m_uidChallenger)
							sprintf_s( charName[ 0], "%s%d  %s", ZMsg( MSG_CHARINFO_LEVELMARKER), pCharacter->GetProperty()->nLevel, pCharacter->GetUserName());

						bIsChallengerDie = pCharacter->IsDie();
					}

					// Waiting 1
					else if (pCharacter->GetUID() == pDuel->QInfo.m_WaitQueue[0])
						sprintf_s( charName[ 1], "%s%d  %s", ZMsg( MSG_CHARINFO_LEVELMARKER), pCharacter->GetProperty()->nLevel, pCharacter->GetUserName());

					// Waiting 2
					else if (pCharacter->GetUID() == pDuel->QInfo.m_WaitQueue[1])
						sprintf_s( charName[ 2], "%s%d  %s", ZMsg( MSG_CHARINFO_LEVELMARKER), pCharacter->GetProperty()->nLevel, pCharacter->GetUserName());
				}
			}

			MBitmap* pBitmap = MBitmapManager::Get( "duel-mode.tga");
			if ( pBitmap)
			{
				pDC->SetBitmap( pBitmap);

				int nIcon = 50.0f*fRx;
				pDC->Draw( 8.0f*fRx, 153.0f*fRy, nIcon, nIcon);
			}

			pBitmap = MBitmapManager::Get( "icon_play.tga");
			if ( pBitmap && ( charName[1][0] != 0))
			{
				pDC->SetBitmap( pBitmap);

				int nIcon = 22.0f*fRx;
				pDC->Draw( 60.0f*fRx, 175.0f*fRy, nIcon, nIcon);
				pDC->Draw( 53.0f*fRx, 175.0f*fRy, nIcon, nIcon);
			}

			MCOLOR color;

			int nTime = timeGetTime() % 200;
			if ( nTime < 100)
				pDC->SetColor( MCOLOR( 0xFFFFFF00));
			else
				pDC->SetColor( MCOLOR( 0xFFA0A0A0));

			if ( bIsChallengerDie)
				pDC->SetColor( MCOLOR( 0xFF808080));

			int nPosY = 160.0f*fRy;
			pDC->Text( 60.0f*fRx, nPosY, charName[ 0]);

			pDC->SetColor( MCOLOR(0xFF808080));
			nPosY += 20;
			pDC->Text( 80.0f*fRx, nPosY, charName[ 1]);
			nPosY += 15;
//			pDC->Text( 80.0f*fRx, nPosY, charName[ 2]);
		}
	}


	m_bDrawScoreBoard = false;

	if( ZIsActionKeyPressed(ZACTION_SCORE) == true ) {
		if (m_Chat.IsShow() == false)
			m_bDrawScoreBoard = true;
	}
	else if( ZApplication::GetGame()->GetMatch()->GetRoundState() == MMATCH_ROUNDSTATE_PREPARE ) {

		int cur_round = ZApplication::GetGame()->GetMatch()->GetCurrRound();
		//		int max_round = ZApplication::GetGame()->GetMatch()->GetRoundCount();

		if(cur_round == 0) {
			m_bDrawScoreBoard = true;
		}
	}

	// 퀘스트모드일때는 퀘스트 화면을 보여준다.
	if (m_pQuestScreen)
	{
		m_pQuestScreen->OnDraw(pDC);
	}

	if (ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2)
	{
		DrawTDMScore(pDC);
	}

	if ( m_bDrawScoreBoard ) {

		DrawScoreBoard(pDC);
	}


//	DrawResultBoard(pDC);

	// 채팅 인풋 창을 그린다.

	m_Chat.OnDraw(pDC);

	DrawSoloSpawnTimeMessage(pDC);

	// 크로스헤어
	if(ZGetGameInterface()->GetCamera()->GetLookMode()==ZCAMERA_DEFAULT)
	{
		m_CrossHair.Draw(pDC);
	}

	//그리는 순서 때문에
	if(g_pGame)
		g_pGame->m_HelpScreen.DrawHelpScreen();


	// Finish 후에 일정 시간이 경과하면 결과 화면 보여줌
	if ( !m_bShowResult && IsFinish())
	{
		// 배경 음악 볼륨을 서서히 낮춤
		float fVolume;
		DWORD dwClock = timeGetTime() - m_nReserveFinishTime;
		if ( dwClock > 4000)
			fVolume = 0.0f;
		else
			fVolume = (float)(4000 - dwClock) / 4000.0f * m_fOrgMusicVolume;

		ZApplication::GetSoundEngine()->SetMusicVolume( fVolume);


		if ( timeGetTime() >= m_nReservedOutTime)
		{
			MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "Option");
			if ( pWidget)
				pWidget->Show( false);
			pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatMenuFrame");
			if ( pWidget)
				pWidget->Show( false);
			MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatChatInput");
			if ( pLabel)
				pLabel->SetText("");
			ZApplication::GetGameInterface()->GetCombatInterface()->EnableInputChat( false);



			// 만약 퀘스트 실패면 스테이지로 바로 넘어간다.
			if ( ZGetGameTypeManager()->IsQuestOnly( g_pGame->GetMatch()->GetMatchType()))
			{
				if ( !ZGetQuest()->IsQuestComplete())
				{
					ZChangeGameState( GUNZ_STAGE);
					m_bShowResult = true;

					return;
				}
			}

			// 배경 화면에 필요한 정보를 업데이트 한다
			GetResultInfo();

			pWidget = ZGetGameInterface()->GetIDLResource()->FindWidget( "GameResult");
			if ( pWidget)
				pWidget->Show( true, true);


			// 사운드를 출력한다
			ZApplication::GetSoundEngine()->SetMusicVolume( m_fOrgMusicVolume);
#ifdef _BIRDSOUND
			ZApplication::GetSoundEngine()->OpenMusic(BGMID_FIN);
			ZApplication::GetSoundEngine()->PlayMusic(false);
#else
			ZApplication::GetSoundEngine()->OpenMusic(BGMID_FIN, ZApplication::GetFileSystem());
			ZApplication::GetSoundEngine()->PlayMusic(false);
#endif
			m_nReservedOutTime = timeGetTime() + 15000;
			m_bShowResult = true;
		}
	}
}


int ZCombatInterface::DrawVictory( MDrawContext* pDC, int x, int y, int nWinCount, bool bGetWidth)
{
//	nWinCount = 99;										// for test

	// Get total width
	if ( bGetWidth)
	{
		int nWidth = 0;

		int nNum = nWinCount % 5;
		if ( nNum)
			nWidth += 17.0f + 17.0f * 0.63f * (nNum - 1);

		if ( (nWinCount % 10) >= 5)
			nWidth += 19.0f * 0.2f + 19.0f * 1.1f;
		else
			nWidth += 19.0f * 0.5f;

		nNum = nWinCount / 10;
		if ( nNum)
			nWidth += 22.0f + 22.0f * 0.5f * (nNum - 1);

		return nWidth;
	}


    // Get image
	MBitmap* pBitmap = MBitmapManager::Get( "killstone.tga");
	if ( !pBitmap)
		return 0;

	pDC->SetBitmap( pBitmap);

	// Get screen
	float fRx = (float)MGetWorkspaceWidth()  / 800.0f;
	float fRy = (float)MGetWorkspaceHeight() / 600.0f;


	// Get Image Number
	int nImage = ( (timeGetTime() / 100) % 20);
	if ( nImage > 10)
		nImage = 0;
	nImage *= 32;
	nImage = ( (timeGetTime() / 100) % 20);
	if ( nImage > 10)
		nImage = 0;
	nImage *= 32;

	// Draw
	int nPosX = x * fRx;
	int nPosY = y * fRy;
	int nSize = 17.0f * fRx;								// 1 단위
	for ( int i = 0;  i < (nWinCount % 5);  i++)
	{
		pDC->Draw( nPosX, nPosY, nSize, nSize, nImage, 0, 32, 32);
		nPosX -= nSize * 0.63f;
	}

	nSize = 19.0f * fRx;
	nPosY = ( y - 2) * fRy;
	if ( (nWinCount % 10) >= 5)								// 5 단위
	{
		nPosX -= nSize * 0.2f;
		pDC->Draw( nPosX, nPosY, nSize, nSize, nImage, 64, 32, 32);
		nPosX -= nSize * 1.1f;
	}
	else
		nPosX -= nSize * 0.5f;

	nSize = 22.0f * fRx;									// 10 단위
	nPosY = ( y - 5) * fRy;
	for ( int i = 0;  i < (nWinCount / 10);  i++)
	{
		pDC->Draw( nPosX, nPosY, nSize, nSize, nImage, 32, 32, 32);
		nPosX -= nSize * 0.5f;
	}

	// 연승 숫자 표시
/*	if ( nWinCount >= 10)
	{
		pFont = MFontManager::Get( "FONTa9b");
		pDC->SetFont( pFont);
		pDC->SetColor( MCOLOR(0xFFFFFFFF));
		char szVictory[ 16];
		sprintf_s( szVictory, "%d", nWinCount);
		TextRelative( pDC, 0.195f, 0.01f, szVictory, true);
	}
*/
	return 0;
}


// TODO : 이게 필요 없는듯.
// 그리는 순서때문에 만든 펑션
void ZCombatInterface::OnDrawCustom(MDrawContext* pDC)
{
	// 결과 화면 보인 이후에 일정 시간 후 자동 종료한다
	if ( m_bShowResult)
	{
		// 제한시간이면 종료한다.
		if ( timeGetTime() > m_nReservedOutTime)
		{
			if(ZGetGameClient()->IsLadderGame())
				ZChangeGameState(GUNZ_LOBBY);
			else
				ZChangeGameState(GUNZ_STAGE);

			return;
		}


		// 숫자를 카운터한다.
		if ( ZGetGameTypeManager()->IsQuestOnly( g_pGame->GetMatch()->GetMatchType()))
		{
			int nNumCount = timeGetTime() - (m_nReservedOutTime - 15000);
			ZBmNumLabel* pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetPlusXP");
			if ( pBmNumLabel)
			{
				if ( nNumCount < ZGetQuest()->GetRewardXP())
					pBmNumLabel->SetNumber( nNumCount, false);
				else
					pBmNumLabel->SetNumber( ZGetQuest()->GetRewardXP(), false);
			}
			pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetMinusXP");
			if ( pBmNumLabel)
				pBmNumLabel->SetNumber( 0, false);
			pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetTotalXP");
			if ( pBmNumLabel)
			{
				if ( nNumCount < ZGetQuest()->GetRewardXP())
					pBmNumLabel->SetNumber( nNumCount, false);
				else
					pBmNumLabel->SetNumber( ZGetQuest()->GetRewardXP(), false);
			}
			pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetBounty");
			if ( pBmNumLabel)
			{
				if ( nNumCount < ZGetQuest()->GetRewardBP())
					pBmNumLabel->SetNumber( nNumCount, false);
				else
					pBmNumLabel->SetNumber( ZGetQuest()->GetRewardBP(), false);
			}
		}


		// 남은 시간을 표시한다
		MLabel* pLabel = (MLabel*)ZGetGameInterface()->GetIDLResource()->FindWidget( "GameResult_RemaindTime");
		if ( pLabel)
		{
			char szRemaindTime[ 100];
			sprintf_s( szRemaindTime, "%d", ( m_nReservedOutTime - timeGetTime()) / 1000);
			char szText[ 100];
			ZTransMsg( szText, MSG_GAME_EXIT_N_MIN_AFTER, 1, szRemaindTime);

			pLabel->SetAlignment( MAM_HCENTER);
			pLabel->SetText( szText);
		}

		return ;
	}


	if (m_Observer.IsVisible())
	{
		if( !ZGetGameInterface()->IsMiniMapEnable())
		{
			if ( !g_pGame->IsReplay() || g_pGame->IsShowReplayInfo())
				ZGetScreenEffectManager()->DrawSpectator();
		}

		m_Observer.OnDraw(pDC);
	}

	if(m_bDrawLeaveBattle)
		DrawLeaveBattleTimeMessage(pDC);
}


void ZCombatInterface::DrawSoloSpawnTimeMessage(MDrawContext* pDC)
{
	if(g_pGame->m_pMyCharacter->IsAdminHide()) return;

	ZMatch* pMatch = ZApplication::GetGame()->GetMatch();
	if (pMatch->GetRoundState() == MMATCH_ROUNDSTATE_PLAY)
	{
		if (!pMatch->IsWaitForRoundEnd())
		{
			if (g_pGame->m_pMyCharacter && g_pGame->m_pMyCharacter->IsDie())
				{
					char szMsg[128] = "";
				int nRemainTime = pMatch->GetRemainedSpawnTime();
				if ((nRemainTime > 0) && (nRemainTime <= 5))
				{
//					sprintf_s(szMsg, "%d 초동안 기다려주세요!", nRemainTime);
					char temp[ 4 ];
					sprintf_s( temp, "%d", nRemainTime );
					ZTransMsg( szMsg, MSG_GAME_WAIT_N_MIN, 1, temp );
				}
				else if ((nRemainTime == 0) && (!g_pGame->GetSpawnRequested()))
				{
//					sprintf_s(szMsg, "플레이하려면 Fire키를 눌러주세요!");
					sprintf_s( szMsg, 
						ZMsg(MSG_GAME_CLICK_FIRE) );
				}

				MFont *pFont=GetGameFont();
				pDC->SetFont(pFont);
				pDC->SetColor(MCOLOR(0xFFFFFFFF));
				TextRelative(pDC,400.f/800.f,400.f/600.f, szMsg, true);
			}
		}
	}
}

void ZCombatInterface::DrawLeaveBattleTimeMessage(MDrawContext* pDC)
{
	char szMsg[128] = "";
//	sprintf_s(szMsg, "%d 초후에 게임에서 나갑니다", m_nDrawLeaveBattleSeconds);

	char temp[ 4 ];
	sprintf_s( temp, "%d", m_nDrawLeaveBattleSeconds );
	ZTransMsg( szMsg, MSG_GAME_EXIT_N_MIN_AFTER, 1, temp );

	MFont *pFont=GetGameFont();
	pDC->SetFont(pFont);
	pDC->SetColor(MCOLOR(0xFFFFFFFF));
	TextRelative(pDC,400.f/800.f,350.f/600.f, szMsg, true);
}


bool ZCombatInterface::IsDone()
{
	return false;
}

bool ZCombatInterface::OnEvent(MEvent* pEvent, MListener* pListener)
{
	return false;
}

void ZCombatInterface::Resize(int w, int h)
{
	SetSize(w, h);

}

void ZCombatInterface::ShowMenu(bool bVisible)
{
	if (m_bMenuVisible == bVisible) return;

	
	m_bMenuVisible = bVisible;
}

void ZCombatInterface::EnableInputChat(bool bInput, bool bTeamChat)
{
	// 채팅창 안보기인 상태일 때에는 입력도 안된다.
//	if ((!ZGetConfiguration()->GetViewGameChat()) && (bInput)) return;

	m_Chat.EnableInput(bInput, bTeamChat);
}

void ZCombatInterface::OutputChatMsg(const char* szMsg)
{
	m_Chat.OutputChatMsg(szMsg);
}

void ZCombatInterface::OutputChatMsg(MCOLOR color, const char* szMsg)
{
	m_Chat.OutputChatMsg(color, szMsg);
}


void ZCombatInterface::SetBullet(int nBullet)
{
	if (m_nBullet == nBullet) return;

	m_nBullet = nBullet;
}
void ZCombatInterface::SetBulletAMagazine(int nBulletAMagazine)
{
	if (m_nBulletAMagazine == nBulletAMagazine) return;

	m_nBulletAMagazine = nBulletAMagazine;
}

void ZCombatInterface::SetItemName(const char* szName)
{
	if (!strcmp(m_szItemName, szName)) return;

	strcpy_safe(m_szItemName, szName);
}


void ZCombatInterface::ShowInfo(bool bVisible)
{
	MWidget* pWidget;
	char szTemp[256];
	for (int i = 0; i < 9; i++)
	{
		sprintf_s(szTemp, "%s%d", ZIITEM_COMBAT_INFO, i);
		pWidget = m_pIDLResource->FindWidget(szTemp);
		if (pWidget!=NULL)
		{
			pWidget->Show(bVisible);
		}
	}
	pWidget = m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATFRAME);
	if (pWidget!=NULL)
	{
		pWidget->Show(bVisible);
	}
}

void ZCombatInterface::Update()
{
	if (m_bReserveFinish)
	{
		if ((timeGetTime() - m_nReserveFinishTime) > 1000)
		{
			OnFinish();
			m_bReserveFinish = false;
		}
	}

	ZCharacter* pCharacter = GetTargetCharacter();
	if (pCharacter == NULL) return;

	if (!pCharacter->GetInitialized()) return;

	if(ZGetScreenEffectManager()==NULL) return;
	if(pCharacter->GetStatus()==NULL)	return;
	if(pCharacter->GetProperty()==NULL) return;

	float fGuage = 100.f;
	float fCur,fMax;
/*
	bool bPre = false;

	if(g_pGame&&g_pGame->GetMatch()) {
//	if(g_pGame&&g_pGame->GetMatch()->GetRoundState()==MMATCH_ROUNDSTATE_PLAY) {
//	if(g_pGame&&(g_pGame->GetReadyState()==ZGAME_READYSTATE_RUN)) {

	if(bPre) 
*/
	if(g_pGame&&g_pGame->GetMatch())
	{
		fCur = (float)pCharacter->GetHP();
		fMax = (float)pCharacter->GetProperty()->fMaxHP;

		if( fCur!=0.f && fMax!=0.f )	fGuage = fCur/fMax;
		else							fGuage = 0.f;

		if( g_pGame->GetMatch()->GetCurrRound()==0 && 
			g_pGame->GetMatch()->GetRoundState()==MMATCH_ROUNDSTATE_PREPARE)
			fGuage = 100.f;

		ZGetScreenEffectManager()->SetGuage_HP(fGuage);

		fCur = (float)pCharacter->GetAP();
		fMax = (float)pCharacter->GetProperty()->fMaxAP;

		if( fCur!=0.f && fMax!=0.f )	fGuage = fCur/fMax;
		else							fGuage = 0.f;
		

		ZGetScreenEffectManager()->SetGuage_AP(fGuage);

	} 
	else {

		ZGetScreenEffectManager()->SetGuage_HP(fGuage);
		ZGetScreenEffectManager()->SetGuage_AP(fGuage);
	}

//	ZGetScreenEffectManager()->SetGuage_EXP((float)pCharacter->GetStatus()->fStamina/100.f);//임시로 stamina 로 테스트
//	ZGetScreenEffectManager()->SetGuage_EXP(100.f);//임시로 stamina 로 테스트

	MMatchWeaponType wtype = MWT_NONE;

	ZItem* pSItem = pCharacter->GetItems()->GetSelectedWeapon();

	MMatchItemDesc* pSelectedItemDesc = NULL; 

	if( pSItem ) {
		pSelectedItemDesc = pSItem->GetDesc();

		SetBullet( pSItem->GetBullet() );
		SetBulletAMagazine( pSItem->GetBulletAMagazine() );
	}

	if( pSelectedItemDesc ) {
		wtype = pSelectedItemDesc->m_nWeaponType;
	}

	ZGetScreenEffectManager()->SetWeapon( wtype ,pSelectedItemDesc );


	if ((pSelectedItemDesc) && (m_pLastItemDesc != pSelectedItemDesc))
	{
		SetItemName( pSelectedItemDesc->m_szName );
	}

	UpdateCombo(pCharacter);

	m_Chat.Update();
	m_AdminMsg.Update();


	/*
	if (m_pScoreBoard->IsVisible())
	{
		m_pScoreBoard->Update();
	}
	*/

	if (pCharacter->GetItems()->GetSelectedWeaponParts() == MMCIP_MELEE)
	{
		ShowCrossHair(false);
	}
	else 
	{
		ShowCrossHair(true);
	}

	GameCheckPickCharacter();
}

bool GetUserInfoUID(MUID uid,MCOLOR& _color,char* sp_name,MMatchUserGradeID& gid);

void ZCombatInterface::SetPickTarget(bool bPick, ZCharacter* pCharacter)
{
	bool bFriend = false;
	if (bPick)
	{
		if (pCharacter == NULL) return;

		if (ZApplication::GetGame()->GetMatch()->IsTeamPlay())
		{
			ZCharacter *pTargetCharacter=GetTargetCharacter();
			if(pTargetCharacter && pTargetCharacter->GetTeamID()==pCharacter->GetTeamID())
			{
				bFriend = true;
			}
		}

		if (bFriend == false) 
		{
			m_CrossHair.SetState(ZCS_PICKENEMY);
			m_pTargetLabel->SetTextColor(0xffff0000);
		}

		if(pCharacter->IsAdmin())
			m_pTargetLabel->SetTextColor(ZCOLOR_ADMIN_NAME);

		if (!bFriend == true && !pCharacter->IsDie()) 
		{
			
			strcpy_safe(m_szTargetName, pCharacter->GetUserName());
#ifdef _DEBUG
			sprintf_s(m_szTargetName, "%s : %d", pCharacter->GetUserName(), pCharacter->GetHP());
#endif
			m_pTargetLabel->SetText(m_szTargetName);
		}

		int nCrosshairHeight = m_CrossHair.GetHeight();

		int nLen = m_pTargetLabel->GetRect().w;
		m_pTargetLabel->SetPosition(((MGetWorkspaceWidth()-m_pTargetLabel->GetRect().w)/2) ,(MGetWorkspaceHeight()/2) - nCrosshairHeight );
		m_pTargetLabel->SetAlignment(MAM_HCENTER);
	}
	else
	{
		m_CrossHair.SetState(ZCS_NORMAL);
		memset(m_szTargetName, 0, sizeof(m_szTargetName));
		m_pTargetLabel->Show(false);
	}

	m_bPickTarget = bPick;
}

void ZCombatInterface::SetItemImageIndex(int nIndex)
{
	char szTemp[256];
	sprintf_s(szTemp, "item%02d.png", nIndex);
	BEGIN_WIDGETLIST("CombatItemPic", ZApplication::GetGameInterface()->GetIDLResource(),
		MPicture*, pPicture);

	pPicture->SetBitmap(MBitmapManager::Get(szTemp));

	END_WIDGETLIST();
}

void ZCombatInterface::SetMagazine(int nMagazine)
{
	if (m_nMagazine == nMagazine) return;

	char szTemp[256];
	sprintf_s(szTemp, "%02d", nMagazine);
	BEGIN_WIDGETLIST("CombatMagazine", ZApplication::GetGameInterface()->GetIDLResource(),
		MWidget*, pWidget);

	pWidget->SetText(szTemp);

	END_WIDGETLIST();

	m_nMagazine = nMagazine;
}


void ZCombatInterface::UpdateCombo(ZCharacter* pCharacter)
{
	if(pCharacter==NULL) return;

	static int nComboX = -999, nComboY = -999;
	static int nLastCombo = 0;

	if (pCharacter->GetStatus()->nCombo != nLastCombo)
	{
		nLastCombo = pCharacter->GetStatus()->nCombo;
		ZGetScreenEffectManager()->SetCombo(nLastCombo);
	}
	else if (pCharacter->GetStatus()->nCombo != 0)
	{

	}
}


void ZCombatInterface::DrawFriendName(MDrawContext* pDC)
{
	if (ZApplication::GetGame()->m_pMyCharacter == NULL) return;

	if (ZApplication::GetGame()->GetMatch()->IsTeamPlay())
	{
		ZCharacter* pTargetCharacter = GetTargetCharacter();
		if (pTargetCharacter == NULL) return;
		
		for(ZCharacterManager::iterator itor = ZApplication::GetGame()->m_CharacterManager.begin();
			itor != ZApplication::GetGame()->m_CharacterManager.end(); ++itor)
		{
			rvector pos, screen_pos;
			ZCharacter* pCharacter = (*itor).second;
			if (!pCharacter->IsVisible()) continue;
			if (pCharacter->IsDie()) continue;
			if (pCharacter->GetTeamID() != pTargetCharacter->GetTeamID()) continue;
			if (pCharacter==pTargetCharacter) continue;

			pos = pCharacter->GetPosition();
			RVisualMesh* pVMesh = pCharacter->m_pVMesh;
			RealSpace2::rboundingbox box;

			if (pVMesh == NULL) continue;
			
//			box.vmax = pVMesh->m_vBMax + pos;
//			box.vmin = pVMesh->m_vBMin + pos;
			
			box.vmax = pos + rvector(50.f, 50.f, 190.f);
			box.vmin = pos + rvector(-50.f, -50.f, 0.f);

			if (isInViewFrustum(&box, RGetViewFrustum()))
			{
				/*
#define CHARACTER_HEIGHT	185.0f
				pos.z = pos.z + CHARACTER_HEIGHT;
				screen_pos = RGetTransformCoord(pos);
				*/
				screen_pos = RGetTransformCoord(pCharacter->GetVisualMesh()->GetHeadPosition()+rvector(0,0,30.f));

				MFont *pFont=NULL;

				if(pCharacter->IsAdmin()) {
					pFont = MFontManager::Get("FONTa12_O1Org");
					pDC->SetColor(MCOLOR(ZCOLOR_ADMIN_NAME));
				}
				else {
					pFont = MFontManager::Get("FONTa12_O1Blr");
					pDC->SetColor(MCOLOR(0xFF00FF00));
				}

				pDC->SetBitmap(NULL);

				/////// Outline Font //////////
//				MFont *pFont=MFontManager::Get("FONTa12_O1Blr");
				if (pFont == NULL) _ASSERT(0);
				pDC->SetFont(pFont);
				///////////////////////////////

				int x = screen_pos.x - pDC->GetFont()->GetWidth(pCharacter->GetUserName()) / 2;

				pDC->Text(x, screen_pos.y - 12, pCharacter->GetUserName());
			}
		}
	}
}

void ZCombatInterface::DrawEnemyName(MDrawContext* pDC)
{
	MPOINT Cp = GetCrosshairPoint();

	ZPICKINFO pickinfo;

	rvector pos,dir;
	if(!RGetScreenLine(Cp.x,Cp.y,&pos,&dir))
		return;
	
	ZCharacter *pTargetCharacter=GetTargetCharacter();

	if(ZApplication::GetGame()->Pick(pTargetCharacter,pos,dir,&pickinfo))
	{
		if (pickinfo.pObject) {
			if (!IsPlayerObject(pickinfo.pObject)) return;
			if (pickinfo.pObject->IsDie()) return;

			ZCharacter* pPickedCharacter = (ZCharacter*)pickinfo.pObject;

			bool bFriend = false;
			if (ZApplication::GetGame()->GetMatch()->IsTeamPlay()) {
				if (pTargetCharacter && pPickedCharacter->GetTeamID() == pTargetCharacter->GetTeamID())
					bFriend = true;
			}

			if (bFriend == false) {

				/////// Outline Font //////////

				MFont *pFont = NULL;//MFontManager::Get("FONTa12_O1Red");

				if(pPickedCharacter->IsAdmin()) {
					pDC->SetColor(MCOLOR(ZCOLOR_ADMIN_NAME));
					pFont = MFontManager::Get("FONTa12_O1Org");
				}
				else {
					pFont = MFontManager::Get("FONTa12_O1Red");
				}

				if (pFont == NULL) _ASSERT(0);
				pDC->SetFont(pFont);

				int x = Cp.x - pDC->GetFont()->GetWidth(pPickedCharacter->GetUserName()) / 2;
				pDC->Text(x, Cp.y - pDC->GetFont()->GetHeight()-10, pPickedCharacter->GetUserName());
			}			
		}
	}
}

void ZCombatInterface::DrawAllPlayerName(MDrawContext* pDC)
{
	for(ZCharacterManager::iterator itor = ZApplication::GetGame()->m_CharacterManager.begin();
		itor != ZApplication::GetGame()->m_CharacterManager.end(); ++itor)
	{
		rvector pos, screen_pos;
		ZCharacter* pCharacter = (*itor).second;
		if (!pCharacter->IsVisible()) continue;
		if (pCharacter->IsDie()) continue;

		pos = pCharacter->GetPosition();
		RVisualMesh* pVMesh = pCharacter->m_pVMesh;
		RealSpace2::rboundingbox box;

		if (pVMesh == NULL) continue;
		
		box.vmax = pos + rvector(50.f, 50.f, 190.f);
		box.vmin = pos + rvector(-50.f, -50.f, 0.f);

		if (isInViewFrustum(&box, RGetViewFrustum()))
		{
			// 미니맵이면 z 값을 0에 맞춘다
			if(ZGetCamera()->GetLookMode()==ZCAMERA_MINIMAP) {
				rvector pos = pCharacter->m_Position;
				pos.z=0;
				screen_pos = RGetTransformCoord(pos);
			}else
				screen_pos = RGetTransformCoord(pCharacter->GetVisualMesh()->GetHeadPosition()+rvector(0,0,30.f));

			MFont *pFont=NULL;

			if(pCharacter->IsAdmin()) {
				pFont = MFontManager::Get("FONTa12_O1Org");
				pDC->SetColor(MCOLOR(ZCOLOR_ADMIN_NAME));
			}
			else {
				if (pCharacter->GetTeamID() == MMT_RED)
					pFont = MFontManager::Get("FONTa12_O1Red");
				else if (pCharacter->GetTeamID() == MMT_BLUE)
					pFont = MFontManager::Get("FONTa12_O1Blr");
				else
					pFont = MFontManager::Get("FONTa12_O1Blr");

				pDC->SetColor(MCOLOR(0xFF00FF00));
			}

			pDC->SetBitmap(NULL);

			/////// Outline Font //////////
//				MFont *pFont=MFontManager::Get("FONTa12_O1Blr");
			if (pFont == NULL) _ASSERT(0);
			pDC->SetFont(pFont);
			///////////////////////////////

			int x = screen_pos.x - pDC->GetFont()->GetWidth(pCharacter->GetUserName()) / 2;

			pDC->Text(x, screen_pos.y - 12, pCharacter->GetUserName());
		}
	}
}

MFont *ZCombatInterface::GetGameFont()
{
	MFont *pFont=MFontManager::Get("FONTa10_O2Wht");
	return pFont;
}

// 듀얼순위 / 팀 / 생사 / 성적이 소트의 기준이다
bool CompareZScoreBoardItem(ZScoreBoardItem* a,ZScoreBoardItem* b) {
	if(a->nDuelQueueIdx < b->nDuelQueueIdx) return true;
	if(a->nDuelQueueIdx > b->nDuelQueueIdx) return false;

	if(a->nTeam < b->nTeam) return true;
	if(a->nTeam > b->nTeam) return false;

	/*
	if(!a->bDeath && b->bDeath) return true;
	if(a->bDeath && !b->bDeath) return false;
	*/

	if( a->nExp > b->nExp) return true;
	if( a->nExp < b->nExp) return false;

	if(a->nKills > b->nKills) return true;
	if(a->nKills < b->nKills) return false;
	return false;
}

typedef list<ZScoreBoardItem*> ZSCOREBOARDITEMLIST;
void ZCombatInterface::DrawScoreBoard(MDrawContext* pDC)
{
	bool bClanGame = ZGetGameClient()->IsLadderGame();
//	bool bClanGame = true;

	ZSCOREBOARDITEMLIST items;

	ZGetScreenEffectManager()->DrawScoreBoard();

	MFont *pFont=GetGameFont();
	pDC->SetFont(pFont);
	pFont=pDC->GetFont();	// 만약 폰트가 없으면 다시 디폴트 폰트를 얻는다
	pDC->SetColor(MCOLOR(TEXT_COLOR_TITLE));

	char szText[256];

	// 첫번째 줄 앞 : 클랜 이름 혹은 방제
	if(bClanGame)
	{
		// 클랜전일 경우 클랜 이름을 표시한다
		int nRed = 0, nBlue = 0;

		for (ZCharacterManager::iterator itor = ZGetCharacterManager()->begin();
			itor != ZGetCharacterManager()->end(); ++itor)
		{
			ZCharacter* pCharacter = (*itor).second;

			if(pCharacter->GetTeamID() == MMT_BLUE) nBlue ++;
			if(pCharacter->GetTeamID() == MMT_RED) nRed ++;
		}

		char nvsn[32];
		sprintf_s(nvsn,"%d:%d",nRed,nBlue);
		ZTransMsg( szText, MSG_GAME_SCORESCREEN_STAGENAME, 3, nvsn, m_szRedClanName, m_szBlueClanName );
		
	}
	else
	{
		// 클랜전이 아니면 방제를 표시한다
		sprintf_s(szText, "(%03d) %s", ZGetGameClient()->GetStageNumber(), ZGetGameClient()->GetStageName());
	}
	TextRelative(pDC,0.26f,0.22f,szText);


	float x = 0.27f;
	float y = 0.284f;
	float linespace2= 0.071f / 3.f;

	// 세번째줄 앞 : 점수(팀플)
	if (ZApplication::GetGame()->GetMatch()->IsTeamPlay())
	{
		if ( bClanGame)
		{
			sprintf_s(szText, "%d (%s)  VS  %d (%s)", 
				g_pGame->GetMatch()->GetTeamScore(MMT_RED), m_szRedClanName,
				g_pGame->GetMatch()->GetTeamScore(MMT_BLUE), m_szBlueClanName);
		}
		else
		{
			if (ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2) // 팀전일때 무한데스매치만 예외가 많이 발생합니다 =_=;
				sprintf_s(szText, "%s : %d(Red) vs %d(Blue)",  ZGetGameTypeManager()->GetGameTypeStr(g_pGame->GetMatch()->GetMatchType()),
															g_pGame->GetMatch()->GetTeamKills(MMT_RED), 
															g_pGame->GetMatch()->GetTeamKills(MMT_BLUE));
			else
				sprintf_s(szText, "%s : %d(Red) vs %d(Blue)",  ZGetGameTypeManager()->GetGameTypeStr(g_pGame->GetMatch()->GetMatchType()),
															g_pGame->GetMatch()->GetTeamScore(MMT_RED), 
															g_pGame->GetMatch()->GetTeamScore(MMT_BLUE));
		}
	}
	else
		sprintf_s(szText, "%s", ZGetGameTypeManager()->GetGameTypeStr(g_pGame->GetMatch()->GetMatchType()));
	TextRelative(pDC,x,y,szText);
	y-=linespace2;


	// 두번째줄 앞 : 맵이름
   	strcpy_safe( szText, ZGetGameClient()->GetMatchStageSetting()->GetMapName());
	if ( ZGetGameTypeManager()->IsQuestOnly(g_pGame->GetMatch()->GetMatchType()))		// 퀘스트 모드이면...
	{
	   	strcpy_safe( szText, "Mansion");			// 임시 하드코딩 우에엥~ ㅠ.ㅠ
		if ( ZGetQuest()->GetGameInfo()->GetQuestLevel() > 0)
		{
   			strcat( szText, " (");

			for ( int i = 0;  i < ZGetQuest()->GetGameInfo()->GetQuestLevel();  i++)	// 나중에 퀘스트 레벨에 맞게 표시하도록 바꾼다
    			strcat( szText, ZMsg( MSG_WORD_QUESTLEVELMARKER));

   			strcat( szText, ")");
		}
	}
	TextRelative(pDC,x,y,szText);


	x = 0.70f;
	y = 0.284f;

	// 보상 아이템
	if ( ZGetGameTypeManager()->IsQuestOnly( g_pGame->GetMatch()->GetMatchType()))		// 퀘스트 모드일경우
	{
		sprintf_s( szText, "%s : %d", ZMsg( MSG_WORD_GETITEMQTY), ZGetQuest()->GetGameInfo()->GetNumOfObtainQuestItem());
		TextRelative( pDC, x, y, szText);
		y -= linespace2;
	}

	// NPC 수
	if ( ZGetGameTypeManager()->IsQuestDerived( g_pGame->GetMatch()->GetMatchType())) 	// 서바이벌, 퀘스트 모드일 경우
	{
		sprintf_s( szText, "%s : %d", ZMsg( MSG_WORD_REMAINNPC), ZGetQuest()->GetGameInfo()->GetNPCCount() - ZGetQuest()->GetGameInfo()->GetNPCKilled());
		TextRelative( pDC, x, y, szText);
		y -= linespace2;
	}

	// 진행도 표시
	if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
	{
		if ( g_pGame->GetMatch()->GetRoundState() == MMATCH_ROUNDSTATE_PLAY)				// 플레이 중이라면
		{
			DWORD dwTime = g_pGame->GetMatch()->GetRemaindTime();
			DWORD dwLimitTime = ZGetGameClient()->GetMatchStageSetting()->GetStageSetting()->nLimitTime;
			if ( dwLimitTime != 99999)
			{
				dwLimitTime *= 60000;
				if ( dwTime <= dwLimitTime)
				{
					dwTime = (dwLimitTime - dwTime) / 1000;
					sprintf_s( szText, "%s : %d:%02d", ZMsg( MSG_WORD_REMAINTIME), (dwTime / 60), (dwTime % 60));
				}
				else
					sprintf_s( szText, "%s :", ZMsg( MSG_WORD_REMAINTIME));
			}
			else
				sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME));
		}
		else
			sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME));

		TextRelative( pDC, x, y, szText);
		y -= linespace2;

		
		sprintf_s( szText, "%s : %d", ZMsg( MSG_WORD_ENDKILL), g_pGame->GetMatch()->GetRoundCount());
	}

	// 라운드를 기다려야 하는 종류라면 라운드 표시 아니면 시간 표시
	else if ( ZApplication::GetGame()->GetMatch()->IsWaitForRoundEnd() && !bClanGame)
	{
		if ( g_pGame->GetMatch()->GetRoundState() == MMATCH_ROUNDSTATE_PLAY)				// 플레이 중이라면
		{
			DWORD dwTime = g_pGame->GetMatch()->GetRemaindTime();
			DWORD dwLimitTime = ZGetGameClient()->GetMatchStageSetting()->GetStageSetting()->nLimitTime;
			if ( dwLimitTime != 99999)
			{
				dwLimitTime *= 60000;
				if ( dwTime <= dwLimitTime)
				{
					dwTime = (dwLimitTime - dwTime) / 1000;
					sprintf_s( szText, "%s : %d:%02d", ZMsg( MSG_WORD_REMAINTIME), (dwTime / 60), (dwTime % 60));
				}
				else
					sprintf_s( szText, "%s :", ZMsg( MSG_WORD_REMAINTIME));
			}
			else
				sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME));
		}
		else
			sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME)); 

		TextRelative( pDC, x, y, szText);
		y -= linespace2;

		sprintf_s( szText, "%s : %d / %d %s", ZMsg( MSG_WORD_RPROGRESS), g_pGame->GetMatch()->GetCurrRound() + 1, g_pGame->GetMatch()->GetRoundCount(), ZMsg( MSG_WORD_ROUND));
	}
	else if ( ZGetGameTypeManager()->IsQuestDerived(g_pGame->GetMatch()->GetMatchType())) 	// 서바이벌, 퀘스트 모드일 경우
		sprintf_s( szText, "%s : %d / %d", ZMsg( MSG_WORD_RPROGRESS), ZGetQuest()->GetGameInfo()->GetCurrSectorIndex() + 1, ZGetQuest()->GetGameInfo()->GetMapSectorCount());
	// 남은 시간 표시( 클랜전 제외)
	else if ( !bClanGame)
	{
		if ( g_pGame->GetMatch()->GetRoundState() == MMATCH_ROUNDSTATE_PLAY)				// 플레이 중이라면
		{
			DWORD dwTime = g_pGame->GetMatch()->GetRemaindTime();
			DWORD dwLimitTime = ZGetGameClient()->GetMatchStageSetting()->GetStageSetting()->nLimitTime;
			if ( dwLimitTime != 99999)
			{
				dwLimitTime *= 60000;
				if ( dwTime <= dwLimitTime)
				{
					dwTime = (dwLimitTime - dwTime) / 1000;
					sprintf_s( szText, "%s : %d:%02d", ZMsg( MSG_WORD_REMAINTIME), (dwTime / 60), (dwTime % 60));
				}
				else
					sprintf_s( szText, "%s :", ZMsg( MSG_WORD_REMAINTIME));
			}
			else
				sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME));
		}
		else
			sprintf_s( szText, "%s : ---", ZMsg( MSG_WORD_REMAINTIME));

		TextRelative( pDC, x, y, szText);
		y -= linespace2;
		
		sprintf_s( szText, "%s : %d", ZMsg( MSG_WORD_ENDKILL), g_pGame->GetMatch()->GetRoundCount());

	}
	TextRelative( pDC, x, y, szText);


//	const float normalXPOS[6] = {0.25f, 0.46f, 0.68f, 0.77f, 0.83f, 0.92f};
//	const float clanXPOS[6] = {0.45f, 0.25f, 0.68f, 0.77f, 0.83f, 0.92f};
	const float normalXPOS[6] = {0.28f, 0.47f, 0.67f, 0.78f, 0.84f, 0.93f};
	const float clanXPOS[6]   = {0.44f, 0.24f, 0.67f, 0.76f, 0.82f, 0.91f};
	
	const float *ITEM_XPOS = bClanGame ? clanXPOS : normalXPOS;

	// player
	// 화면의 xy 상대위치 ( 0~1 로봤을때 ) 는 0.25 , 0.361
	y=0.343f;
	const float fHeight=0.578f;	// 공간의 높이

	// 컬럼 타이틀 출력
	char szBuff[ 25];
	pDC->SetColor(MCOLOR(TEXT_COLOR_TITLE));
	x = ITEM_XPOS[0];	// level, 이름
	sprintf_s( szBuff, "%s   %s", ZMsg(MSG_CHARINFO_LEVEL), ZMsg(MSG_CHARINFO_NAME));
	TextRelative(pDC,x,y, szBuff);
	x = ITEM_XPOS[1] + .02f;;	// Clan
	TextRelative(pDC,x,y, ZMsg(MSG_CHARINFO_CLAN));
	if ( ZGetGameTypeManager()->IsQuestDerived( g_pGame->GetMatch()->GetMatchType()))
	{
		x = ITEM_XPOS[2];	// HP/AP
		sprintf_s( szBuff, "%s/%s", ZMsg(MSG_CHARINFO_HP), ZMsg(MSG_CHARINFO_AP));
		TextRelative(pDC,x,y, szBuff);
	}
	else
	{
		x = ITEM_XPOS[2] - .01f;	// Exp
		TextRelative(pDC,x,y, ZMsg(MSG_WORD_EXP));
	}
	x = ITEM_XPOS[3] - .01f;	// Kills
	TextRelative(pDC,x,y, ZMsg(MSG_WORD_KILL));
	x = ITEM_XPOS[4] - .01f;	// Deaths
	TextRelative(pDC,x,y, ZMsg(MSG_WORD_DEATH));
	x = ITEM_XPOS[5] - .01f;	// Ping
	TextRelative(pDC,x,y, ZMsg(MSG_WORD_PING));
	
	float fTitleHeight = (float)pFont->GetHeight()*1.1f / (float)RGetScreenHeight();
	y+=fTitleHeight;

	// 그릴수있는 최대줄수 줄간격은 150%
//	int nMaxLineCount=int((fHeight-fTitleHeight)*(float)RGetScreenHeight()/((float)pFont->GetHeight()*1.1f));
	int nMaxLineCount = 16;
	
	// 한줄사이의 간격(높이)
	float linespace=(fHeight-fTitleHeight)/(float)nMaxLineCount;

	// 좌측의 클랜마크및 이름,점수
	if(bClanGame)
	{
		for(int i=0;i<2;i++)
		{
			MMatchTeam nTeam = (i==0) ? MMT_RED : MMT_BLUE;
			char *szClanName = (i==0) ? m_szRedClanName : m_szBlueClanName;
			int nClanID = (i==0) ? m_nClanIDRed : m_nClanIDBlue;

			MFont *pClanFont=MFontManager::Get("FONTb11b");
			if (pClanFont == NULL) _ASSERT(0);
			pDC->SetFont(pClanFont);
			pDC->SetColor(MCOLOR(TEXT_COLOR_CLAN_NAME));

			float clancenter = .5f*(ITEM_XPOS[0]-ITEM_XPOS[1]) + ITEM_XPOS[1];
			float clanx = clancenter - .5f*((float)pClanFont->GetWidth(szClanName)/(float)MGetWorkspaceWidth());
			float clany = y + linespace * ((nTeam==MMT_RED) ? .5f : 8.5f) ;

			// 아이콘 출력
			MBitmap *pbmp = ZGetEmblemInterface()->GetClanEmblem(nClanID);
			if(pbmp) {
				pDC->SetBitmap(pbmp);

				const float fIconSize = .1f;
				int nIconSize = fIconSize * MGetWorkspaceWidth();

				int screenx=(clancenter-.5f*fIconSize)*MGetWorkspaceWidth();
				int screeny=(clany)*MGetWorkspaceHeight();

				pDC->Draw(screenx,screeny,nIconSize,nIconSize);

				clany+=fIconSize+1.2*linespace;
			}

			// 팀이름 출력
			TextRelative(pDC,clanx,clany ,szClanName);

			// 점수 출력. 가운데 정렬
			sprintf_s(szText,"%d",g_pGame->GetMatch()->GetTeamScore(nTeam));
			clanx = clancenter - .5f*((float)pClanFont->GetWidth(szText)/(float)MGetWorkspaceWidth());
			clany+=1.f*linespace;
			TextRelative(pDC,clanx,clany,szText);

		}
	}


	// 캐릭터 리스트
	ZCharacterManager::iterator itor;
	for (itor = ZGetCharacterManager()->begin();
		itor != ZGetCharacterManager()->end(); ++itor)
	{
		ZCharacter* pCharacter = (*itor).second;

		if(pCharacter->GetTeamID() == MMT_SPECTATOR) continue;

		if(pCharacter->IsAdminHide()) continue;

		ZScoreBoardItem *pItem=new ZScoreBoardItem;

		sprintf_s(pItem->szName, "%d%s %s", pCharacter->GetProperty()->nLevel, ZMsg(MSG_CHARINFO_LEVELMARKER), pCharacter->GetUserName());

		if(pCharacter->IsAdmin())
			pItem->SetColor(ZCOLOR_ADMIN_NAME);

		memcpy(pItem->szClan,pCharacter->GetProperty()->szClanName,CLAN_NAME_LENGTH);
		
		pItem->nClanID = pCharacter->GetClanID();
		pItem->nTeam = ZApplication::GetGame()->GetMatch()->IsTeamPlay() ? pCharacter->GetTeamID() : MMT_END;
		pItem->bDeath = pCharacter->IsDie();
		if ( ZGetGameTypeManager()->IsQuestDerived( g_pGame->GetMatch()->GetMatchType()))
			pItem->nExp = pCharacter->GetStatus()->nKills * 100;
		else
			pItem->nExp = pCharacter->GetStatus()->nExp;
		pItem->nKills = pCharacter->GetStatus()->nKills;
		pItem->nDeaths = pCharacter->GetStatus()->nDeaths;
		pItem->uidUID = pCharacter->GetUID();

		int nPing = (pCharacter->GetUID() == ZGetGameClient()->GetPlayerUID() ? 0 : MAX_PING);
		MMatchPeerInfo* pPeer = ZGetGameClient()->FindPeer(pCharacter->GetUID());
		if (pPeer) {
			if ( g_pGame->IsReplay())
				nPing = 0;
			else
				nPing = pPeer->GetPing(ZApplication::GetGame()->GetTickTime());
		}
		pItem->nPing = nPing;
		pItem->bMyChar = pCharacter->IsHero();
		
		if(ZGetGame()->m_pMyCharacter->GetTeamID()==MMT_SPECTATOR &&
			m_Observer.IsVisible() && m_Observer.GetTargetCharacter()==pCharacter)
			pItem->bMyChar = true;


		
		if(pCharacter->GetTeamID()==g_pGame->m_pMyCharacter->GetTeamID() && pCharacter->m_bCommander)
			pItem->bCommander = true;
		else
			pItem->bCommander = false;

		// 버서커모드의 버서커
		if (g_pGame->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_BERSERKER)
		{
			if (pCharacter->IsTagger()) pItem->bCommander = true;
		}

		if (g_pGame->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
		{
			ZRuleDuel* pDuel = (ZRuleDuel*)g_pGame->GetMatch()->GetRule();	// 위험 -_-
			pItem->nDuelQueueIdx = pDuel->GetQueueIdx(pCharacter->GetUID());
		}
		else
			pItem->nDuelQueueIdx = 0;


		// GameRoom User
		MMatchObjCache* pCache = ZGetGameClient()->FindObjCache( pCharacter->GetUID());
		if ( pCache)
			pItem->bGameRoomUser = (pCache->GetPGrade() == MMPG_PREMIUM_IP) ? true : false;
		else
			pItem->bGameRoomUser = false;

		items.push_back(pItem);
	}


	items.sort(CompareZScoreBoardItem);
	ZSCOREBOARDITEMLIST::iterator i;

	int nCurrentTeamIndex;
	if(ZApplication::GetGame()->GetMatch()->IsTeamPlay())
		nCurrentTeamIndex=MMT_RED;
	else
	{
		if (items.size() > 0) 
			nCurrentTeamIndex = (*items.begin())->nTeam;
	}

	int nCount = 0;
	
	for(i=items.begin();i!=items.end();i++)
	{
		ZScoreBoardItem *pItem=*i;

		if(nCurrentTeamIndex!=pItem->nTeam)	// 팀이 바뀌면
		{
			nCurrentTeamIndex=pItem->nTeam;
			nCount = max(nCount,min(8,nMaxLineCount - ((int)items.size()-nCount)));
		}

		float itemy = y + linespace * nCount;

		nCount++;

		if(nCount>nMaxLineCount) break;

		// 배경 색깔을 결정한다
		MCOLOR backgroundcolor;
		if(pItem->bMyChar) {
			backgroundcolor = BACKGROUND_COLOR_MYCHAR_DEATH_MATCH;
			if(!bClanGame) {
				backgroundcolor = 
					(pItem->nTeam==MMT_RED) ? BACKGROUND_COLOR_MYCHAR_RED_TEAM :
				(pItem->nTeam==MMT_BLUE ) ? BACKGROUND_COLOR_MYCHAR_BLUE_TEAM :
				BACKGROUND_COLOR_MYCHAR_DEATH_MATCH;
			}
		}

		if(pItem->bCommander) backgroundcolor = BACKGROUND_COLOR_COMMANDER;

		if(pItem->bMyChar || pItem->bCommander)
		{
			int y1 = itemy * MGetWorkspaceHeight();
			int y2 = (y + linespace * nCount) * MGetWorkspaceHeight();

			int x1 = bClanGame ? 0.43*MGetWorkspaceWidth() : 0.255*MGetWorkspaceWidth();
			int x2 = (0.715+0.26)*MGetWorkspaceWidth();

			pDC->SetColor(backgroundcolor);
			pDC->FillRectangle(x1,y1,x2-x1,y2-y1);
		}

//		backgroundy=newbackgroundy;


		// PC방 유저일 경우에 PC방 마크를 표시한다.
		if ( pItem->bGameRoomUser)	
		{
			pDC->SetBitmap( MBitmapManager::Get( "icon_gameroom_s.tga"));
			int nIconSize = .8f * linespace * (float)MGetWorkspaceHeight();
			float icony = itemy + (linespace - (float)nIconSize / (float)MGetWorkspaceHeight())*.5f;
			int screenx = ( ITEM_XPOS[0] - 0.024f) * MGetWorkspaceWidth();
			int screeny = icony * MGetWorkspaceHeight();
			pDC->Draw( screenx, screeny, nIconSize+4, nIconSize);
		}


		// 글자 색깔을 결정한다.. (팀과 생사여부)
		MCOLOR textcolor=pItem->bDeath ? TEXT_COLOR_DEATH_MATCH_DEAD : TEXT_COLOR_DEATH_MATCH;

		if(!bClanGame)
		{
			if(pItem->nTeam==MMT_RED)		// red
				textcolor=pItem->bDeath ? TEXT_COLOR_RED_TEAM_DEAD : TEXT_COLOR_RED_TEAM ;
			else
				if(pItem->nTeam==MMT_BLUE)		// blue
					textcolor=pItem->bDeath ? TEXT_COLOR_BLUE_TEAM_DEAD : TEXT_COLOR_BLUE_TEAM ;
				else
					if(pItem->nTeam==MMT_SPECTATOR)
						textcolor = TEXT_COLOR_SPECTATOR;

		}

		if(pItem->bSpColor)	// 특수한 유저라면..고유의 컬러를 가지고 있다.
		{
			if(!pItem->bDeath)
				textcolor = pItem->GetColor();
			else 
				textcolor = 0xff402010;
		}

		pDC->SetColor(textcolor);
		pDC->SetFont(pFont);

		// 글자를 가운데 정렬하기 위해 ..
		float texty= itemy + (linespace - (float)pFont->GetHeight() / (float)MGetWorkspaceHeight())*.5f;
		x = ITEM_XPOS[0];
		TextRelative(pDC,x,texty,pItem->szName);

		if(!bClanGame)
		{
			x = ITEM_XPOS[1];

			int nIconSize = .8f * linespace * (float)MGetWorkspaceHeight();
			float icony = itemy + (linespace - (float)nIconSize / (float)MGetWorkspaceHeight())*.5f;

			if(pItem->szClan[0]) {
				MBitmap *pbmp = ZGetEmblemInterface()->GetClanEmblem(pItem->nClanID);
				if(pbmp) {
					pDC->SetBitmap(pbmp);
					int screenx=x*MGetWorkspaceWidth();
					int screeny=icony*MGetWorkspaceHeight();

					pDC->Draw(screenx,screeny,nIconSize,nIconSize);

				}
			}
			x+= (float)nIconSize/(float)MGetWorkspaceWidth() +0.005f;
			TextRelative(pDC,x,texty,pItem->szClan);
		}

		if ( ZGetGameTypeManager()->IsQuestDerived( g_pGame->GetMatch()->GetMatchType()))
		{
			ZCharacterManager::iterator itor = ZApplication::GetGame()->m_CharacterManager.find( pItem->uidUID);
			if ( itor != ZApplication::GetGame()->m_CharacterManager.end())
			{
				ZCharacter* pQuestPlayerInfo = (*itor).second;

				MCOLOR tmpColor = pDC->GetColor();

				x=ITEM_XPOS[2];
//				sprintf_s(szText,"%d/%d", pQuestPlayerInfo->GetHP(), pQuestPlayerInfo->GetAP());
//				TextRelative(pDC,x,texty,szText);
				pDC->SetColor( MCOLOR( 0x40FF0000));
				pDC->FillRectangle( (x*MGetWorkspaceWidth()), texty*MGetWorkspaceHeight()+1, 0.08*MGetWorkspaceWidth(), 7);
				float nValue = 0.08 * pQuestPlayerInfo->GetHP() / pQuestPlayerInfo->GetProperty()->fMaxHP;
				pDC->SetColor( MCOLOR( 0x90FF0000));
				pDC->FillRectangle( (x*MGetWorkspaceWidth()), texty*MGetWorkspaceHeight()+1, nValue*MGetWorkspaceWidth(), 7);

				pDC->SetColor( MCOLOR( 0x4000FF00));
				pDC->FillRectangle( (x*MGetWorkspaceWidth()), texty*MGetWorkspaceHeight()+9, 0.08*MGetWorkspaceWidth(), 3);
				nValue = 0.08 * pQuestPlayerInfo->GetAP() / pQuestPlayerInfo->GetProperty()->fMaxAP;
				pDC->SetColor( MCOLOR( 0x9000FF00));
				pDC->FillRectangle( (x*MGetWorkspaceWidth()), texty*MGetWorkspaceHeight()+9, nValue*MGetWorkspaceWidth(), 3);

				pDC->SetColor( tmpColor);

				x=ITEM_XPOS[3];
				int nKills = 0;
				ZModule_QuestStatus* pMod = (ZModule_QuestStatus*)pQuestPlayerInfo->GetModule(ZMID_QUESTSTATUS);
				if (pMod)
					nKills = pMod->GetKills();
				sprintf_s(szText,"%d", nKills);
				TextRelative(pDC,x,texty,szText,true);
			}
		}
		else
		{
			x=ITEM_XPOS[2];
			sprintf_s(szText,"%d",pItem->nExp);
			TextRelative(pDC,x,texty,szText,true);

			MCOLOR color = pDC->GetColor();

			if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
			{
				if(!pItem->bDeath)
					pDC->SetColor( 200, 0, 0);
				else
					pDC->SetColor( 120, 0, 0);
			}

			x=ITEM_XPOS[3];
			sprintf_s(szText,"%d",pItem->nKills);
			TextRelative(pDC,x,texty,szText,true);

			pDC->SetColor( color);
		}

		x=ITEM_XPOS[4];
		sprintf_s(szText,"%d",pItem->nDeaths);
		TextRelative(pDC,x,texty,szText,true);

		x=ITEM_XPOS[5];
		sprintf_s(szText,"%d",pItem->nPing);
		TextRelative(pDC,x,texty,szText,true);

//		y+=linespace;
	}

	while(!items.empty())
	{
		delete *items.begin();
		items.erase(items.begin());
	}

	char buf[256] = "Spectators: ";
	int count = 0;
	for (auto it : *ZGetCharacterManager())
	{
		ZCharacter &Player = *it.second;

		if (Player.GetTeamID() != MMT_SPECTATOR)
			continue;

		if (count)
			strcat_safe(buf, ", ");

		strcat_safe(buf, Player.GetUserNameA());
		count++;
	}

	if (count)
		TextRelative(pDC, 0.2, 0.8, buf);
}

// 팀 / 생사 / 성적이 소트의 기준이다
bool CompareZResultBoardItem(ZResultBoardItem* a,ZResultBoardItem* b) {
	if( a->nScore > b->nScore) return true;
	else if( a->nScore < b->nScore) return false;

	if( a->nKills > b->nKills) return true;
	else if( a->nKills < b->nKills) return false;

	if( a->nDeaths < b->nDeaths) return true;
	else if( a->nDeaths > b->nDeaths) return false;

	return false;
}

void AddCombatResultInfo( const char* szName, int nScore, int nKill, int nDeath, int bMyChar, bool bGameRoomUser)
{
	char szText[ 128];

	MTextArea* pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerNameList");
	if ( pWidget)
		pWidget->AddText( szName, ( bMyChar ? MCOLOR( 0xFFFFF794) : MCOLOR( 0xFFFFF794)));

	for ( int i = 0;  i < 16;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "CombatResult_PlayerScore%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			if ( strcmp( pLabel->GetText(), "") == 0)
			{
				sprintf_s( szText, "%d", nScore);
				pLabel->SetText( szText);
				pLabel->SetAlignment( MAM_RIGHT);

				sprintf_s( szWidget, "CombatResult_GameRoomImg%02d", i);
				MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
				if ( pWidget)
					pWidget->Show( bGameRoomUser);

				break;
			}
		}
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerKillList");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nKill);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerDeathList");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nDeath);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}
}


void AddClanResultInfoWin( const char* szName, int nScore, int nKill, int nDeath, int bMyChar, bool bGameRoomUser)
{
	char szText[125];

	MTextArea* pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameList1");
	if ( pWidget)
		pWidget->AddText( szName, ( bMyChar ? MCOLOR( 0xFFFFF794) : MCOLOR( 0xFFFFF794)));

	for ( int i = 0;  i < 4;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "ClanResult_PlayerScore1%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			if ( strcmp( pLabel->GetText(), "") == 0)
			{
				sprintf_s( szText, "%d", nScore);
				pLabel->SetText( szText);
				pLabel->SetAlignment( MAM_RIGHT);

				sprintf_s( szWidget, "ClanResult_GameRoomImg1%d", i);
				MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
				if ( pWidget)
					pWidget->Show( bGameRoomUser);

				break;
			}
		}
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerKillList1");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nKill);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerDeathList1");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nDeath);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}
}

void AddClanResultInfoLose( const char* szName, int nScore, int nKill, int nDeath, int bMyChar, bool bGameRoomUser)
{
	char szText[125];

	MTextArea* pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameList2");
	if ( pWidget)
		pWidget->AddText( szName, ( bMyChar ? MCOLOR( 0xFFFFF794) : MCOLOR( 0xFFFFF794)));

	for ( int i = 0;  i < 4;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "ClanResult_PlayerScore2%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			if ( strcmp( pLabel->GetText(), "") == 0)
			{
				sprintf_s( szText, "%d", nScore);
				pLabel->SetText( szText);
				pLabel->SetAlignment( MAM_RIGHT);

				sprintf_s( szWidget, "ClanResult_GameRoomImg2%d", i);
				MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
				if ( pWidget)
					pWidget->Show( bGameRoomUser);

				break;
			}
		}
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerKillList2");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nKill);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}

	pWidget = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerDeathList2");
	if ( pWidget)
	{
		sprintf_s( szText, "%d", nDeath);
		pWidget->AddText( szText, MCOLOR( 0xFFFFF794));
	}
}


// 결과 화면을 그린다.
void ZCombatInterface::GetResultInfo( void)
{
	// Sort list
#ifdef _DEBUG
	m_ResultItems.push_back(new ZResultBoardItem("test01", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test02", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test03", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test04", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test05", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test06", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test07", "RED Clan",  MMT_RED,  ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test08", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test09", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test10", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test11", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test12", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test13", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
	m_ResultItems.push_back(new ZResultBoardItem("test14", "BLUE Clan", MMT_BLUE, ((rand()%80000)-5000), (rand()%100), (rand()%100)));
#endif
	m_ResultItems.sort( CompareZResultBoardItem);

	// Set UI
	MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult");
	if ( pWidget)  pWidget->Show( false);
	pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult");
	if ( pWidget)  pWidget->Show( false);
	pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult");
	if ( pWidget)  pWidget->Show( false);

	MTextArea* pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerNameList");
	if ( pTextArea)  pTextArea->Clear();
	for ( int i = 0;  i < 16;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "CombatResult_PlayerScore%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			MRECT rect;
			rect = pLabel->GetRect();
			rect.y = pTextArea->GetRect().y + 18 * i - 2;
			rect.h = 21;
			pLabel->SetBounds( rect);

			pLabel->SetText( "");
			pLabel->SetAlignment( MAM_LEFT | MAM_TOP);
		}

		sprintf_s( szWidget, "CombatResult_GameRoomImg%02d", i);
		MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pWidget)
			pWidget->Show( false);
	}
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerKillList");
	if ( pTextArea)  pTextArea->Clear();
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerDeathList");
	if ( pTextArea)  pTextArea->Clear();
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameList1");
	if ( pTextArea)  pTextArea->Clear();
	for ( int i = 0;  i < 4;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "ClanResult_PlayerScore1%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			MRECT rect;
			rect = pLabel->GetRect();
			rect.y = pTextArea->GetRect().y + 18 * i - 2;
			rect.h = 21;
			pLabel->SetBounds( rect);

			pLabel->SetText( "");
			pLabel->SetAlignment( MAM_LEFT | MAM_TOP);
		}

		sprintf_s( szWidget, "ClanResult_GameRoomImg1%d", i);
		MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pWidget)
			pWidget->Show( false);
	}
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerKillList1");
	if ( pTextArea)  pTextArea->Clear();
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerDeathList1");
	if ( pTextArea)  pTextArea->Clear();
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameList2");
	if ( pTextArea)  pTextArea->Clear();
	for ( int i = 0;  i < 4;  i++)
	{
		char szWidget[ 128];
		sprintf_s( szWidget, "ClanResult_PlayerScore2%d", i);
		MLabel* pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pLabel)
		{
			MRECT rect;
			rect = pLabel->GetRect();
			rect.y = pTextArea->GetRect().y + 18 * i - 2;
			rect.h = 21;
			pLabel->SetBounds( rect);

			pLabel->SetText( "");
			pLabel->SetAlignment( MAM_LEFT | MAM_TOP);
		}

		sprintf_s( szWidget, "ClanResult_GameRoomImg2%d", i);
		MWidget* pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szWidget);
		if ( pWidget)
			pWidget->Show( false);
	}
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerKillList2");
	if ( pTextArea)  pTextArea->Clear();
	pTextArea = (MTextArea*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerDeathList2");
	if ( pTextArea)  pTextArea->Clear();


	char szFileName[256];
	szFileName[0] = 0;

	// Set player info
	if ( ZGetGameTypeManager()->IsQuestOnly(g_pGame->GetMatch()->GetMatchType()))	// 퀘스트이면...
	{
		// 초기 UI 설정
		strcpy_safe( szFileName, "interface/loadable/rstbg_quest.jpg");
		pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult");
		if ( pWidget)
			pWidget->Show( true);

		//  경험치 및 바운티 초기화
		ZBmNumLabel* pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetPlusXP");
		if ( pBmNumLabel)
			pBmNumLabel->SetNumber( 0, false);
		pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetMinusXP");
		if ( pBmNumLabel)
			pBmNumLabel->SetNumber( 0, false);
		pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetTotalXP");
		if ( pBmNumLabel)
			pBmNumLabel->SetNumber( 0, false);
		pBmNumLabel = (ZBmNumLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "QuestResult_GetBounty");
		if ( pBmNumLabel)
			pBmNumLabel->SetNumber( 0, false);
	}

	else if ( ZGetGameClient()->IsLadderGame())		// 클랜전이면...
	{
		// 초기 UI 설정
		strcpy_safe( szFileName, "interface/loadable/rstbg_clan.jpg");
		pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult");
		if ( pWidget)
			pWidget->Show( true);

		// Get winner team
		int nWinnerTeam;
		if ( g_pGame->GetMatch()->GetTeamScore( MMT_RED) > g_pGame->GetMatch()->GetTeamScore( MMT_BLUE))
			nWinnerTeam = MMT_RED;
		else
			nWinnerTeam = MMT_BLUE;

		for ( int i = 0;  i < 2;  i++) 
		{
			MMatchTeam nTeam = (i==0) ? MMT_RED : MMT_BLUE;
			char *szClanName = (i==0) ? m_szRedClanName : m_szBlueClanName;
			int nClanID = (i==0) ? m_nClanIDRed : m_nClanIDBlue;

			// Put clan mark
			MPicture* pPicture;
			if ( nWinnerTeam == nTeam)
				pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_ClanBitmap1");
			else
				pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_ClanBitmap2");
			if ( pPicture)
			{
				MBitmap* pBitmap = ZGetEmblemInterface()->GetClanEmblem( nClanID);
				if ( pBitmap)
				{
					pPicture->SetBitmap( pBitmap);
					pPicture->Show( true);
				}
			}

			// Put label
			MLabel* pLabel;
			if ( nWinnerTeam == nTeam)
				pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameListLabel1");
			else
				pLabel = (MLabel*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "ClanResult_PlayerNameListLabel2");
			if ( pLabel)
			{
				pLabel->SetText( szClanName);
				pLabel->Show( true);
			}


			// 게임방 표시 정렬
			int nStartX = 0;
			int nStartY = 0;
			char szName[ 256];
			sprintf_s( szName, "ClanResult_PlayerNameList%d", i+1);
			pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szName);
			if ( pWidget)
			{
				nStartX = pWidget->GetRect().x;
				nStartY = pWidget->GetRect().y;
			}

			for ( int j = 0;  j < 4;  j++)
			{
				char szName[ 256];
				sprintf_s( szName, "ClanResult_GameRoomImg%d%d", i+1, j);
				pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szName);
				if ( pWidget)
				{
					MRECT rect2;
					rect2.x = nStartX - 17;
					rect2.y = 18 * j + nStartY;
					rect2.w = 21;
					rect2.h = 18;

					pWidget->SetBounds( rect2);
				}
			}
		}


		// 팀 정보 추가
		for ( ZResultBoardList::iterator i = m_ResultItems.begin(); i != m_ResultItems.end();  i++)
		{
			ZResultBoardItem *pItem = *i;

			if ( (pItem->nTeam != MMT_RED) && (pItem->nTeam != MMT_BLUE))
				continue;

			// Put info
			if ( nWinnerTeam == pItem->nTeam)
				AddClanResultInfoWin( pItem->szName, pItem->nScore, pItem->nKills, pItem->nDeaths, pItem->bMyChar, pItem->bGameRoomUser);
			else
				AddClanResultInfoLose( pItem->szName, pItem->nScore, pItem->nKills, pItem->nDeaths, pItem->bMyChar, pItem->bGameRoomUser);
		}
	}

	else
	{
		// 초기 UI 설정
		if ( (ZGetLocale()->GetCountry() == MC_US) || (ZGetLocale()->GetCountry() == MC_BRAZIL) || (ZGetLocale()->GetCountry() == MC_INDIA))
		{
			// 인터내셔널 및 브라질 버젼 한정 옵션
			if ( (rand() % 2))
				strcpy_safe( szFileName, "interface/loadable/rstbg_deathmatch.jpg");
			else
				strcpy_safe( szFileName, "interface/loadable/rstbg_clan.jpg");
		}
		else
			strcpy_safe( szFileName, "interface/loadable/rstbg_deathmatch.jpg");

		pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult");
		if ( pWidget)
			pWidget->Show( true);


		int nStartY = 0;
		pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_PlayerNameList");
		if ( pWidget)
			nStartY = pWidget->GetRect().y;


		ZResultBoardList::iterator itrList = m_ResultItems.begin();
		for ( int i = 0;  i < 16;  i++)
		{
			int nTeam = 0;

			if ( itrList != m_ResultItems.end())
			{
				ZResultBoardItem *pItem = *itrList;

				if ( (pItem->nTeam == MMT_RED) || (pItem->nTeam == MMT_BLUE) || (pItem->nTeam == 4))
					AddCombatResultInfo( pItem->szName, pItem->nScore, pItem->nKills, pItem->nDeaths, pItem->bMyChar, pItem->bGameRoomUser);

				nTeam = pItem->nTeam;
				itrList++;
			}


            for ( int j = MMT_RED;  j <= MMT_BLUE;  j++)
			{
				char szName[ 128];
				sprintf_s( szName, "CombatResult_%sTeamBG%02d", ((j==MMT_RED) ? "Red" : "Blue"), i);

				pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szName);
				if ( pWidget)
				{
					MRECT rect;
					rect = pWidget->GetRect();
					rect.y = 18 * i + nStartY;
					rect.h = 18;

					pWidget->SetBounds( rect);
		
					if ( nTeam == j)
						pWidget->Show( true);
					else
						pWidget->Show( false);

					pWidget->SetOpacity( 110);


					// 좀 안좋은 위치지만...  -_-;
					sprintf_s( szName, "CombatResult_GameRoomImg%02d", i);
					pWidget = ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( szName);
					if ( pWidget)
					{
						MRECT rect2;
						rect2 = pWidget->GetRect();
						rect2.x = rect.x - 20;
						rect2.y = 18 * i + nStartY;
						rect2.w = 21;
						rect2.h = 18;

						pWidget->SetBounds( rect2);
					}
				}
			}
		}


		// 이미지 설정
		MPicture* pPicture;
		if ( g_pGame->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2)		// 으아아아악
		{
			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_Finish");
			if ( pPicture)
				pPicture->Show( false);

			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_WinLoseDraw");
			if ( pPicture)
			{
				if ( g_pGame->GetMatch()->GetTeamKills( MMT_RED) == g_pGame->GetMatch()->GetTeamKills( MMT_BLUE))
					pPicture->SetBitmap( MBitmapManager::Get( "result_draw.tga"));
				else
				{
					if ( g_pGame->GetMatch()->GetTeamKills( MMT_RED) > g_pGame->GetMatch()->GetTeamKills( MMT_BLUE))
					{
						if ( g_pGame->m_pMyCharacter->GetTeamID() == MMT_RED)
							pPicture->SetBitmap( MBitmapManager::Get( "result_win.tga"));
						else
							pPicture->SetBitmap( MBitmapManager::Get( "result_lose.tga"));
					}
					else
					{
						if ( g_pGame->m_pMyCharacter->GetTeamID() == MMT_BLUE)
							pPicture->SetBitmap( MBitmapManager::Get( "result_win.tga"));
						else
							pPicture->SetBitmap( MBitmapManager::Get( "result_lose.tga"));
					}
				}

				pPicture->Show( true);
			}
		}
		else if ( ZGetGameInterface()->m_bTeamPlay)		// 팀전이면...
		{
			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_Finish");
			if ( pPicture)
				pPicture->Show( false);

			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_WinLoseDraw");
			if ( pPicture)
			{
				if ( g_pGame->GetMatch()->GetTeamScore( MMT_RED) == g_pGame->GetMatch()->GetTeamScore( MMT_BLUE))
					pPicture->SetBitmap( MBitmapManager::Get( "result_draw.tga"));
				else
				{
					if ( g_pGame->GetMatch()->GetTeamScore( MMT_RED) > g_pGame->GetMatch()->GetTeamScore( MMT_BLUE))
					{
						if ( g_pGame->m_pMyCharacter->GetTeamID() == MMT_RED)
							pPicture->SetBitmap( MBitmapManager::Get( "result_win.tga"));
						else
							pPicture->SetBitmap( MBitmapManager::Get( "result_lose.tga"));
					}
					else
					{
						if ( g_pGame->m_pMyCharacter->GetTeamID() == MMT_BLUE)
							pPicture->SetBitmap( MBitmapManager::Get( "result_win.tga"));
						else
							pPicture->SetBitmap( MBitmapManager::Get( "result_lose.tga"));
					}
				}

				pPicture->Show( true);
			}
		}
		else										// 개인전이면...
		{
			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_Finish");
			if ( pPicture)
				pPicture->Show( true);

			pPicture = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "CombatResult_WinLoseDraw");
			if ( pPicture)
				pPicture->Show( false);
		}
	}


	// 배경이미지 로딩
	m_pResultBgImg = new MBitmapR2;
	((MBitmapR2*)m_pResultBgImg)->Create( "resultbackground.png", RGetDevice(), szFileName);
	if ( m_pResultBgImg != NULL)
	{
		// 읽어온 비트맵 이미지 포인터를 해당 위젯에 넘겨줘서 표시한다
		MPicture* pBgImage = (MPicture*)ZApplication::GetGameInterface()->GetIDLResource()->FindWidget( "GameResult_Background");
		if ( pBgImage)
			pBgImage->SetBitmap( m_pResultBgImg->GetSourceBitmap());
	}
}

void ZCombatInterface::DrawResultBoard(MDrawContext* pDC)
{

	// 클랜전일때 내 팀을 왼편에, 상대팀을 오른쪽에 보여준다

	bool bClanGame = ZGetGameClient()->IsLadderGame();
//	bool bClanGame = true;

	if(!m_pResultPanel) return;

	m_pResultPanel->Draw(0);

	if(m_pResultPanel_Team)
		m_pResultPanel_Team->Draw(0);

	/*
	if(m_pResultPanel->GetVMesh()->isOncePlayDone())
	{
		SAFE_DELETE(m_pResultPanel);
		return;
	}
	*/

#define FADE_START_FRAME	20000

	RVisualMesh *pvm=m_pResultPanel->GetVMesh();
	if(bClanGame && pvm->isOncePlayDone())
	{
		if(!m_pResultLeft)
		{
			char *szEffectNames[] = { "clan_win", "clan_draw", "clan_lose" };

			int nRed = g_pGame->GetMatch()->GetTeamScore(MMT_RED);
			int nBlue = g_pGame->GetMatch()->GetTeamScore(MMT_BLUE);
			int nLeft,nRight;

			if(g_pGame->m_pMyCharacter->GetTeamID()==MMT_RED) {
				nLeft = (nRed==nBlue) ? 1 : (nRed>nBlue) ? 0 : 2;
			}else
				nLeft = (nRed==nBlue) ? 1 : (nRed<nBlue) ? 0 : 2;
			
			// 오른쪽은 왼쪽의 반대
			nRight = 2 - nLeft;

			m_pResultLeft = ZGetScreenEffectManager()->CreateScreenEffect(szEffectNames[nLeft],
				rvector(-240.f,-267.f,0));
			m_pResultRight = ZGetScreenEffectManager()->CreateScreenEffect(szEffectNames[nRight],
				rvector(240.f,-267.f,0));
		}

		m_pResultLeft->Draw(0);
		m_pResultRight->Draw(0);
	}

	int nFrame = pvm->GetFrameInfo(ani_mode_lower)->m_nFrame;

	float fOpacity=min(1.f,max(0,float(nFrame-FADE_START_FRAME)
		/float(pvm->GetFrameInfo(ani_mode_lower)->m_pAniSet->GetMaxFrame()-FADE_START_FRAME)));

	MFont *pFont=GetGameFont();
	pDC->SetFont(pFont);
	pFont=pDC->GetFont();	// 만약 폰트가 없으면 다시 디폴트 폰트를 얻는다

	MCOLOR opacity=MCOLOR(0,0,0,255*fOpacity);
	pDC->SetOpacity(255*fOpacity);

	float x,y;

	char szText[256];

	x=0.026f;
	y=0.107f;

	const float fHeight=0.651f;	// 공간의 높이

	// 그릴수있는 최대줄수 줄간격은 150%
//	int nMaxLineCount=int(fHeight*RGetScreenHeight()/((float)pFont->GetHeight()*1.5f));
	int nMaxLineCount = 16;

	// 한줄사이의 간격(높이)
	float linespace=fHeight/(float)nMaxLineCount;

	m_ResultItems.sort(CompareZResultBoardItem);

	/*
	m_ResultItems.clear();
	g_pGame->m_pMyCharacter->SetTeamID(MMT_RED);
	m_ResultItems.push_back(new ZResultBoardItem("test1","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test2","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test3","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test4","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test5","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test6","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test7","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test8","지옥의발차기",MMT_RED,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test1","대략낭패",MMT_BLUE,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test2","대략낭패",MMT_BLUE,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test3","대략낭패",MMT_BLUE,0,0,0));
	m_ResultItems.push_back(new ZResultBoardItem("test4","대략낭패",MMT_BLUE,0,0,0,true));
	*/

	if(bClanGame)
	{
		int nLeft = 0;
		int nRight = 0;

		y=0.387f;

		// TODO : 클랜 이름및 emblem 출력은 stagesetting 혹은 match쪽에 정보가 추가되는대로 수정
		for(ZResultBoardList::iterator i=m_ResultItems.begin();i!=m_ResultItems.end();i++)
		{
			ZResultBoardItem *pItem=*i;

			int y1,y2;
			float itemy;

			float clancenter;
			bool bDrawClanName = false;

			MCOLOR backgroundcolor;

			if(pItem->nTeam == g_pGame->m_pMyCharacter->GetTeamID()) {
				x = 0.035f;
				itemy = y + linespace * nLeft;
				nLeft++;
				if(nLeft == 1)
				{
					bDrawClanName = true;
					clancenter = 0.25f;
				}
				backgroundcolor = (nLeft%2==0) ? BACKGROUND_COLOR1 : BACKGROUND_COLOR2;
				y1 = itemy * MGetWorkspaceHeight();
				y2 = (y + linespace * nLeft) * MGetWorkspaceHeight();
			}else {
				x = 0.55f;
				itemy = y + linespace * nRight;
				nRight++;
				if(nRight == 1)
				{
					bDrawClanName = true;
					clancenter = 0.75f;
				}
				backgroundcolor = (nRight%2==1) ? BACKGROUND_COLOR1 : BACKGROUND_COLOR2;
				y1 = itemy * MGetWorkspaceHeight();
				y2 = (y + linespace * nRight) * MGetWorkspaceHeight();
			}

			if(bDrawClanName)
			{
				MCOLOR textcolor = TEXT_COLOR_CLAN_NAME;
				textcolor.a=opacity.a;
				pDC->SetColor(textcolor);

				MFont *pClanFont=MFontManager::Get("FONTb11b");
				if (pClanFont == NULL) _ASSERT(0);
				pDC->SetFont(pClanFont);

				float clanx = clancenter - .5f*(float)pClanFont->GetWidth(pItem->szClan)/(float)MGetWorkspaceWidth();
				TextRelative(pDC,clanx,0.15,pItem->szClan);

				char szText[32];
				sprintf_s(szText,"%d",g_pGame->GetMatch()->GetTeamScore((MMatchTeam)pItem->nTeam));

				clanx = clancenter - .5f*(float)pClanFont->GetWidth(szText)/(float)MGetWorkspaceWidth();
				TextRelative(pDC,clanx,0.2,szText);

				// 칼럼표시
				textcolor = TEXT_COLOR_TITLE;
				textcolor.a=opacity.a;
				pDC->SetColor(textcolor);
				float texty= itemy - linespace + (linespace - (float)pFont->GetHeight() / (float)RGetScreenHeight())*.5f;
				TextRelative(pDC,x,texty,"Level Name");
				TextRelative(pDC,x+.25f,texty,"Exp",true);
				TextRelative(pDC,x+.32f,texty,"Kill",true);
				TextRelative(pDC,x+.39f,texty,"Death",true);

			}

			if(pItem->bMyChar)
				backgroundcolor = BACKGROUND_COLOR_MYCHAR_DEATH_MATCH;
			backgroundcolor.a=opacity.a>>1;
			pDC->SetColor(backgroundcolor);
			pDC->FillRectangle(
				(x-.01f)*MGetWorkspaceWidth(),y1,
				.44f*MGetWorkspaceWidth(),y2-y1);

			MCOLOR textcolor = TEXT_COLOR_DEATH_MATCH;
			textcolor.a=opacity.a;
			pDC->SetColor(textcolor);
			pDC->SetFont(pFont);

			// 글자를 가운데 정렬하기 위해 ..
			float texty= itemy + (linespace - (float)pFont->GetHeight() / (float)RGetScreenHeight())*.5f;
			TextRelative(pDC,x,texty,pItem->szName);

			sprintf_s(szText,"%d",pItem->nScore);
			TextRelative(pDC,x+.25f,texty,szText,true);

			sprintf_s(szText,"%d",pItem->nKills);
			TextRelative(pDC,x+.32f,texty,szText,true);

			sprintf_s(szText,"%d",pItem->nDeaths);
			TextRelative(pDC,x+.39f,texty,szText,true);
		}
	}else
	{
		//	int backgroundy=y*MGetWorkspaceHeight();
		int nCount=0;

		for(ZResultBoardList::iterator i=m_ResultItems.begin();i!=m_ResultItems.end();i++)
		{
			ZResultBoardItem *pItem=*i;

			float itemy = y + linespace * nCount;

			nCount++;

			/*
			// 마지막줄이면 ... 을찍고 넘어간다
			if(nCount==nMaxLineCount)
			{
			pDC->SetColor(MCOLOR(255,255,255,opacity.a));
			x=0.50f;
			TextRelative(pDC,x,y,".....");
			break;
			}
			*/

			// 배경 색깔을 결정한다
			MCOLOR backgroundcolor= (nCount%2==0) ? BACKGROUND_COLOR1 : BACKGROUND_COLOR2;
			if(pItem->bMyChar) backgroundcolor = 
				(pItem->nTeam==MMT_RED) ? BACKGROUND_COLOR_MYCHAR_RED_TEAM :
			(pItem->nTeam==MMT_BLUE ) ? BACKGROUND_COLOR_MYCHAR_BLUE_TEAM :
			BACKGROUND_COLOR_MYCHAR_DEATH_MATCH;

			backgroundcolor.a=opacity.a>>1;
			pDC->SetColor(backgroundcolor);

			int y1 = itemy * MGetWorkspaceHeight();
			int y2 = (y + linespace * nCount) * MGetWorkspaceHeight();

			pDC->FillRectangle(
				0.022f*MGetWorkspaceWidth(),y1,
				0.960*MGetWorkspaceWidth(),y2-y1);
			//		backgroundy=newbackgroundy;

			// 글자 색깔을 결정한다.. 
			MCOLOR textcolor= TEXT_COLOR_DEATH_MATCH ;

			if(pItem->nTeam==MMT_RED)		// red
				textcolor=TEXT_COLOR_RED_TEAM;
			else
				if(pItem->nTeam==MMT_BLUE)		// blue
					textcolor=TEXT_COLOR_BLUE_TEAM;
				else
					if(pItem->nTeam==MMT_SPECTATOR)
						textcolor = TEXT_COLOR_SPECTATOR;

			textcolor.a=opacity.a;
			pDC->SetColor(textcolor);

			// 글자를 가운데 정렬하기 위해 ..
			float texty= itemy + (linespace - (float)pFont->GetHeight() / (float)RGetScreenHeight())*.5f;

			x=0.025f;
			TextRelative(pDC,x,texty,pItem->szName);

			x=0.43f;
			sprintf_s(szText,"%d",pItem->nScore);
			TextRelative(pDC,x,texty,szText,true);

			x=0.52f;
			sprintf_s(szText,"%d",pItem->nKills);
			TextRelative(pDC,x,texty,szText,true);

			x=0.61f;
			sprintf_s(szText,"%d",pItem->nDeaths);
			TextRelative(pDC,x,texty,szText,true);

			const float iconspace=0.053f;

			x=0.705f;

			pDC->SetBitmapColor(MCOLOR(255,255,255,255*fOpacity));

			IconRelative(pDC,x,texty,0);x+=iconspace;
			IconRelative(pDC,x,texty,1);x+=iconspace;
			IconRelative(pDC,x,texty,2);x+=iconspace;
			IconRelative(pDC,x,texty,3);x+=iconspace;
			IconRelative(pDC,x,texty,4);

			pDC->SetBitmapColor(MCOLOR(255,255,255,255));

			x=0.705f+(float(pFont->GetHeight()*1.3f)/MGetWorkspaceWidth());
			sprintf_s(szText,"%d",pItem->nAllKill);
			TextRelative(pDC,x,texty,szText);x+=iconspace;
			sprintf_s(szText,"%d",pItem->nUnbelievable);
			TextRelative(pDC,x,texty,szText);x+=iconspace;
			sprintf_s(szText,"%d",pItem->nExcellent);
			TextRelative(pDC,x,texty,szText);x+=iconspace;
			sprintf_s(szText,"%d",pItem->nFantastic);
			TextRelative(pDC,x,texty,szText);x+=iconspace;
			sprintf_s(szText,"%d",pItem->nHeadShot);
			TextRelative(pDC,x,texty,szText);x+=iconspace;

			//		y+=linespace;
		}
	}
}

void ZCombatInterface::IconRelative(MDrawContext* pDC,float x,float y,int nIcon)
{
	MBitmap *pbmp=m_ppIcons[nIcon];
	if(!pbmp) return;

	pDC->SetBitmap(pbmp);
	int screenx=x*MGetWorkspaceWidth();
	int screeny=y*MGetWorkspaceHeight();

	int nSize=pDC->GetFont()->GetHeight();
	pDC->Draw(screenx,screeny,nSize,nSize);
}

void ZCombatInterface::Finish()
{	
	if ( IsFinish())
		return;

	ZGetFlashBangEffect()->End();

	m_fOrgMusicVolume = ZApplication::GetSoundEngine()->GetMusicVolume();
	m_nReserveFinishTime = timeGetTime();
	m_bReserveFinish = true;

	m_CrossHair.Show(false);

#ifdef _BIRDSOUND

#else
	ZGetSoundEngine()->Set3DSoundUpdate( false );
#endif

}

bool ZCombatInterface::IsFinish()
{
//	if(m_pResultPanel)
//		return m_pResultPanel->GetVMesh()->isOncePlayDone();
//	return false;

	return m_bOnFinish;
}

void ZCombatInterface::OnFinish()
{
	if(m_pResultPanel) return;

	m_pResultLeft = NULL;
	m_pResultRight = NULL;

	ZGetScreenEffectManager()->AddRoundFinish();
	
//	m_pResultPanel=ZGetScreenEffectManager()->CreateScreenEffect("ef_in_result.elu");

	if(ZApplication::GetGame()->GetMatch()->IsTeamPlay() && !ZGetGameClient()->IsLadderGame())
	{
		int nRed = g_pGame->GetMatch()->GetTeamScore(MMT_RED), nBlue = g_pGame->GetMatch()->GetTeamScore(MMT_BLUE);
		if(nRed==nBlue)
			m_pResultPanel_Team = ZGetScreenEffectManager()->CreateScreenEffect("teamdraw");
		else
			if(nRed>nBlue)
				m_pResultPanel_Team = ZGetScreenEffectManager()->CreateScreenEffect("teamredwin");
			else
				m_pResultPanel_Team = ZGetScreenEffectManager()->CreateScreenEffect("teambluewin");
	}

	m_ResultItems.Destroy();

	ZCharacterManager::iterator itor;
	for (itor = ZGetCharacterManager()->begin();
		itor != ZGetCharacterManager()->end(); ++itor)
	{
		ZCharacter* pCharacter = (*itor).second;
		ZResultBoardItem *pItem=new ZResultBoardItem;

		if(pCharacter->IsAdminHide()) continue;

		if(pCharacter->IsAdmin()) {
			sprintf_s(pItem->szName,"--%s  %s", ZMsg(MSG_CHARINFO_LEVELMARKER), pCharacter->GetUserName());
		}
		else {
			sprintf_s(pItem->szName,"%d%s %s",pCharacter->GetProperty()->nLevel, ZMsg(MSG_CHARINFO_LEVELMARKER), pCharacter->GetUserName());
		}

		strcpy_safe(pItem->szClan,pCharacter->GetProperty()->szClanName);
		pItem->nClanID = pCharacter->GetClanID();
		pItem->nTeam = ZApplication::GetGame()->GetMatch()->IsTeamPlay() ? pCharacter->GetTeamID() : MMT_END;
		pItem->nScore = pCharacter->GetStatus()->nExp;
		pItem->nKills = pCharacter->GetStatus()->nKills;
		pItem->nDeaths = pCharacter->GetStatus()->nDeaths;

		pItem->nAllKill= pCharacter->GetStatus()->nAllKill;
		pItem->nExcellent = pCharacter->GetStatus()->nExcellent;
		pItem->nFantastic = pCharacter->GetStatus()->nFantastic;
		pItem->nHeadShot = pCharacter->GetStatus()->nHeadShot;
		pItem->nUnbelievable = pCharacter->GetStatus()->nUnbelievable;

		pItem->bMyChar = pCharacter->IsHero();
	
		MMatchObjCache* pCache = ZGetGameClient()->FindObjCache( pCharacter->GetUID());
		if ( pCache)
			pItem->bGameRoomUser = (pCache->GetPGrade() == MMPG_PREMIUM_IP) ? true : false;
		else
			pItem->bGameRoomUser = false;

		m_ResultItems.push_back(pItem);
	}

	m_Observer.Show(false);

	m_nReservedOutTime = timeGetTime() + 5000;		// 5초 후에 자동 종료.
	m_bOnFinish = true;
}

void ZCombatInterface::SetObserverMode(bool bEnable)
{
	if (bEnable) ZGetScreenEffectManager()->ResetSpectator();
	m_Observer.Show(bEnable);
}


ZCharacter* ZCombatInterface::GetTargetCharacter()
{
	if (m_Observer.IsVisible())
	{
		return m_Observer.GetTargetCharacter();
	}

	return ZApplication::GetGame()->m_pMyCharacter;	
}

MUID ZCombatInterface::GetTargetUID()
{
	return GetTargetCharacter()->GetUID();
}


void ZCombatInterface::GameCheckPickCharacter()
{
	MPOINT Cp = GetCrosshairPoint();

	ZPICKINFO pickinfo;

	rvector pos,dir;
	RGetScreenLine(Cp.x,Cp.y,&pos,&dir);

	ZMyCharacter* pMyChar = NULL;

	pMyChar = g_pGame->m_pMyCharacter;

	bool bPick = false;

	if(g_pGame->Pick(pMyChar,pos,dir,&pickinfo,RM_FLAG_ADDITIVE | RM_FLAG_HIDE,true)) {
		
		if(pickinfo.pObject)	{
			if (pickinfo.info.parts == eq_parts_head) bPick=true;
			bPick = true;
		}
	}

	if(pMyChar && pMyChar->m_pVMesh) {

		RWeaponMotionType type = (RWeaponMotionType)pMyChar->m_pVMesh->GetSetectedWeaponMotionID();

		if( (type==eq_wd_katana) || (type==eq_wd_grenade) || (type==eq_ws_dagger) || (type==eq_wd_dagger) 
			|| (type==eq_wd_item) || (type==eq_wd_sword) || (type==eq_wd_blade) ) {
			bPick = false;
		}

		if(pMyChar->m_pVMesh->m_vRotXYZ.y  > -20.f &&  pMyChar->m_pVMesh->m_vRotXYZ.y < 30.f) {
			bPick = false;
		}

		if(pMyChar->m_pVMesh->m_vRotXYZ.y < -25.f)
			bPick = true;

		if( pMyChar->IsMan() ) { // 모델이 남자고
			if( pMyChar->m_pVMesh->m_vRotXYZ.x < -20.f) {//오른쪽으로 이동중에
				if( RCameraDirection.z < -0.2f)
					bPick = true;
			}
		}

		if( ( pMyChar->m_AniState_Lower == ZC_STATE_LOWER_TUMBLE_RIGHT) || 
			( pMyChar->m_AniState_Lower == ZC_STATE_LOWER_TUMBLE_LEFT) )
		{		
			if( RCameraDirection.z < -0.3f)
				bPick = true;
		}


		if( RCameraDirection.z < -0.6f)
			bPick = true;

		if(bPick) {
			pMyChar->m_pVMesh->SetVisibility(0.4f);
		} else {
			pMyChar->m_pVMesh->SetVisibility(1.0f);
		}
	}

	if(g_pGame->Pick(pMyChar,pos,dir,&pickinfo))
	{
		if(pickinfo.pObject)
		{
			rvector v1;
			v1 = pickinfo.info.vOut;

			if (IsPlayerObject(pickinfo.pObject)) {
				SetPickTarget(true, (ZCharacter*)pickinfo.pObject);
			}
			else
			{
				m_CrossHair.SetState(ZCS_PICKENEMY);
			}
		}else
			SetPickTarget(false);
	}
}

void ZCombatInterface::OnGadget(MMatchWeaponType nWeaponType)
{
	if (m_pWeaponScreenEffect) m_pWeaponScreenEffect->OnGadget(nWeaponType);
	m_CrossHair.Show(false);
}

void ZCombatInterface::OnGadgetOff()
{
	if (m_pWeaponScreenEffect) m_pWeaponScreenEffect->OnGadgetOff();
	m_CrossHair.Show(true);
}


void ZCombatInterface::SetDrawLeaveBattle(bool bShow, int nSeconds)
{
	m_bDrawLeaveBattle = bShow;
	m_nDrawLeaveBattleSeconds = nSeconds;
}

void ZCombatInterface::OnAddCharacter(ZCharacter *pChar)
{
	bool bClanGame = ZGetGameClient()->IsLadderGame();
	if(bClanGame) {
		if (pChar->GetTeamID() == MMT_RED) {
			if(m_nClanIDRed==0) {
				m_nClanIDRed = pChar->GetClanID();
				ZGetEmblemInterface()->AddClanInfo(m_nClanIDRed);
				strcpy_safe(m_szRedClanName,pChar->GetProperty()->szClanName);
			}
		}
		else if (pChar->GetTeamID() == MMT_BLUE) {
			if(m_nClanIDBlue==0) {
				m_nClanIDBlue = pChar->GetClanID();
				ZGetEmblemInterface()->AddClanInfo(m_nClanIDBlue);
				strcpy_safe(m_szBlueClanName,pChar->GetProperty()->szClanName);
			}
		}
	}
}

void ZCombatInterface::ShowChatOutput(bool bShow)
{
	m_Chat.ShowOutput(bShow);
	ZGetConfiguration()->SetViewGameChat(bShow);
}
