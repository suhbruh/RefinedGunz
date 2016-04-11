#include "stdafx.h"

#include "ZCombatChat.h"
#include "ZGameInterface.h"
#include "ZInterfaceItem.h"
#include "ZApplication.h"
#include "ZInterfaceListener.h"
//#include "MListBox.h"
#include "ZIDLResource.h"
#include "ZPost.h"
#include "MChattingFilter.h"
#include "MTextArea.h"
#include "ZConfiguration.h"

#include "NewChat.h"

#define MAX_CHAT_OUTPUT_LINE 7


/////////////////
// ZTabPlayerList
ZTabPlayerList::ZTabPlayerList(const char* szName, MWidget* pParent, MListener* pListener)
: MListBox(szName, pParent, pListener)
{
	SetChatControl(NULL);
}

bool ZTabPlayerList::OnShow(void)
{
	RemoveAll();

	// Push Target List
	for(ZCharacterManager::iterator i=ZGetCharacterManager()->begin(); i!=ZGetCharacterManager()->end();i++)
	{
		ZCharacter *pChar = i->second;
		if(pChar->IsAdminHide()) continue;
		Add(pChar->GetProperty()->szName);
	}

	return true;
}

void ZTabPlayerList::OnHide(void)
{
	if (m_pEditChat)
		m_pEditChat->SetFocus();
}

bool ZTabPlayerList::OnEvent(MEvent* pEvent, MListener* pListener)
{
	if(pEvent->nMessage==MWM_KEYDOWN){
		if(pEvent->nKey==VK_ESCAPE) 
		{
			Show(false);
			return true;
		}
		else if( (pEvent->nKey==VK_TAB) || (pEvent->nKey==VK_RETURN) || (pEvent->nKey==VK_SPACE))
		{
			OnPickPlayer();
			Show(false);
			return true;
		}
	}
	return MListBox::OnEvent(pEvent, pListener);
}

void ZTabPlayerList::OnPickPlayer()
{
	const char* pszString = GetSelItemString();
	if (m_pEditChat && pszString) {
		m_pEditChat->AddText(pszString);
	}
}
/////////////////

class MCombatChatInputListener : public MListener{
public:
	virtual bool OnCommand(MWidget* pWidget, const char* szMessage){
		if(MWidget::IsMsg(szMessage, MEDIT_ENTER_VALUE)==true)
		{
			if (strlen(pWidget->GetText()) >= 256) return false;

			const char* szCommand = pWidget->GetText();
			if (szCommand[0] != '\0')
			{
				char szMsg[512];
				strcpy_safe(szMsg, szCommand);
				strcpy_safe(szMsg, pWidget->GetText());

				if (!ZGetConfiguration()->GetViewGameChat())
				{
					if (ZApplication::GetGameInterface()->GetCombatInterface())
					{
						ZApplication::GetGameInterface()->GetCombatInterface()->ShowChatOutput(true);
					}
				}
				ZApplication::GetGameInterface()->GetChat()->Input(szMsg);
			}

			pWidget->SetText("");
			if (ZApplication::GetGameInterface()->GetCombatInterface())
			{
				ZApplication::GetGameInterface()->GetCombatInterface()->EnableInputChat(false);
			}
			return true;
		}
		else if(MWidget::IsMsg(szMessage, MEDIT_ESC_VALUE)==true)
		{
			pWidget->SetText("");
			ZApplication::GetGameInterface()->GetCombatInterface()->EnableInputChat(false);
		}
		else if ((MWidget::IsMsg(szMessage, MEDIT_CHAR_MSG)==true) || (MWidget::IsMsg(szMessage, MEDIT_KEYDOWN_MSG)==true))
		{
			ZApplication::GetGameInterface()->GetChat()->FilterWhisperKey(pWidget);
		}


		return false;
	}
};
MCombatChatInputListener	g_CombatChatInputListener;

MListener* ZGetCombatChatInputListener(void)
{
	return &g_CombatChatInputListener;
}


ZCombatChat::ZCombatChat()
{
	m_bChatInputVisible = true;
	m_nLastChattingMsgTime = 0;
	m_pIDLResource = ZApplication::GetGameInterface()->GetIDLResource();

	m_pChattingOutput = NULL;
	m_pInputEdit = NULL;
	m_pTabPlayerList = NULL;
	m_bTeamChat = false;
	m_bShowOutput = true;
}

ZCombatChat::~ZCombatChat()
{

}

bool ZCombatChat::Create( const char* szOutputTxtarea,bool bUsePlayerList)
{
	MWidget* pWidget = m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATINPUT);
	if (pWidget!=NULL)
	{
		pWidget->SetListener(ZGetCombatChatInputListener());
	}

	m_pChattingOutput = NULL;
//	pWidget = m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATOUTPUT);
	pWidget = m_pIDLResource->FindWidget( szOutputTxtarea);
	if (pWidget!=NULL)
	{
		m_pChattingOutput = ((MTextArea*)pWidget);
	}

	if (m_pChattingOutput != NULL)
	{
		m_pChattingOutput->Clear();
//		m_pChattingOutput->RemoveAll();
//		m_pChattingOutput->GetScrollBar()->Show(false);
	}
	
	pWidget = m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATINPUT);
	if (pWidget!=NULL)
	{
		m_pInputEdit = (MEdit*)pWidget;
		m_pInputEdit->Show(false);

		if(bUsePlayerList)
		{
			// TabPlayerList
			MWidget* pPivot = m_pInputEdit->GetParent();
			m_pTabPlayerList = new ZTabPlayerList("TabPlayerList", pPivot, ZGetCombatInterface());
			m_pTabPlayerList->Show(false);
			m_pTabPlayerList->SetBounds(m_pInputEdit->GetPosition().x, m_pInputEdit->GetPosition().y-120-5, 150, 120);
			m_pTabPlayerList->SetChatControl(m_pInputEdit);
			m_pInputEdit->SetTabHandler(m_pTabPlayerList);
		}
	}

	MLabel* pLabelToTeam = (MLabel*)m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATMODE_TOTEAM);
	if (pLabelToTeam) pLabelToTeam->Show(false);
	MLabel* pLabelToAll = (MLabel*)m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATMODE_TOALL);
	if (pLabelToAll) pLabelToAll->Show(false);

	return true;
}

