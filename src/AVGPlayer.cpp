//
// $Id$
//

#include "AVGPlayer.h"
#include "AVGAVGNode.h"
#include "AVGImage.h"
#include "AVGVideo.h"
#include "AVGWords.h"
#include "AVGExcl.h"
#include "AVGEvent.h"
#include "AVGMouseEvent.h"
#include "AVGKeyEvent.h"
#include "AVGWindowEvent.h"
#include "AVGException.h"
#include "AVGRegion.h"
#include "AVGDFBDisplayEngine.h"
#include "AVGLogger.h"
#include "AVGConradRelais.h"
#include "XMLHelper.h"

#include "acIJSContextPublisher.h"

#include <paintlib/plstdpch.h>
#include <paintlib/plexcept.h>
#include <paintlib/pldebug.h>
#include <paintlib/plpoint.h>
#include <paintlib/plpixel32.h>

#include <libxml/xmlmemory.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <pthread.h>

#include "nsMemory.h"

#ifdef XPCOM_GLUE
#include "nsXPCOMGlue.h"
#endif

#include <xpcom/nsComponentManagerUtils.h>

using namespace std;


AVGPlayer::AVGPlayer()
    : m_pRootNode (0),
      m_pDisplayEngine(0),
      m_pFramerateManager(0),
      m_pDebugDest(0)
{
#ifdef XPCOM_GLUE
    XPCOMGlueStartup("XPCOMComponentGlue");
#endif
    NS_INIT_ISUPPORTS();
    nsresult myErr;
    nsCOMPtr<acIJSContextPublisher> myJSContextPublisher =
        do_CreateInstance("@artcom.com/jscontextpublisher;1", &myErr);
    if (NS_FAILED(myErr)) {
        AVG_TRACE(DEBUG_ERROR, 
              "Could not obtain reference to js context. Was xpshell used to start AVGPlayer?");
        exit(-1);
    }
    myJSContextPublisher->GetContext((PRInt32*) &m_pJSContext);
    AVGLogger::get()->setDestination(&cerr);
}

AVGPlayer::~AVGPlayer()
{
#ifdef XPCOM_GLUE
    XPCOMGlueShutdown();
#endif
    if (m_pDebugDest) {
        delete m_pDebugDest;
    }

}

NS_IMPL_ISUPPORTS1_CI(AVGPlayer, IAVGPlayer);

