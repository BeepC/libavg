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

#ifndef _Event_H_
#define _Event_H_

#include "../api.h"
#include <functional>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#undef _POSIX_C_SOURCE

namespace avg {

class InputDevice;
typedef boost::shared_ptr<class InputDevice> InputDevicePtr;
typedef boost::weak_ptr<class InputDevice> InputDeviceWeakPtr;

class AVG_API Event {
    public:
        enum Type {
            // XXX: Hack to make sure this enum can't be passed to Node.subscribe.
            KEY_UP = 10000,
            KEY_DOWN,
            CURSOR_MOTION,
            CURSOR_UP,
            CURSOR_DOWN,
            CURSOR_OVER,  
            CURSOR_OUT,
            CUSTOM_EVENT,
            QUIT,
            UNKNOWN
        };
        enum Source {MOUSE=1, TOUCH=2, TRACK=4, TANGIBLE=8, PEN=16, CUSTOM=32, NONE=64};
    
        Event(Type type, Source source=NONE, int when=-1);
        Event(const Event& e);
        virtual ~Event();
        
        long long getWhen() const;
        Type getType() const;
        Event::Source getSource() const;
        InputDevicePtr getInputDevice() const;
        void setInputDevice(InputDevicePtr pInputDevice);
        bool hasInputDevice() const;
        const std::string& getInputDeviceName() const;
        
        std::string typeStr() const;
        static std::string typeStr(Event::Type type);

        virtual void trace();

        friend struct isEventAfter;
        
    protected:
        Type m_Type;

    private:
        long long m_When;
        int m_Counter;
        Source m_Source;

        InputDeviceWeakPtr m_pInputDevice;
        static int s_CurCounter;
};

typedef boost::shared_ptr<class Event> EventPtr;

// Functor to compare two EventPtrs chronologically
struct isEventAfter:std::binary_function<EventPtr, EventPtr, bool> {
    bool operator()(const EventPtr & x, const EventPtr & y) const {
        if (x->getWhen() == y->getWhen()) {
            return x->m_Counter > y->m_Counter;
        }
        return x->getWhen() > y->getWhen();
    }
};

}
#endif