void ZCombatChat::Destroy()
{
	if (m_pTabPlayerList) 
	{
		delete m_pTabPlayerList;
		m_pTabPlayerList = NULL;
	}
	if (m_pInputEdit)
	{
		m_pInputEdit->SetListener(NULL);
	}
	m_pChattingOutput = NULL;
}

void ZCombatChat::Update()
{
	UpdateChattingBox();
	if ((m_pInputEdit) && (m_bChatInputVisible))
	{
		if (!m_pInputEdit->IsFocus())
		{
			if (m_pTabPlayerList && !m_pTabPlayerList->IsFocus())
				EnableInput(false);
		}
	}
}

void ZCombatChat::UpdateChattingBox()
{
	if (m_pChattingOutput == NULL) return;

	if (m_pChattingOutput->GetLineCount() > 0)
	{
		unsigned long int nNowTime = timeGetTime();

#define CHAT_DELAY_TIME	5000
		if ((nNowTime - m_nLastChattingMsgTime) > CHAT_DELAY_TIME)
		{
			m_pChattingOutput->DeleteFirstLine();
			m_nLastChattingMsgTime = nNowTime;
		}
	}
}


void ZCombatChat::EnableInput(bool bEnable, bool bToTeam)
{
	extern bool g_bNewChat;
	if (g_bNewChat)
	{
		bEnable = false;
		bToTeam = false;
	}

	MLabel* pLabelToTeam = (MLabel*)m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATMODE_TOTEAM);
	MLabel* pLabelToAll = (MLabel*)m_pIDLResource->FindWidget(ZIITEM_COMBAT_CHATMODE_TOALL);
	if (bEnable == true) {
		SetTeamChat(bToTeam);
		if (bToTeam) {
			if (pLabelToTeam!=NULL) pLabelToTeam->Show(true);
			if (pLabelToAll!=NULL) pLabelToAll->Show(false);
		} else {
			if (pLabelToTeam!=NULL) pLabelToTeam->Show(false);
			if (pLabelToAll!=NULL) pLabelToAll->Show(true);
		}
	} else {
		if (pLabelToTeam!=NULL) pLabelToTeam->Show(false);
		if (pLabelToAll!=NULL) pLabelToAll->Show(false);
	}

	if (m_pInputEdit)
	{
		m_pInputEdit->Show(bEnable);
	}

	if (bEnable)
	{
		if (m_pInputEdit)
		{
			if (!m_pInputEdit->IsFocus()) m_pInputEdit->SetFocus();
		}
	}
	else
	{
//		ZApplication::GetGameInterface()->SetFocus();
	}

	m_bChatInputVisible = bEnable;

	if (ZApplication::GetGame()->m_pMyCharacter == NULL) return;

	if(bEnable)
	{
		ZPostPeerChatIcon(true);
	}else
	{
		ZPostPeerChatIcon(false);
	}
}

extern bool g_bNewChat;

void ZCombatChat::OutputChatMsg(const char* szMsg)
{
	if (g_bNewChat)
	{
		g_Chat.OutputChatMsg(szMsg);
		return;
	}

	if (m_pChattingOutput == NULL) return;

	if (m_pChattingOutput->GetLineCount() == 0)
		for (int i = 0; i < (MAX_CHAT_OUTPUT_LINE-1); i++) m_pChattingOutput->AddText("");
	m_pChattingOutput->AddText(szMsg);

	ProcessChatMsg();
}

void ZCombatChat::OutputChatMsg(MCOLOR color, const char* szMsg)
{
	if (g_bNewChat)
	{
		g_Chat.OutputChatMsg(szMsg, *PDWORD(&color));
		return;
	}

	if (m_pChattingOutput == NULL) return;

	if (m_pChattingOutput->GetLineCount() == 0)
		for (int i = 0; i < (MAX_CHAT_OUTPUT_LINE-1); i++) m_pChattingOutput->AddText("");
	m_pChattingOutput->AddText(szMsg, color);

	ProcessChatMsg();
}

void ZCombatChat::ProcessChatMsg()
{
	while ((m_pChattingOutput->GetLineCount() > MAX_CHAT_OUTPUT_LINE))
	{
		m_pChattingOutput->DeleteFirstLine();
	}

	
	if (m_pChattingOutput->GetLineCount() >= 1)
	{
		m_nLastChattingMsgTime = timeGetTime();
	}
}


void ZCombatChat::OnDraw(MDrawContext* pDC)
{
	if (m_pInputEdit)
	{
		if (m_pInputEdit->IsVisible())
		{
			pDC->SetColor(0xFF, 0xFF, 0xFF, 50);
			pDC->FillRectangle(m_pInputEdit->GetScreenRect());
		}
	}
}

void ZCombatChat::SetFont( MFont* pFont)
{
	m_pChattingOutput->SetFont( pFont);
}


void ZCombatChat::ShowOutput(bool bShow)
{
	if (m_pChattingOutput) m_pChattingOutput->Show(bShow);
	m_bShowOutput = bShow;
}