NS_IMETHODIMP
AVGPlayer::LoadFile(const char * fileName, 
        PRBool * pResult)
{
    AVG_TRACE(DEBUG_MEMORY, 
          std::string("AVGPlayer::LoadFile(") + fileName + ")");
    try {
        loadFile (fileName);
        *pResult = true;
    } catch  (AVGException& ex) {
        AVG_TRACE(DEBUG_ERROR, ex.GetStr());
        *pResult = false;
    } catch (PLTextException& ex) {
        AVG_TRACE(DEBUG_ERROR, (const char*)ex);
        *pResult = false;
    }
    return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::Play(float framerate)
{
	play(framerate);
	return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::Stop()
{
	stop();
	return NS_OK;
}

NS_IMETHODIMP 
AVGPlayer::GetElementByID(const char *id, IAVGNode **_retval)
{
    AVGNode * pNode = getElementByID(id);
    if (!pNode) {
        AVG_TRACE(DEBUG_ERROR, "getElementByID(" << id << ") failed");
    }
    *_retval = getElementByID(id);
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP 
AVGPlayer::GetRootNode(IAVGNode **_retval)
{
    NS_IF_ADDREF(m_pRootNode);
    *_retval = m_pRootNode;
    return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::SetInterval(PRInt32 time, const char * code, PRInt32 * pResult)
{
    AVGTimeout *t = new AVGTimeout(time, code, true, m_pJSContext);
    m_NewTimeouts.push_back(t);
    *pResult = t->GetID();
	return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::SetTimeout(PRInt32 time, const char * code, PRInt32 * pResult)
{
    AVGTimeout *t = new AVGTimeout(time, code, false, m_pJSContext);
    m_NewTimeouts.push_back(t);
    *pResult = t->GetID();
	return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::ClearInterval(PRInt32 id, PRBool * pResult)
{
    vector<AVGTimeout*>::iterator it;
    for (it=m_PendingTimeouts.begin(); it!=m_PendingTimeouts.end(); it++) {
        if (id == (*it)->GetID()) {
            delete *it;
            m_PendingTimeouts.erase(it);
            *pResult = true;
            return NS_OK;
        }
    }

    *pResult = false;
    return NS_OK;
}

NS_IMETHODIMP 
AVGPlayer::GetCurEvent(IAVGEvent **_retval)
{
    NS_IF_ADDREF(m_pCurEvent);
    *_retval = m_pCurEvent;
    return NS_OK;
}

NS_IMETHODIMP 
AVGPlayer::SetDebugOutput(PRInt32 flags)
{
    AVGLogger::get()->setCategories(flags);
    return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::SetDebugOutputFile(const char *name)
{
    if (m_pDebugDest) {
        delete m_pDebugDest;
    }
    m_pDebugDest = new ofstream(name, ios::out | ios::app);
    if (!*m_pDebugDest) {
        AVG_TRACE(DEBUG_ERROR, "Could not open " << name << " as log destination.");
    } else {
        AVGLogger::get()->setDestination(m_pDebugDest);
        AVG_TRACE(DEBUG_ERROR, "Logging started ");
    }
}
    
NS_IMETHODIMP
AVGPlayer::GetErrStr(char ** ppszResult)
{
	strcpy (*ppszResult, "kaputt");
	return NS_OK;
}

NS_IMETHODIMP
AVGPlayer::GetErrCode(PRInt32 * pResult)
{
	*pResult = 0;
	return NS_OK;
}

NS_IMETHODIMP 
AVGPlayer::CreateRelais(PRInt16 port, IAVGConradRelais **_retval)
{
    nsresult rv;
    AVGConradRelais* pRelais;
    rv = nsComponentManager::CreateInstance ("@c-base.org/avgconradrelais;1", 0,
            NS_GET_IID(IAVGConradRelais), (void**)&pRelais);
    if (NS_FAILED(rv)) {
        AVG_TRACE(DEBUG_ERROR, "CreateRelais failed: " << rv);
    }
    pRelais->init(port);
    m_pRelais.push_back(pRelais);
    *_retval = pRelais;
    return NS_OK;
}

void AVGPlayer::loadFile (const std::string& filename)
{
    jsgc();
    PLASSERT (!m_pRootNode);
    if (!m_pDisplayEngine) {
        m_pDisplayEngine = new AVGDFBDisplayEngine ();
        m_pEventSource = dynamic_cast<AVGDFBDisplayEngine *>(m_pDisplayEngine);
    }

    xmlPedanticParserDefault(1);
    xmlDoValidityCheckingDefaultValue =1;
    
    xmlDocPtr doc;
    doc = xmlParseFile(filename.c_str());
    if (!doc) {
        throw (AVGException(AVG_ERR_XML_PARSE, 
                string("Error parsing xml document ")+filename));
    }
    m_CurDirName = filename.substr(0,filename.rfind('/')+1);

    m_pRootNode = dynamic_cast<AVGAVGNode*>
            (createNodeFromXml(xmlDocGetRootElement(doc), 0));
    PLRect rc = m_pRootNode->getRelViewport();
    xmlFreeDoc(doc);
}

void AVGPlayer::play (double framerate)
{
    DFBResult err;
    if (!m_pRootNode) {
        AVG_TRACE(DEBUG_ERROR, "play called, but no xml file loaded.");
    }
    PLASSERT (m_pRootNode);
//    setRealtimePriority();

    m_EventDispatcher.addSource(m_pEventSource);
    m_EventDispatcher.addSink(&m_EventDumper);
    m_EventDispatcher.addSink(this);
    
    m_pFramerateManager = new AVGFramerateManager;
    m_pFramerateManager->SetRate(framerate);
    m_bStopping = false;
    m_pDisplayEngine->render(m_pRootNode, m_pFramerateManager, true);
    while (!m_bStopping) {
        doFrame();
    }
    // Kill all timeouts.
    vector<AVGTimeout*>::iterator it;
    for (it=m_PendingTimeouts.begin(); it!=m_PendingTimeouts.end(); it++) {
         delete *it;
    }
    m_PendingTimeouts.clear();

    NS_IF_RELEASE(m_pRootNode);
    m_pRootNode = 0;
    delete m_pFramerateManager;
    m_IDMap.clear();
}

void AVGPlayer::stop ()
{
    m_bStopping = true;
}

void AVGPlayer::doFrame ()
{
    handleTimers();
    
    m_EventDispatcher.dispatch();
    if (!m_bStopping) {
        m_pDisplayEngine->render(m_pRootNode, m_pFramerateManager, false);
    }
    for (int i=0; i<m_pRelais.size(); i++) {
        m_pRelais[i]->send();
    }
    
    jsgc();
}

void AVGPlayer::jsgc()
{
    JS_GC(m_pJSContext);
}

void AVGPlayer::teardownDFB()
{
    m_pDisplayEngine->teardown();
    delete m_pDisplayEngine;
    m_pDisplayEngine = 0;
}


AVGNode * AVGPlayer::getElementByID (const std::string id)
{
    if (m_IDMap.find(id) != m_IDMap.end()) {
        return m_IDMap.find(id)->second;
    } else {
        return 0;
    }
}

AVGAVGNode * AVGPlayer::getRootNode ()
{
    return m_pRootNode;
}

double AVGPlayer::getFramerate ()
{
    return m_pFramerateManager->GetRate();
}

void AVGPlayer::addEvent (int time, AVGEvent * event)
{
}

void AVGPlayer::setRealtimePriority()
{
    sched_param myParam;

    myParam.sched_priority = sched_get_priority_max (SCHED_FIFO);
    int myRetVal = pthread_setschedparam (pthread_self(), 
		    SCHED_FIFO, &myParam);
    if (myRetVal == 0) {
        AVG_TRACE(DEBUG_PROFILE, "AVGPlayer running with realtime priority.");
    } else {
        AVG_TRACE(DEBUG_PROFILE, "Setting realtime priority failed.");
    }
}

AVGNode * AVGPlayer::createNodeFromXml (const xmlNodePtr xmlNode, 
        AVGContainer * pParent)
{
    const xmlChar * nodeType = xmlNode->name;
    AVGNode * curNode = 0;
    if (!xmlStrcmp (nodeType, (const xmlChar *)"avg")) {
        // create node itself.
        string id;
        int x,y,z;
        int width, height;
        double opacity;
        getVisibleNodeAttrs(xmlNode, &id, &x, &y, &z, &width, &height, &opacity);
        curNode = AVGAVGNode::create();
        // Fullscreen, bpp and debug handling only for topmost node.
        if (!pParent) {
            bool isFullscreen = getDefaultedBoolAttr 
                    (xmlNode, (const xmlChar *)"fullscreen", false);
            int bpp = getDefaultedIntAttr
                    (xmlNode, (const xmlChar *)"bpp", 16);
            m_pDisplayEngine->init(width, height, isFullscreen, bpp);
        }
        curNode->init(id, m_pDisplayEngine, pParent, this);
        curNode->initVisible(x, y, z, width, height, opacity);
        initEventHandlers(curNode, xmlNode);
    } else if (!xmlStrcmp (nodeType, (const xmlChar *)"image")) {
        string id;
        int x,y,z;
        int width, height;
        double opacity;
        getVisibleNodeAttrs(xmlNode, &id, &x, &y, &z, &width, &height, &opacity);
        string filename = m_CurDirName + 
                getRequiredStringAttr(xmlNode, (const xmlChar *)"href");

        AVGImage * pImage = AVGImage::create();
        curNode = pImage;
        pImage->init(id, x, y, z, width, height, opacity, 
                filename, m_pDisplayEngine, pParent, this);
        initEventHandlers(curNode, xmlNode);
    } else if (!xmlStrcmp (nodeType, (const xmlChar *)"video")) {
        string id;
        int x,y,z;
        int width, height;
        double opacity;
        getVisibleNodeAttrs(xmlNode, &id, &x, &y, &z, &width, &height, &opacity);
        string filename = m_CurDirName + 
                getRequiredStringAttr(xmlNode, (const xmlChar *)"href");
        bool bLoop = getDefaultedBoolAttr(xmlNode, (const xmlChar *)"loop", false); 
        bool bOverlay = getDefaultedBoolAttr(xmlNode, (const xmlChar *)"overlay", false); 
        AVGVideo * pVideo = AVGVideo::create();
        curNode = pVideo;
        pVideo->init(id, x, y, z, width, height, opacity, 
                filename, bLoop, bOverlay, m_pDisplayEngine, pParent, this);
        initEventHandlers(curNode, xmlNode);
    } else if (!xmlStrcmp (nodeType, (const xmlChar *)"words")) {
        string id;
        int x,y,z;
        int width, height;  // Bogus
        double opacity;
        getVisibleNodeAttrs(xmlNode, &id, &x, &y, &z, &width, &height, &opacity);
        string font = getDefaultedStringAttr(xmlNode, 
                (const xmlChar *)"font", "arial");
        string str = getRequiredStringAttr(xmlNode, (const xmlChar *)"text");
        string color = getDefaultedStringAttr(xmlNode, 
                (const xmlChar *)"color", "FFFFFF");
        int size = getDefaultedIntAttr(xmlNode, (const xmlChar *)"size", 15);
        AVGWords * pWords = AVGWords::create();
        curNode = pWords;
        pWords->init(id, x, y, z, opacity, size,
                font, str, color, m_pDisplayEngine, pParent, this);
        initEventHandlers(curNode, xmlNode);
    } else if (!xmlStrcmp (nodeType, (const xmlChar *)"excl")) {
        string id  = getDefaultedStringAttr (xmlNode, (const xmlChar *)"id", "");
        curNode = AVGExcl::create();
        curNode->init(id, m_pDisplayEngine, pParent, this);
        curNode->initVisible(0,0,1,10000,10000,1);
        initEventHandlers(curNode, xmlNode);
    } else if (!xmlStrcmp (nodeType, (const xmlChar *)"text") || 
               !xmlStrcmp (nodeType, (const xmlChar *)"comment")) {
        // Ignore whitespace & comments
        return 0;
    } else {
        throw (AVGException (AVG_ERR_XML_NODE_UNKNOWN, 
            string("Unknown node type ")+(const char *)nodeType+" encountered."));
    }
    // If this is a container, recurse into children
    AVGContainer * curContainer = dynamic_cast<AVGContainer*>(curNode);
    if (curContainer) {
        xmlNodePtr curXmlChild = xmlNode->xmlChildrenNode;
        while (curXmlChild) {
            AVGNode *curChild = createNodeFromXml (curXmlChild, curContainer);
            if (curChild) {
                curContainer->addChild(curChild);
            }
            curXmlChild = curXmlChild->next;
        }
    }
    const string& ID = curNode->getID();
    if (ID != "") {
        if (m_IDMap.find(ID) != m_IDMap.end()) {
            throw (AVGException (AVG_ERR_XML_DUPLICATE_ID,
                string("Error: duplicate id ")+ID));
        }
        m_IDMap.insert(NodeIDMap::value_type(ID, curNode));
    }
    return curNode;
}

void AVGPlayer::getVisibleNodeAttrs (const xmlNodePtr xmlNode, 
        string * pid, int * px, int * py, int * pz,
        int * pWidth, int * pHeight, double * pOpacity)
{
    *pid = getDefaultedStringAttr (xmlNode, (const xmlChar *)"id", "");
    *px = getDefaultedIntAttr (xmlNode, (const xmlChar *)"x", 0);
    *py = getDefaultedIntAttr (xmlNode, (const xmlChar *)"y", 0);
    *pz = getDefaultedIntAttr (xmlNode, (const xmlChar *)"z", 0);
    *pWidth = getDefaultedIntAttr (xmlNode, (const xmlChar *)"width", 0);
    *pHeight = getDefaultedIntAttr (xmlNode, (const xmlChar *)"height", 0);
    *pOpacity = getDefaultedDoubleAttr (xmlNode, (const xmlChar *)"opacity", 1.0);
}

void AVGPlayer::initEventHandlers (AVGNode * pAVGNode, const xmlNodePtr xmlNode)
{
    string MouseMoveHandler = getDefaultedStringAttr 
            (xmlNode, (const xmlChar *)"onmousemove", "");
    string MouseButtonUpHandler = getDefaultedStringAttr 
            (xmlNode, (const xmlChar *)"onmouseup", "");
    string MouseButtonDownHandler = getDefaultedStringAttr 
            (xmlNode, (const xmlChar *)"onmousedown", "");
    string MouseOverHandler = getDefaultedStringAttr 
            (xmlNode, (const xmlChar *)"onmouseover", "");
    string MouseOutHandler = getDefaultedStringAttr 
            (xmlNode, (const xmlChar *)"onmouseout", "");
    pAVGNode->InitEventHandlers
            (MouseMoveHandler, MouseButtonUpHandler, MouseButtonDownHandler, 
             MouseOverHandler, MouseOutHandler);
}


void AVGPlayer::handleTimers()
{
    vector<AVGTimeout *>::iterator it;
    it = m_PendingTimeouts.begin();
    while (it != m_PendingTimeouts.end() && (*it)->IsReady() && !m_bStopping)
    {
        (*it)->Fire(m_pJSContext);
        if (!(*it)->IsInterval()) {
            delete *it;
            it = m_PendingTimeouts.erase(it);
        } else {
            AVGTimeout* pTempTimeout = *it;
            it = m_PendingTimeouts.erase(it);
            addTimeout(pTempTimeout);
        }
    }
    for (it = m_NewTimeouts.begin(); it != m_NewTimeouts.end(); ++it) {
        addTimeout(*it);
    }
    m_NewTimeouts.clear();
}

bool AVGPlayer::handleEvent(AVGEvent * pEvent)
{
    m_pCurEvent = pEvent;
    switch (pEvent->getType()) {
        case AVGEvent::MOUSE_MOTION:
        case AVGEvent::MOUSE_BUTTON_UP:
        case AVGEvent::MOUSE_BUTTON_DOWN:
            {
                AVGMouseEvent * pMouseEvent = dynamic_cast<AVGMouseEvent*>(pEvent);
                PLPoint pos(pMouseEvent->getXPosition(), pMouseEvent->getYPosition());
                AVGNode * pNode = m_pRootNode->getElementByPos(pos);            
                if (pNode) {
                    pNode->handleMouseEvent(pMouseEvent, m_pJSContext);
                }
            }
            break;
        case AVGEvent::KEY_DOWN:
            {
                AVGKeyEvent * pKeyEvent = dynamic_cast<AVGKeyEvent*>(pEvent);
                if (pKeyEvent->getKeyCode() == 27) {
                    m_bStopping = true;
                }
            }
            break;
        case AVGEvent::QUIT:
            m_bStopping = true;
            break;
    }
    // Don't pass on any events.
    return true; 
}

/*
void AVGPlayer::handleEvents()
{
    IDirectFBEventBuffer * pEventBuffer = 
            dynamic_cast<AVGDFBDisplayEngine *>(m_pDisplayEngine)->getEventBuffer();
    DFBEvent dfbEvent;

    while (pEventBuffer->HasEvent(pEventBuffer) == DFB_OK && !m_bStopping) {
        pEventBuffer->GetEvent (pEventBuffer, &dfbEvent);
//        dumpDFBEvent (dfbEvent);
        if (dfbEvent.clazz == DFEC_WINDOW) {
            DFBWindowEvent* pdfbWEvent = &(dfbEvent.window);

            AVGEvent * pCPPEvent = createCurEvent();
            bool bOK = pCPPEvent->init(*pdfbWEvent);
            if (bOK) {
                pCPPEvent->dump();
                int EventType;
           
                int b;
                m_CurEvent->IsMouseEvent(&b);
                if (b) {
                    handleMouseEvent(pCPPEvent);
                }
                if (pCPPEvent->getType() == AVGEvent::KEYDOWN && pCPPEvent->getKeySym()==27) {
                    m_bStopping = true;
                }
                NS_IF_RELEASE(pCPPEvent);
            }
        } else {
            AVG_TRACE(DEBUG_ERROR, "Unexpected event received.");
        }
    }
    
}


AVGEvent* AVGPlayer::createCurEvent()
{
    nsresult rv;
    nsCOMPtr<IAVGEvent> pXPEvent = do_CreateInstance("@c-base.org/avgevent;1", &rv);
    PLASSERT(!NS_FAILED(rv));
    m_CurEvent = pXPEvent;
    NS_IF_ADDREF((IAVGEvent*)m_CurEvent);
    return dynamic_cast<AVGEvent*>(m_CurEvent);
}

void AVGPlayer::handleMouseEvent (AVGEvent* pEvent)
{
    PLPoint pos;
    pEvent->GetXPos(&pos.x);
    pEvent->GetYPos(&pos.y);
    int ButtonState;
    pEvent->GetMouseButtonState(&ButtonState);
    AVGNode * pNode = m_pRootNode->getElementByPos(pos);
    if (pNode) {
        pNode->handleEvent(pEvent, m_pJSContext);
    }
    if (pNode != m_pLastMouseNode) {
        if (pNode) {
            AVGEvent * pEvent = createCurEvent();
            pEvent->init(AVGEvent::MOUSEOVER, pos, ButtonState);
            pEvent->dump();
            pNode->handleEvent(pEvent, m_pJSContext);
        }
        if (m_pLastMouseNode) {
            AVGEvent * pEvent = createCurEvent();
            pEvent->init(AVGEvent::MOUSEOUT, pos, ButtonState);
            pEvent->dump();
            m_pLastMouseNode->handleEvent(pEvent, m_pJSContext);
        }

        m_pLastMouseNode = pNode;
    }
}
*/

int AVGPlayer::addTimeout(AVGTimeout* timeout)
{
    vector<AVGTimeout*>::iterator it=m_PendingTimeouts.begin();
    while (it != m_PendingTimeouts.end() && (**it)<*timeout) {
        it++;
    }
    m_PendingTimeouts.insert(it, timeout);
    return timeout->GetID();
}

/*
void AVGPlayer::dumpDFBEvent (const DFBEvent& dfbEvent)
{
    // TODO: Convert to AVG_TRACE.
    if (dfbEvent.clazz == DFEC_WINDOW && dfbEvent.window.type == DWET_MOTION) {
        // Don't dump mouse move events
        return;
    }
    cerr << "Event: ";
    cerr << "class=";

    switch (dfbEvent.clazz) {
        case DFEC_NONE:
            cerr << "none";
            break;
        case DFEC_INPUT:
            cerr << "input";
            break;
        case DFEC_WINDOW:
            {            
                cerr << "window, ";
                DFBWindowEvent WEvent = dfbEvent.window;
                cerr << "  type = ";
                switch (WEvent.type) {
                    case DWET_POSITION:
                        cerr << " position";
                        break;
                    case DWET_SIZE:
                        cerr << " size";
                        break;
                    case DWET_POSITION | DWET_SIZE:
                        cerr << " position & size";
                        break;
                    case DWET_CLOSE:
                        cerr << " close";
                        break;
                    case DWET_DESTROYED:
                        cerr << " destroyed";
                        break;
                    case DWET_GOTFOCUS:
                        cerr << " got focus";
                        break;
                    case DWET_LOSTFOCUS:
                        cerr << " lost focus";
                        break;
                    case DWET_KEYDOWN:
                        cerr << " key down";
                        break;
                    case DWET_KEYUP:
                        cerr << " key up";
                        break;
                    case DWET_BUTTONDOWN:
                        cerr << " button down";
                        break;
                    case DWET_MOTION:
                        cerr << " motion"; 
                        break;
                    case DWET_ENTER:
                        cerr << " enter";
                        break;
                    case DWET_LEAVE:
                        cerr << " leave";
                        break;
                    case DWET_WHEEL:
                        cerr << " wheel";
                        break;
                }
            }
            break;
        case DFEC_USER:
            cerr << "user";
            break;
    }
    cerr << endl;
    
}
*/
