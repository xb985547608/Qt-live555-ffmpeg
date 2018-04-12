// Copyright (c) 2011 UPTTOP.COM, All rights reserved.
// class AACAudioStreamFramer, for aac audio streaming
// C++ header

#ifndef _AAC_AUDIO_STREAM_FRAMER_HH
#define _AAC_AUDIO_STREAM_FRAMER_HH

#include "FramedFilter.hh"


class AACAudioStreamFramer: public FramedFilter {
public:
  static AACAudioStreamFramer*
  createNew(UsageEnvironment& env, FramedSource* inputSource, unsigned char streamCode = 0x80);

  void flushInput(); // called if there is a discontinuity (seeking) in the input
  
protected:
  AACAudioStreamFramer(UsageEnvironment& env,
				 FramedSource* inputSource);
      // called only by createNew()
  virtual ~AACAudioStreamFramer();

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
