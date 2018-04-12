// Copyright (c) 2011 UPTTOP.COM, All rights reserved.
// class AACAudioStreamFramer, for aac audio streaming
// Implementation

#include "AACAudioStreamFramer.hh"
#include "GroupsockHelper.hh"

AACAudioStreamFramer*
AACAudioStreamFramer::createNew(UsageEnvironment& env,
								FramedSource* inputSource, unsigned char streamCode)
{
  // Need to add source type checking here???  #####
	return new AACAudioStreamFramer(env, inputSource);
}

AACAudioStreamFramer
::AACAudioStreamFramer(UsageEnvironment& env,
					   FramedSource* inputSource)
					   : FramedFilter(env, inputSource)
{
	fPresentationTime.tv_sec = fPresentationTime.tv_usec = 0;
}

AACAudioStreamFramer::~AACAudioStreamFramer() {
}

void AACAudioStreamFramer::doGetNextFrame() {
  // Arrange to read data (which should be a complete MPEG-4 video frame)
  // from our data source, directly into the client's input buffer.
  // After reading this, we'll do some parsing on the frame.
	fInputSource->getNextFrame(fTo, fMaxSize,
                             afterGettingFrame, this,
                             FramedSource::handleClosure, this);
}

void AACAudioStreamFramer::flushInput() {
	fPresentationTime.tv_sec = 0;
	fPresentationTime.tv_usec = 0;
}

void AACAudioStreamFramer
::afterGettingFrame(void* clientData, unsigned frameSize,
                    unsigned numTruncatedBytes,
                    struct timeval presentationTime,
                    unsigned durationInMicroseconds) {
  	AACAudioStreamFramer* source = (AACAudioStreamFramer*)clientData;
  	source->afterGettingFrame1(frameSize, numTruncatedBytes,
                             presentationTime, durationInMicroseconds);
}

void AACAudioStreamFramer
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                     struct timeval presentationTime,
                     unsigned durationInMicroseconds) {

  // Set the 'presentation time':
  	if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
	    // This is the first frame, so use the current time:
	    gettimeofday(&fPresentationTime, NULL);
  	} else {
    // Increment by the play time of the previous frame (20 ms)
	    unsigned uSeconds	= fPresentationTime.tv_usec + durationInMicroseconds;
	    fPresentationTime.tv_sec += uSeconds/1000000;
	    fPresentationTime.tv_usec = uSeconds%1000000;
  	}
  
	// Complete delivery to the client:
	fFrameSize = frameSize;
	fNumTruncatedBytes = numTruncatedBytes;
	fDurationInMicroseconds = durationInMicroseconds;
	afterGetting(this);
}


