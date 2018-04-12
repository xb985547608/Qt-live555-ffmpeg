/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2011 Live Networks, Inc.  All rights reserved.
// A simplified version of "MPEG4VideoStreamFramer" that takes only complete,
// discrete frames (rather than an arbitrary byte stream) as input.
// This avoids the parsing and data copying overhead of the full
// "MPEG4VideoStreamFramer".
// C++ header

#ifndef _AVI_VIDEO_DISCRETE_FRAMER_HH
#define _AVI_VIDEO_DISCRETE_FRAMER_HH

#ifndef _MPEG4_VIDEO_STREAM_FRAMER_HH
#include "MPEG4VideoStreamDiscreteFramer.hh"
#endif

class AVIVideoDiscreteFramer: public MPEG4VideoStreamDiscreteFramer {
public:
  static AVIVideoDiscreteFramer*
  createNew(UsageEnvironment& env, FramedSource* inputSource);

protected:
  AVIVideoDiscreteFramer(UsageEnvironment& env,
				 FramedSource* inputSource);
      // called only by createNew()
  virtual ~AVIVideoDiscreteFramer();

protected:
  // redefined virtual functions:
  virtual void doGetNextFrame();

protected:
  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame1(unsigned frameSize,
                          unsigned numTruncatedBytes,
                          struct timeval presentationTime,
                          unsigned durationInMicroseconds);


};

#endif
