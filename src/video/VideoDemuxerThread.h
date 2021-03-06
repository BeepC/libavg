//
//  libavg - Media Playback Engine. 
//  Copyright (C) 2003-2014 Ulrich von Zadow
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Current versions can be found at www.libavg.de
//

#ifndef _VideoDemuxerThread_H_
#define _VideoDemuxerThread_H_

#include "../api.h"
#include "VideoMsg.h"
#include "WrapFFMpeg.h"

#include "../base/WorkerThread.h"
#include "../base/Command.h"

#include <boost/thread.hpp>

namespace avg {

class FFMpegDemuxer;
typedef boost::shared_ptr<FFMpegDemuxer> FFMpegDemuxerPtr;

class AVG_API VideoDemuxerThread: public WorkerThread<VideoDemuxerThread> {
    public:
        VideoDemuxerThread(CQueue& cmdQ, AVFormatContext* pFormatContext, 
                const std::map<int, VideoMsgQueuePtr>& packetQs);
        virtual ~VideoDemuxerThread();
        bool init();
        bool work();

        void seek(int seqNum, float DestTime);
        void close();

    private:
        void onStreamEOF(int streamIndex);
        void clearQueue(VideoMsgQueuePtr pPacketQ);

        std::map<int, VideoMsgQueuePtr> m_PacketQs;
        std::map<int, bool> m_PacketQEOFMap;
        bool m_bEOF;
        AVFormatContext* m_pFormatContext;
        FFMpegDemuxerPtr m_pDemuxer;
};

}
#endif 

