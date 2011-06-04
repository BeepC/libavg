#!/usr/bin/env python
# -*- coding: utf-8 -*-
# libavg - Media Playback Engine.
# Copyright (C) 2003-2008 Ulrich von Zadow
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Current versions can be found at www.libavg.de
#

from libavg import *

g_player = avg.Player.get()

class TouchApp(AVGApp):
    multitouch = True

    def init(self):
        self._parentNode.connectEventHandler(avg.CURSORDOWN, avg.TOUCH, self, 
                self.__onDown)
    
    def __onDown(self, event):
        print "down: ", event.contact.id
        event.contact.connectListener(self.__onContact)

    def __onContact(self, event):
        if event.type == CURSORMOTION:
            print "motion: ", event.cursorid, event.pos
        elif event.type == CURSORUP:
            print "up: ", event.cursorid, event.pos
        else:
            print event.type, event.cursorid


TouchApp.start(resolution=(800,600))

